// Author: Raguram Kasthurisamy

#include "drivers/uart_dma.hpp"

#include <algorithm>
#include <cstring>

namespace drivers {
namespace {

// Maps HAL handles back to driver objects for the HAL's global callbacks.
// Registration happens in init(), before interrupts can fire.
constexpr std::size_t kMaxInstances = 2;
std::array<UartDma*, kMaxInstances> g_instances{};

void register_instance(UartDma* uart) {
  for (UartDma*& slot : g_instances) {
    if (slot == nullptr || slot == uart) {
      slot = uart;
      return;
    }
  }
  configASSERT(false);  // raise kMaxInstances
}

UartDma* find_instance(const UART_HandleTypeDef* huart) {
  for (UartDma* uart : g_instances) {
    if (uart != nullptr && uart->hal_handle() == huart) {
      return uart;
    }
  }
  return nullptr;
}

constexpr auto kRelaxed = std::memory_order_relaxed;

}  // namespace

void UartDma::init() {
  rx_stream_ = xStreamBufferCreateStatic(kRxStreamSize, 1, rx_stream_storage_.data(),
                                         &rx_stream_struct_);
  configASSERT(rx_stream_ != nullptr);
  tx_done_ = xSemaphoreCreateBinaryStatic(&tx_done_storage_);
  configASSERT(tx_done_ != nullptr);
  tx_mutex_.init();
  register_instance(this);
}

void UartDma::start_rx() {
  rx_last_position_ = 0;
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart_, rx_dma_.data(),
                                   static_cast<std::uint16_t>(rx_dma_.size())) != HAL_OK) {
    rx_errors_.fetch_add(1, kRelaxed);
  }
}

void UartDma::write(std::span<const std::uint8_t> data) {
  os::LockGuard lock(tx_mutex_);
  while (!data.empty()) {
    const std::size_t chunk = std::min(data.size(), tx_dma_.size());
    std::memcpy(tx_dma_.data(), data.data(), chunk);

    // Drop a completion left over from a transfer that previously timed out.
    (void)xSemaphoreTake(tx_done_, 0);

    if (HAL_UART_Transmit_DMA(&huart_, tx_dma_.data(), static_cast<std::uint16_t>(chunk)) !=
        HAL_OK) {
      tx_errors_.fetch_add(1, kRelaxed);
      return;
    }
    if (xSemaphoreTake(tx_done_, pdMS_TO_TICKS(kTxTimeoutMs)) != pdTRUE) {
      (void)HAL_UART_AbortTransmit(&huart_);
      tx_errors_.fetch_add(1, kRelaxed);
      return;
    }
    tx_bytes_.fetch_add(static_cast<std::uint32_t>(chunk), kRelaxed);
    data = data.subspan(chunk);
  }
}

std::size_t UartDma::read(std::span<std::uint8_t> buffer, std::uint32_t timeout_ms) {
  const TickType_t ticks =
      (timeout_ms == io::kWaitForever) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xStreamBufferReceive(rx_stream_, buffer.data(), buffer.size(), ticks);
}

UartDma::Stats UartDma::stats() const {
  return Stats{
      .rx_bytes = rx_bytes_.load(kRelaxed),
      .rx_dropped = rx_dropped_.load(kRelaxed),
      .rx_errors = rx_errors_.load(kRelaxed),
      .tx_bytes = tx_bytes_.load(kRelaxed),
      .tx_errors = tx_errors_.load(kRelaxed),
  };
}

void UartDma::push_from_isr(std::size_t offset, std::size_t length, BaseType_t& woken) {
  if (length == 0) {
    return;
  }
  BaseType_t task_woken = pdFALSE;
  const std::size_t sent =
      xStreamBufferSendFromISR(rx_stream_, &rx_dma_[offset], length, &task_woken);
  if (task_woken == pdTRUE) {
    woken = pdTRUE;
  }
  rx_bytes_.fetch_add(static_cast<std::uint32_t>(sent), kRelaxed);
  rx_dropped_.fetch_add(static_cast<std::uint32_t>(length - sent), kRelaxed);
}

// `dma_position` is how far the DMA has written into rx_dma_ (1..size). Everything between
// the previous position and this one is new. Because the HAL also reports half- and
// full-buffer events, at most half a buffer can arrive between two events, so a wrap is
// always detected.
void UartDma::on_rx_event(std::uint16_t dma_position) {
  const std::size_t position = dma_position;
  if (position > rx_dma_.size()) {
    return;
  }

  BaseType_t woken = pdFALSE;
  if (position > rx_last_position_) {
    push_from_isr(rx_last_position_, position - rx_last_position_, woken);
  } else if (position < rx_last_position_) {
    push_from_isr(rx_last_position_, rx_dma_.size() - rx_last_position_, woken);
    push_from_isr(0, position, woken);
  }
  rx_last_position_ = (position == rx_dma_.size()) ? 0 : position;

  portYIELD_FROM_ISR(woken);
}

void UartDma::on_tx_complete() {
  BaseType_t woken = pdFALSE;
  (void)xSemaphoreGiveFromISR(tx_done_, &woken);
  portYIELD_FROM_ISR(woken);
}

void UartDma::on_error() {
  rx_errors_.fetch_add(1, kRelaxed);
  // In DMA mode the HAL treats every receive error as blocking and stops reception.
  // Re-arm it so one glitch on the line does not silence the console forever.
  if (huart_.RxState == HAL_UART_STATE_READY) {
    start_rx();
  }
}

}  // namespace drivers

// HAL weak-callback overrides. They run in interrupt context.
extern "C" {

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size) {
  if (drivers::UartDma* uart = drivers::find_instance(huart)) {
    uart->on_rx_event(size);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  if (drivers::UartDma* uart = drivers::find_instance(huart)) {
    uart->on_tx_complete();
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
  if (drivers::UartDma* uart = drivers::find_instance(huart)) {
    uart->on_error();
  }
}

}  // extern "C"
