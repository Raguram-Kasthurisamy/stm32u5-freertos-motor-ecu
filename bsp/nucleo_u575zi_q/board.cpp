// Author: Raguram Kasthurisamy

// Board support for NUCLEO-U575ZI-Q (STM32U575ZIT6Q: Cortex-M33 + FPU, 160 MHz, SMPS).
//
// Pin map used so far:
//   PC7   LD1 green user LED (heartbeat)
//   PA9   USART1_TX -> STLINK-V3E virtual COM port (default solder bridges)
//   PA10  USART1_RX <- STLINK-V3E virtual COM port
// Motor power stage and sensors: see motor_hw.cpp.
// Reserved for later phases: PD0/PD1 FDCAN1 RX/TX, PB7 LD2 blue, PG2 LD3 red (port G
// needs VDDIO2 enabled first), PC13 user button.
// DMA: GPDMA1 channel 0 = USART1_RX (circular, as a one-node linked list),
//      GPDMA1 channel 1 = USART1_TX (normal).
//
// Requires TrustZone disabled (option byte TZEN = 0, the factory default).

#include "bsp/board.hpp"

#include "bsp/hal.hpp"
#include "bsp/irq_priority.hpp"
#include "bsp/peripherals.hpp"
#include "bsp/fault.hpp"
#include "bsp/motor_hw.hpp"

#define BSP_CHECK(expr)                                                \
  do {                                                                 \
    if (!(expr)) {                                                     \
      bsp::fatal_error_hook("board init failed: " #expr, __FILE__, __LINE__); \
    }                                                                  \
  } while (0)

extern "C" char _smsp_stack[];  // linker script: lowest address of the main stack

// Defined in timing.cpp and motor_hw.cpp; internal to this BSP.
namespace bsp {
void init_cycle_counter();
namespace motor_hw {
void init();
}
}  // namespace bsp

namespace {

constexpr std::uint32_t kConsoleBaud = 115200;

UART_HandleTypeDef g_console_uart{};
DMA_HandleTypeDef g_console_dma_rx{};
DMA_HandleTypeDef g_console_dma_tx{};
// GPDMA has no plain circular mode: circular reception is a linked list whose only node
// points back at itself. The node is read by the DMA controller, so it must live in RAM.
DMA_NodeTypeDef g_console_rx_node{};
DMA_QListTypeDef g_console_rx_queue{};

bsp::ClockSource g_clock_source = bsp::ClockSource::kInternalFallback;
bsp::ResetCause g_reset_cause = bsp::ResetCause::kUnknown;

// Reads and clears the RCC reset flags. Several flags can be set at once (a power-on
// reset also sets PIN), so they are checked from most to least specific. The U5 has no
// separate power-on flag: power-on sets BORRSTF.
bsp::ResetCause capture_reset_cause() {
  const std::uint32_t csr = RCC->CSR;
  bsp::ResetCause cause = bsp::ResetCause::kUnknown;
  if ((csr & RCC_CSR_IWDGRSTF) != 0U) {
    cause = bsp::ResetCause::kIndependentWatchdog;
  } else if ((csr & RCC_CSR_WWDGRSTF) != 0U) {
    cause = bsp::ResetCause::kWindowWatchdog;
  } else if ((csr & RCC_CSR_LPWRRSTF) != 0U) {
    cause = bsp::ResetCause::kLowPower;
  } else if ((csr & RCC_CSR_SFTRSTF) != 0U) {
    cause = bsp::ResetCause::kSoftware;
  } else if ((csr & RCC_CSR_OBLRSTF) != 0U) {
    cause = bsp::ResetCause::kOptionByteLoad;
  } else if ((csr & RCC_CSR_BORRSTF) != 0U) {
    cause = bsp::ResetCause::kPowerOnOrBrownout;
  } else if ((csr & RCC_CSR_PINRSTF) != 0U) {
    cause = bsp::ResetCause::kPin;
  }
  RCC->CSR |= RCC_CSR_RMVF;
  return cause;
}

// Hardware stack-overflow detection for the main stack: the Cortex-M33 raises a
// UsageFault (STKOF) the moment SP drops below MSPLIM. FreeRTOS's ARMv8-M port does the
// same for every task by loading PSPLIM on each context switch.
void init_stack_limit_and_faults() {
  __set_MSPLIM(reinterpret_cast<std::uint32_t>(_smsp_stack));
  // Route faults to their own handlers instead of escalating everything to HardFault.
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk;
  // Integer division by zero silently returns 0 on Cortex-M unless trapped. Trap it.
  SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
}

void init_power() {
  __HAL_RCC_PWR_CLK_ENABLE();
  // "-Q" package: run the core regulator from the internal SMPS instead of the LDO.
  BSP_CHECK(HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) == HAL_OK);
  // Voltage range 1 is required above 110 MHz.
  BSP_CHECK(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) == HAL_OK);
  // Release the USB-C (UCPD) dead-battery pull-downs on the CC lines.
  HAL_PWREx_DisableUCPDDeadBattery();
}

// SYSCLK = 160 MHz: MSIS 4 MHz -> PLL1 (M=1, N=80: 320 MHz VCO) -> R=2. PLL1Q = 80 MHz feeds FDCAN.
// The board has no high-speed crystal, but it does have a 32.768 kHz LSE crystal. In MSI
// "PLL mode" the hardware continuously trims MSIS against the LSE, giving crystal-class
// accuracy (needed for CAN bit timing in Phase 3). If the LSE fails to start, the board
// still boots on the untrimmed MSI and reports it, instead of hanging.
void init_clocks() {
  HAL_PWR_EnableBkUpAccess();  // the LSE lives in the write-protected backup domain
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  RCC_OscInitTypeDef osc{};
  osc.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_LSE;
  osc.LSEState = RCC_LSE_ON;
  osc.MSIState = RCC_MSI_ON;
  osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  osc.MSIClockRange = RCC_MSIRANGE_4;      // MSIS = 4 MHz
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  osc.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;  // EPOD booster clock = 4 MHz
  osc.PLL.PLLM = 1;
  osc.PLL.PLLN = 80;
  osc.PLL.PLLP = 2;
  osc.PLL.PLLQ = 4;  // 80 MHz: FDCAN kernel clock
  osc.PLL.PLLR = 2;
  osc.PLL.PLLRGE = RCC_PLLVCIRANGE_0;      // PLL input between 4 and 8 MHz
  osc.PLL.PLLFRACN = 0;

  g_clock_source = bsp::ClockSource::kCrystalReferenced;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    osc.LSEState = RCC_LSE_OFF;
    g_clock_source = bsp::ClockSource::kInternalFallback;
    BSP_CHECK(HAL_RCC_OscConfig(&osc) == HAL_OK);
  }

  RCC_ClkInitTypeDef clk{};
  clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 |
                  RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;  // 160 MHz everywhere
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  clk.APB3CLKDivider = RCC_HCLK_DIV1;
  BSP_CHECK(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) == HAL_OK);  // 4 WS at 160 MHz

  if (g_clock_source == bsp::ClockSource::kCrystalReferenced) {
    (void)HAL_RCCEx_EnableMSIPLLModeSelection(RCC_MSISPLL_MODE_SEL);  // trim MSIS (not MSIK)
    HAL_RCCEx_EnableMSIPLLMode();
  }
}

// The instruction cache matters: at 160 MHz flash needs 4 wait states, and without the
// cache every instruction fetch that misses the prefetch buffer pays them.
void init_icache() { BSP_CHECK(HAL_ICACHE_Enable() == HAL_OK); }

void init_gpio() {
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitTypeDef led{};
  led.Pin = GPIO_PIN_7;
  led.Mode = GPIO_MODE_OUTPUT_PP;
  led.Pull = GPIO_NOPULL;
  led.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOC, &led);
}

void init_console_uart() {
  g_console_uart.Instance = USART1;
  g_console_uart.Init.BaudRate = kConsoleBaud;
  g_console_uart.Init.WordLength = UART_WORDLENGTH_8B;
  g_console_uart.Init.StopBits = UART_STOPBITS_1;
  g_console_uart.Init.Parity = UART_PARITY_NONE;
  g_console_uart.Init.Mode = UART_MODE_TX_RX;
  g_console_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  g_console_uart.Init.OverSampling = UART_OVERSAMPLING_16;
  g_console_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  g_console_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  g_console_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  // HAL_UART_Init waits for the peripheral using HAL_GetTick(): this is one of the
  // timeouts that would hang if any FreeRTOS object already existed (see main.cpp).
  BSP_CHECK(HAL_UART_Init(&g_console_uart) == HAL_OK);  // calls HAL_UART_MspInit below
  BSP_CHECK(HAL_UARTEx_DisableFifoMode(&g_console_uart) == HAL_OK);
}

void init_console_dma(UART_HandleTypeDef* huart) {
  // RX: one linked-list node, queue set to circular so the DMA never stops between bursts.
  DMA_NodeConfTypeDef node{};
  node.NodeType = DMA_GPDMA_LINEAR_NODE;
  node.Init.Request = GPDMA1_REQUEST_USART1_RX;
  node.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  node.Init.Direction = DMA_PERIPH_TO_MEMORY;
  node.Init.SrcInc = DMA_SINC_FIXED;
  node.Init.DestInc = DMA_DINC_INCREMENTED;
  node.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
  node.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
  node.Init.SrcBurstLength = 1;
  node.Init.DestBurstLength = 1;
  node.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
  node.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  node.Init.Mode = DMA_NORMAL;
  node.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
  node.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  node.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  // Buffer address and length are filled in by HAL_UARTEx_ReceiveToIdle_DMA().
  BSP_CHECK(HAL_DMAEx_List_BuildNode(&node, &g_console_rx_node) == HAL_OK);
  BSP_CHECK(HAL_DMAEx_List_InsertNode(&g_console_rx_queue, nullptr, &g_console_rx_node) == HAL_OK);
  BSP_CHECK(HAL_DMAEx_List_SetCircularMode(&g_console_rx_queue) == HAL_OK);

  g_console_dma_rx.Instance = GPDMA1_Channel0;
  g_console_dma_rx.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
  g_console_dma_rx.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
  g_console_dma_rx.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  g_console_dma_rx.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  g_console_dma_rx.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
  BSP_CHECK(HAL_DMAEx_List_Init(&g_console_dma_rx) == HAL_OK);
  BSP_CHECK(HAL_DMAEx_List_LinkQ(&g_console_dma_rx, &g_console_rx_queue) == HAL_OK);
  __HAL_LINKDMA(huart, hdmarx, g_console_dma_rx);
  BSP_CHECK(HAL_DMA_ConfigChannelAttributes(&g_console_dma_rx, DMA_CHANNEL_NPRIV) == HAL_OK);

  // TX: plain one-shot transfers started by the driver.
  g_console_dma_tx.Instance = GPDMA1_Channel1;
  g_console_dma_tx.Init.Request = GPDMA1_REQUEST_USART1_TX;
  g_console_dma_tx.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  g_console_dma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  g_console_dma_tx.Init.SrcInc = DMA_SINC_INCREMENTED;
  g_console_dma_tx.Init.DestInc = DMA_DINC_FIXED;
  g_console_dma_tx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
  g_console_dma_tx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
  g_console_dma_tx.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
  g_console_dma_tx.Init.SrcBurstLength = 1;
  g_console_dma_tx.Init.DestBurstLength = 1;
  g_console_dma_tx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
  g_console_dma_tx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  g_console_dma_tx.Init.Mode = DMA_NORMAL;
  BSP_CHECK(HAL_DMA_Init(&g_console_dma_tx) == HAL_OK);
  __HAL_LINKDMA(huart, hdmatx, g_console_dma_tx);
  BSP_CHECK(HAL_DMA_ConfigChannelAttributes(&g_console_dma_tx, DMA_CHANNEL_NPRIV) == HAL_OK);
}

}  // namespace

// Called by HAL_UART_Init: kernel clock, pins, DMA channels and NVIC for the console.
extern "C" void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
  if (huart->Instance != USART1) {
    return;
  }
  RCC_PeriphCLKInitTypeDef kernel_clock{};
  kernel_clock.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  kernel_clock.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  BSP_CHECK(HAL_RCCEx_PeriphCLKConfig(&kernel_clock) == HAL_OK);

  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  GPIO_InitTypeDef pins{};
  pins.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  pins.Mode = GPIO_MODE_AF_PP;
  pins.Pull = GPIO_PULLUP;
  pins.Speed = GPIO_SPEED_FREQ_LOW;
  pins.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &pins);

  init_console_dma(huart);

  HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, bsp::irq_priority::kConsoleDma, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
  HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, bsp::irq_priority::kConsoleDma, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
  HAL_NVIC_SetPriority(USART1_IRQn, bsp::irq_priority::kConsoleUart, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

// Interrupt handlers: override the weak aliases in the CMSIS startup file.
extern "C" {
void USART1_IRQHandler() { HAL_UART_IRQHandler(&g_console_uart); }
void GPDMA1_Channel0_IRQHandler() { HAL_DMA_IRQHandler(&g_console_dma_rx); }
void GPDMA1_Channel1_IRQHandler() { HAL_DMA_IRQHandler(&g_console_dma_tx); }
}

// CPU fault handling. On exception entry the CPU pushes r0-r3, r12, lr, pc and xPSR onto
// whichever stack was active: the task's (PSP) or the main stack (MSP). Bit 2 of the
// EXC_RETURN value in lr says which. The naked trampolines pick the right stack pointer
// without touching any stack themselves, then jump to the C++ dispatcher.
extern "C" [[noreturn]] void bsp_fault_dispatch(const std::uint32_t* frame,
                                                std::uint32_t exc_return, std::uint32_t kind) {
  bsp::FaultInfo info{};
  info.kind = static_cast<bsp::CpuFault>(kind);
  info.r0 = frame[0];
  info.r1 = frame[1];
  info.r2 = frame[2];
  info.r3 = frame[3];
  info.r12 = frame[4];
  info.lr = frame[5];
  info.pc = frame[6];
  info.psr = frame[7];
  info.cfsr = SCB->CFSR;
  info.hfsr = SCB->HFSR;
  info.mmfar = SCB->MMFAR;
  info.bfar = SCB->BFAR;
  info.exc_return = exc_return;
  bsp::motor_hw::emergency_stop();  // first priority: the motor must not keep running
  bsp::cpu_fault_hook(info);
}

#define BSP_FAULT_TRAMPOLINE(name, kind)          \
  extern "C" __attribute__((naked)) void name() { \
    __asm volatile(                               \
        "tst lr, #4            \n"                \
        "ite eq                \n"                \
        "mrseq r0, msp         \n"                \
        "mrsne r0, psp         \n"                \
        "mov r1, lr            \n"                \
        "movs r2, #" #kind "   \n"                \
        "b bsp_fault_dispatch  \n");              \
  }

BSP_FAULT_TRAMPOLINE(HardFault_Handler, 0)
BSP_FAULT_TRAMPOLINE(MemManage_Handler, 1)
BSP_FAULT_TRAMPOLINE(BusFault_Handler, 2)
BSP_FAULT_TRAMPOLINE(UsageFault_Handler, 3)

namespace bsp {

void init() {
  init_stack_limit_and_faults();
  g_reset_cause = capture_reset_cause();
  HAL_Init();  // flash prefetch, HAL tick on TIM6 (see hal_timebase.cpp)
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);  // all bits pre-emption, as FreeRTOS expects
  init_power();
  init_clocks();
  init_icache();
  init_gpio();
  init_console_uart();
  init_cycle_counter();
  motor_hw::init();  // ADC calibration waits on HAL_GetTick(): must run before the RTOS starts
}

const char* board_name() { return "NUCLEO-U575ZI-Q"; }
ClockSource clock_source() { return g_clock_source; }

const char* clock_description() {
  return g_clock_source == ClockSource::kCrystalReferenced
             ? "PLL1 from MSIS 4 MHz, auto-trimmed to the 32.768 kHz LSE crystal"
             : "PLL1 from MSIS 4 MHz, free-running (LSE crystal did not start)";
}

ResetCause reset_cause() { return g_reset_cause; }
std::uint32_t core_clock_hz() { return SystemCoreClock; }

void led_set(bool on) { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
void led_toggle() { HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7); }

void system_reset() {
  NVIC_SystemReset();
  for (;;) {
  }
}

UART_HandleTypeDef& console_uart() { return g_console_uart; }

}  // namespace bsp
