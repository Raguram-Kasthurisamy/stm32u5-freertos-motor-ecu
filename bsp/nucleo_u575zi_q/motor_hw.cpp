// Author: Raguram Kasthurisamy

// Motor power stage and sensors on NUCLEO-U575ZI-Q.
//
//   Signal              Pin    Peripheral                  H-bridge pin (DRV8874, PH/EN mode)
//   PWM                 PE9    TIM1_CH1 (AF1), 20 kHz       EN
//   Direction           PF12   GPIO output                 PH
//   Driver enable       PF13   GPIO output, high = awake   nSLEEP
//   Driver fault        PE15   TIM1_BKIN (AF1), pull-up    nFAULT (open drain, active low)
//   Encoder A / B       PA0/PA1 TIM2_CH1/CH2 (AF1)          (motor encoder, 3.3 V supply)
//   Current sense       PA3    ADC1_IN8                    IPROPI / CS output
//
// Data path for current: TIM1 update (every PWM period) -> TRGO -> ADC1 conversion ->
// GPDMA1 channel 2 -> circular buffer of one control period's worth of samples. No
// interrupt fires anywhere along that path; the control task just averages the buffer.
//
// Hardware trip: nFAULT drives TIM1's break input. When the driver detects overcurrent or
// overtemperature it pulls nFAULT low and TIM1 clears its main output enable (MOE) within a
// few timer clocks, with no software in the path. The break flag latches the event, so the
// control loop sees it even if nFAULT recovers before the next 1 ms cycle.
//
// For an IN1/IN2-style driver (TB6612, L298...), only set_duty() needs to change.

#include "bsp/motor_hw.hpp"

#include <algorithm>
#include <array>

#include "bsp/board.hpp"
#include "bsp/hal.hpp"
#include "timer_clock.hpp"

#define MOTOR_CHECK(expr)                                                      \
  do {                                                                         \
    if (!(expr)) {                                                             \
      bsp::fatal_error_hook("motor hardware init failed: " #expr, __FILE__, __LINE__); \
    }                                                                          \
  } while (0)

namespace {

constexpr std::uint32_t kPwmFrequencyHz = 20'000;  // above the audible range
constexpr std::size_t kCurrentSamples = 20;        // 20 kHz x 1 ms = one control period
constexpr float kAdcFullScale = 16383.0f;          // ADC1 is 14-bit
constexpr float kAdcReferenceV = 3.3f;             // VREF+ is tied to VDD on the Nucleo

TIM_HandleTypeDef g_pwm_timer{};
TIM_HandleTypeDef g_encoder_timer{};
ADC_HandleTypeDef g_adc{};
DMA_HandleTypeDef g_adc_dma{};
DMA_NodeTypeDef g_adc_node{};
DMA_QListTypeDef g_adc_queue{};
std::array<std::uint16_t, kCurrentSamples> g_current_samples{};  // written by the DMA
std::uint32_t g_pwm_period_counts = 0;

void init_driver_gpio() {
  __HAL_RCC_GPIOF_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12 | GPIO_PIN_13, GPIO_PIN_RESET);  // asleep, forward
  GPIO_InitTypeDef outputs{};
  outputs.Pin = GPIO_PIN_12 | GPIO_PIN_13;
  outputs.Mode = GPIO_MODE_OUTPUT_PP;
  outputs.Pull = GPIO_NOPULL;
  outputs.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &outputs);

  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitTypeDef fault{};
  fault.Pin = GPIO_PIN_15;
  fault.Mode = GPIO_MODE_AF_PP;
  fault.Pull = GPIO_PULLUP;  // nFAULT is open drain; with no driver attached it reads "ok"
  fault.Speed = GPIO_SPEED_FREQ_LOW;
  fault.Alternate = GPIO_AF1_TIM1;  // TIM1_BKIN; the pin level stays readable through IDR
  HAL_GPIO_Init(GPIOE, &fault);
}

void init_encoder() {
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef pins{};
  pins.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  pins.Mode = GPIO_MODE_AF_PP;
  pins.Pull = GPIO_PULLUP;  // many Hall encoders have open-collector outputs
  pins.Speed = GPIO_SPEED_FREQ_LOW;
  pins.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &pins);

  // TIM2 is a 32-bit timer: at full motor speed it would take days to wrap, and the speed
  // estimator handles the wrap anyway.
  g_encoder_timer.Instance = TIM2;
  g_encoder_timer.Init.Prescaler = 0;
  g_encoder_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  g_encoder_timer.Init.Period = 0xFFFF'FFFFU;
  g_encoder_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  g_encoder_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  TIM_Encoder_InitTypeDef encoder{};
  encoder.EncoderMode = TIM_ENCODERMODE_TI12;  // count every edge of both channels: 4x
  encoder.IC1Polarity = TIM_ICPOLARITY_RISING;
  encoder.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  encoder.IC1Prescaler = TIM_ICPSC_DIV1;
  encoder.IC1Filter = 0xA;  // ignore pulses shorter than ~0.8 us (8 samples at 160 MHz / 16)
  encoder.IC2Polarity = TIM_ICPOLARITY_RISING;
  encoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  encoder.IC2Prescaler = TIM_ICPSC_DIV1;
  encoder.IC2Filter = 0xA;
  MOTOR_CHECK(HAL_TIM_Encoder_Init(&g_encoder_timer, &encoder) == HAL_OK);
  MOTOR_CHECK(HAL_TIM_Encoder_Start(&g_encoder_timer, TIM_CHANNEL_ALL) == HAL_OK);
}

void init_current_dma() {
  DMA_NodeConfTypeDef node{};
  node.NodeType = DMA_GPDMA_LINEAR_NODE;
  node.Init.Request = GPDMA1_REQUEST_ADC1;
  node.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  node.Init.Direction = DMA_PERIPH_TO_MEMORY;
  node.Init.SrcInc = DMA_SINC_FIXED;
  node.Init.DestInc = DMA_DINC_INCREMENTED;
  node.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
  node.Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;
  node.Init.SrcBurstLength = 1;
  node.Init.DestBurstLength = 1;
  node.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
  node.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  node.Init.Mode = DMA_NORMAL;
  node.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
  node.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  node.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  MOTOR_CHECK(HAL_DMAEx_List_BuildNode(&node, &g_adc_node) == HAL_OK);
  MOTOR_CHECK(HAL_DMAEx_List_InsertNode(&g_adc_queue, nullptr, &g_adc_node) == HAL_OK);
  MOTOR_CHECK(HAL_DMAEx_List_SetCircularMode(&g_adc_queue) == HAL_OK);

  g_adc_dma.Instance = GPDMA1_Channel2;
  g_adc_dma.InitLinkedList.Priority = DMA_HIGH_PRIORITY;
  g_adc_dma.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
  g_adc_dma.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  g_adc_dma.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  g_adc_dma.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
  MOTOR_CHECK(HAL_DMAEx_List_Init(&g_adc_dma) == HAL_OK);
  MOTOR_CHECK(HAL_DMAEx_List_LinkQ(&g_adc_dma, &g_adc_queue) == HAL_OK);
  __HAL_LINKDMA(&g_adc, DMA_Handle, g_adc_dma);
  MOTOR_CHECK(HAL_DMA_ConfigChannelAttributes(&g_adc_dma, DMA_CHANNEL_NPRIV) == HAL_OK);
  // GPDMA1_Channel2_IRQn and ADC1_IRQn deliberately stay disabled in the NVIC: the samples
  // are consumed by polling once per control period, so there is nothing to interrupt for.
}

void init_current_adc() {
  RCC_PeriphCLKInitTypeDef kernel_clock{};
  kernel_clock.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC;
  kernel_clock.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_HCLK;
  MOTOR_CHECK(HAL_RCCEx_PeriphCLKConfig(&kernel_clock) == HAL_OK);
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_GPDMA1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_PWREx_EnableVddA();  // the analog supply must be declared valid before using the ADC

  GPIO_InitTypeDef pin{};
  pin.Pin = GPIO_PIN_3;
  pin.Mode = GPIO_MODE_ANALOG;
  pin.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &pin);

  g_adc.Instance = ADC1;
  g_adc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;  // 160 MHz / 4 = 40 MHz (max 55 MHz)
  g_adc.Init.Resolution = ADC_RESOLUTION_14B;
  g_adc.Init.GainCompensation = 0;
  g_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  g_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  g_adc.Init.LowPowerAutoWait = DISABLE;
  g_adc.Init.ContinuousConvMode = DISABLE;
  g_adc.Init.NbrOfConversion = 1;
  g_adc.Init.DiscontinuousConvMode = DISABLE;
  g_adc.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T1_TRGO;  // one conversion per PWM period
  g_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  g_adc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  g_adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  g_adc.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  g_adc.Init.OversamplingMode = DISABLE;
  MOTOR_CHECK(HAL_ADC_Init(&g_adc) == HAL_OK);

  ADC_ChannelConfTypeDef channel{};
  channel.Channel = ADC_CHANNEL_8;  // PA3
  channel.Rank = ADC_REGULAR_RANK_1;
  channel.SamplingTime = ADC_SAMPLETIME_68CYCLES;  // ~1.7 us: settles the sense filter cap
  channel.SingleDiff = ADC_SINGLE_ENDED;
  channel.OffsetNumber = ADC_OFFSET_NONE;
  channel.Offset = 0;
  MOTOR_CHECK(HAL_ADC_ConfigChannel(&g_adc, &channel) == HAL_OK);

  // Factory calibration of offset and linearity; uses HAL_GetTick() timeouts, which is one
  // more reason this runs in bsp::init() before any FreeRTOS object exists.
  MOTOR_CHECK(HAL_ADCEx_Calibration_Start(&g_adc, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) ==
              HAL_OK);

  init_current_dma();
  // Armed now; conversions start when TIM1 begins producing TRGO events.
  MOTOR_CHECK(HAL_ADC_Start_DMA(&g_adc, reinterpret_cast<uint32_t*>(g_current_samples.data()),
                                kCurrentSamples) == HAL_OK);
}

void init_pwm() {
  __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef pin{};
  pin.Pin = GPIO_PIN_9;
  pin.Mode = GPIO_MODE_AF_PP;
  pin.Pull = GPIO_PULLDOWN;  // keeps the bridge off while the pin is still floating at reset
  pin.Speed = GPIO_SPEED_FREQ_MEDIUM;
  pin.Alternate = GPIO_AF1_TIM1;
  HAL_GPIO_Init(GPIOE, &pin);

  g_pwm_period_counts = bsp::detail::apb2_timer_clock() / kPwmFrequencyHz;  // 8000 at 160 MHz
  g_pwm_timer.Instance = TIM1;
  g_pwm_timer.Init.Prescaler = 0;
  g_pwm_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  g_pwm_timer.Init.Period = g_pwm_period_counts - 1U;
  g_pwm_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  g_pwm_timer.Init.RepetitionCounter = 0;
  g_pwm_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  MOTOR_CHECK(HAL_TIM_PWM_Init(&g_pwm_timer) == HAL_OK);

  TIM_MasterConfigTypeDef master{};
  master.MasterOutputTrigger = TIM_TRGO_UPDATE;  // ADC trigger at the start of every period
  master.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  MOTOR_CHECK(HAL_TIMEx_MasterConfigSynchronization(&g_pwm_timer, &master) == HAL_OK);

  TIM_OC_InitTypeDef output{};
  output.OCMode = TIM_OCMODE_PWM1;
  output.Pulse = 0;
  output.OCPolarity = TIM_OCPOLARITY_HIGH;
  output.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  output.OCFastMode = TIM_OCFAST_DISABLE;
  output.OCIdleState = TIM_OCIDLESTATE_RESET;
  output.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  MOTOR_CHECK(HAL_TIM_PWM_ConfigChannel(&g_pwm_timer, &output, TIM_CHANNEL_1) == HAL_OK);

  TIM_BreakDeadTimeConfigTypeDef protection{};
  protection.OffStateRunMode = TIM_OSSR_DISABLE;
  protection.OffStateIDLEMode = TIM_OSSI_ENABLE;  // MOE = 0 drives the pin low, not floating
  protection.LockLevel = TIM_LOCKLEVEL_OFF;
  protection.DeadTime = 0;
  protection.BreakState = TIM_BREAK_ENABLE;
  protection.BreakPolarity = TIM_BREAKPOLARITY_LOW;  // nFAULT is active low
  protection.BreakFilter = 4;                        // ignore sub-microsecond glitches
  protection.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;  // software decides when to re-arm
  MOTOR_CHECK(HAL_TIMEx_ConfigBreakDeadTime(&g_pwm_timer, &protection) == HAL_OK);

  // Starts the counter (and so the ADC triggers) and sets MOE on this advanced timer.
  MOTOR_CHECK(HAL_TIM_PWM_Start(&g_pwm_timer, TIM_CHANNEL_1) == HAL_OK);
}

}  // namespace

namespace bsp::motor_hw {

void init() {
  init_driver_gpio();
  init_encoder();
  init_current_adc();  // armed first, so the first PWM period is already sampled
  init_pwm();
}

void set_duty(float duty) {
  duty = std::clamp(duty, -1.0f, 1.0f);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12, duty < 0.0f ? GPIO_PIN_SET : GPIO_PIN_RESET);
  const float magnitude = duty < 0.0f ? -duty : duty;
  // CCR1 is preloaded: the new duty takes effect at the next PWM period boundary, so a
  // period is never cut short or doubled (no glitch on the output).
  const auto compare =
      static_cast<std::uint32_t>(magnitude * static_cast<float>(g_pwm_period_counts) + 0.5f);
  __HAL_TIM_SET_COMPARE(&g_pwm_timer, TIM_CHANNEL_1, compare);
}

void set_driver_enabled(bool enabled) {
  if (enabled && (TIM1->BDTR & TIM_BDTR_MOE) == 0U && (TIM1->SR & TIM_SR_BIF) == 0U) {
    __HAL_TIM_MOE_ENABLE(&g_pwm_timer);  // re-arm after a cleared trip
  }
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void clear_fault_latch() { TIM1->SR = ~TIM_SR_BIF; }

void emergency_stop() {
  TIM1->BDTR &= ~TIM_BDTR_MOE;
  TIM1->CCR1 = 0U;
  GPIOF->BSRR = static_cast<std::uint32_t>(GPIO_PIN_13) << 16U;  // nSLEEP low
}

std::int32_t encoder_count() { return static_cast<std::int32_t>(TIM2->CNT); }

float current_sense_volts() {
  // The DMA keeps writing while this runs. Each 16-bit read is atomic, so the worst case
  // is an average that mixes samples from this period and the previous one.
  const volatile std::uint16_t* samples = g_current_samples.data();
  std::uint32_t sum = 0;
  for (std::size_t i = 0; i < kCurrentSamples; ++i) {
    sum += samples[i];
  }
  return static_cast<float>(sum) *
         (kAdcReferenceV / (kAdcFullScale * static_cast<float>(kCurrentSamples)));
}

bool driver_fault() {
  const bool pin_low = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15) == GPIO_PIN_RESET;
  const bool tripped = (TIM1->SR & TIM_SR_BIF) != 0U;
  return pin_low || tripped;
}

}  // namespace bsp::motor_hw
