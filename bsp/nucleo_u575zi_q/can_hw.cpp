// Author: Raguram Kasthurisamy

// FDCAN1 on NUCLEO-U575ZI-Q: PD0 = FDCAN1_RX, PD1 = FDCAN1_TX (AF9), to an external
// 3.3 V transceiver such as the SN65HVD230. Kernel clock: PLL1Q = 80 MHz.

#include "bsp/can_hw.hpp"

#include <array>

#include "bsp/hal.hpp"
#include "bsp/irq_priority.hpp"
#include "bsp_check.hpp"

namespace {

// 500 kbit/s from an 80 MHz kernel clock: prescaler 10 -> 8 MHz time quanta, 16 quanta per
// bit (1 sync + 13 + 2), sample point at 14/16 = 87.5 %, the value CANopen recommends.
constexpr std::uint32_t kKernelClockHz = 80'000'000;
constexpr std::uint32_t kPrescaler = 10;
constexpr std::uint32_t kTimeSeg1 = 13;
constexpr std::uint32_t kTimeSeg2 = 2;
constexpr std::uint32_t kSyncJumpWidth = 2;
constexpr std::uint32_t kBitrate = 500'000;
static_assert(kKernelClockHz / (kPrescaler * (1 + kTimeSeg1 + kTimeSeg2)) == kBitrate,
              "CAN bit timing does not give 500 kbit/s");

FDCAN_HandleTypeDef g_fdcan{};
bsp::can_hw::RxCallback g_rx_callback = nullptr;

const std::array<std::uint32_t, 9> kDlcCodes{
    FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2, FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4,
    FDCAN_DLC_BYTES_5, FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8};

// Some HAL versions encode the DLC shifted left by 16 bits, others not; accept both.
std::uint8_t dlc_to_bytes(std::uint32_t code) {
  if (code > 0xFU) {
    code >>= 16U;
  }
  return code <= 8U ? static_cast<std::uint8_t>(code) : 8U;
}

}  // namespace

extern "C" void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef* hfdcan) {
  if (hfdcan->Instance != FDCAN1) {
    return;
  }
  RCC_PeriphCLKInitTypeDef kernel_clock{};
  kernel_clock.PeriphClockSelection = RCC_PERIPHCLK_FDCAN1;
  kernel_clock.Fdcan1ClockSelection = RCC_FDCAN1CLKSOURCE_PLL1;
  BSP_CHECK(HAL_RCCEx_PeriphCLKConfig(&kernel_clock) == HAL_OK);
  __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL1_DIVQ);
  __HAL_RCC_FDCAN1_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  GPIO_InitTypeDef pins{};
  pins.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  pins.Mode = GPIO_MODE_AF_PP;
  pins.Pull = GPIO_NOPULL;
  pins.Speed = GPIO_SPEED_FREQ_MEDIUM;
  pins.Alternate = GPIO_AF9_FDCAN1;
  HAL_GPIO_Init(GPIOD, &pins);

  HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, bsp::irq_priority::kCan, 0);
  HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
}

extern "C" void FDCAN1_IT0_IRQHandler(void) { HAL_FDCAN_IRQHandler(&g_fdcan); }

extern "C" void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U) {
    return;
  }
  while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U) {
    FDCAN_RxHeaderTypeDef header{};
    protocol::CanFrame frame{};
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &header, frame.data.data()) != HAL_OK) {
      break;
    }
    if (header.IdType != FDCAN_STANDARD_ID) {
      continue;
    }
    frame.id = header.Identifier;
    frame.dlc = dlc_to_bytes(header.DataLength);
    if (g_rx_callback != nullptr) {
      g_rx_callback(frame);
    }
  }
}

namespace bsp::can_hw {

bool start(Mode mode, std::uint32_t accept_id, RxCallback callback) {
  if (g_fdcan.Instance != nullptr) {
    (void)HAL_FDCAN_Stop(&g_fdcan);
    (void)HAL_FDCAN_DeInit(&g_fdcan);
  }
  g_rx_callback = callback;

  g_fdcan.Instance = FDCAN1;
  g_fdcan.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  g_fdcan.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  g_fdcan.Init.Mode = mode == Mode::kInternalLoopback ? FDCAN_MODE_INTERNAL_LOOPBACK : FDCAN_MODE_NORMAL;
  g_fdcan.Init.AutoRetransmission = ENABLE;
  g_fdcan.Init.TransmitPause = DISABLE;
  g_fdcan.Init.ProtocolException = DISABLE;
  g_fdcan.Init.NominalPrescaler = kPrescaler;
  g_fdcan.Init.NominalSyncJumpWidth = kSyncJumpWidth;
  g_fdcan.Init.NominalTimeSeg1 = kTimeSeg1;
  g_fdcan.Init.NominalTimeSeg2 = kTimeSeg2;
  g_fdcan.Init.DataPrescaler = 1;  // data phase unused in classic CAN
  g_fdcan.Init.DataSyncJumpWidth = 1;
  g_fdcan.Init.DataTimeSeg1 = 1;
  g_fdcan.Init.DataTimeSeg2 = 1;
  g_fdcan.Init.StdFiltersNbr = 1;
  g_fdcan.Init.ExtFiltersNbr = 0;
  g_fdcan.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&g_fdcan) != HAL_OK) {
    return false;
  }

  // Hardware acceptance filtering: only the command identifier reaches the CPU.
  FDCAN_FilterTypeDef filter{};
  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = accept_id;
  filter.FilterID2 = 0x7FF;  // all 11 bits must match
  if (HAL_FDCAN_ConfigFilter(&g_fdcan, &filter) != HAL_OK ||
      HAL_FDCAN_ConfigGlobalFilter(&g_fdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK ||
      HAL_FDCAN_ActivateNotification(&g_fdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
    return false;
  }
  return HAL_FDCAN_Start(&g_fdcan) == HAL_OK;
}

bool send(const protocol::CanFrame& frame) {
  FDCAN_TxHeaderTypeDef header{};
  header.Identifier = frame.id;
  header.IdType = FDCAN_STANDARD_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = kDlcCodes[frame.dlc <= 8U ? frame.dlc : 8U];
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = FDCAN_BRS_OFF;
  header.FDFormat = FDCAN_CLASSIC_CAN;
  header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  header.MessageMarker = 0;
  std::array<std::uint8_t, 8> data = frame.data;
  return HAL_FDCAN_AddMessageToTxFifoQ(&g_fdcan, &header, data.data()) == HAL_OK;
}

BusState bus_state() {
  FDCAN_ProtocolStatusTypeDef protocol_status{};
  FDCAN_ErrorCountersTypeDef counters{};
  (void)HAL_FDCAN_GetProtocolStatus(&g_fdcan, &protocol_status);
  (void)HAL_FDCAN_GetErrorCounters(&g_fdcan, &counters);
  return BusState{
      .bus_off = protocol_status.BusOff != 0U,
      .error_passive = protocol_status.ErrorPassive != 0U,
      .warning = protocol_status.Warning != 0U,
      .tx_errors = static_cast<std::uint8_t>(counters.TxErrorCnt),
      .rx_errors = static_cast<std::uint8_t>(counters.RxErrorCnt),
  };
}

void recover_from_bus_off() { CLEAR_BIT(g_fdcan.Instance->CCCR, FDCAN_CCCR_INIT); }

}  // namespace bsp::can_hw
