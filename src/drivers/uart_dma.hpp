// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "FreeRTOS.h"
#include "bsp/hal.hpp"
#include "io/serial_port.hpp"
#include "os/mutex.hpp"
#include "semphr.h"
#include "stream_buffer.h"

namespace drivers {

// Interrupt-light UART driver built on DMA.
//
// RX: the DMA runs continuously in circular mode. The HAL raises an "RX event" at half
//     buffer, full buffer, and whenever the line goes idle; the ISR copies only the new
//     bytes into a FreeRTOS stream buffer and wakes the reader. Cost: one interrupt per
//     burst, not one per byte.
// TX: write() copies into a DMA buffer, starts the transfer, and blocks on a semaphore
//     until the DMA-complete interrupt. The calling task sleeps, the CPU stays free.
//
// Thread safety: write() may be called from several tasks (serialized by a mutex).
// read() supports a single reader task (stream buffers are single-reader by design).
class UartDma final : public io::SerialPort {
 public:
  struct Stats {
    std::uint32_t rx_bytes;
    std::uint32_t rx_dropped;  // bytes lost because the reader fell behind
    std::uint32_t rx_errors;   // overrun, framing, noise, DMA errors
    std::uint32_t tx_bytes;
    std::uint32_t tx_errors;
  };

  explicit UartDma(UART_HandleTypeDef& huart) : huart_(huart) {}
  UartDma(const UartDma&) = delete;
  UartDma& operator=(const UartDma&) = delete;

  // Creates the RTOS objects. Call after bsp::init() and before the scheduler starts.
  void init();
  // Arms circular DMA reception. Call from the task that will read.
  void start_rx();

  void write(std::span<const std::uint8_t> data) override;
  std::size_t read(std::span<std::uint8_t> buffer, std::uint32_t timeout_ms) override;

  [[nodiscard]] Stats stats() const;

  // Interrupt-context entry points, called from the HAL callbacks in uart_dma.cpp.
  void on_rx_event(std::uint16_t dma_position);
  void on_tx_complete();
  void on_error();
  [[nodiscard]] const UART_HandleTypeDef* hal_handle() const { return &huart_; }

 private:
  static constexpr std::size_t kRxDmaSize = 256;      // ~22 ms of data at 115200 baud
  static constexpr std::size_t kRxStreamSize = 512;
  static constexpr std::size_t kTxDmaSize = 256;
  static constexpr std::uint32_t kTxTimeoutMs = 100;  // 256 bytes take ~22 ms

  void push_from_isr(std::size_t offset, std::size_t length, BaseType_t& woken);

  UART_HandleTypeDef& huart_;

  // DMA buffers live in this object (.bss, normal SRAM). TX copies into its own buffer
  // instead of pointing the DMA at the caller's memory: the caller's data may be in flash
  // or, on other STM32 families, in core-coupled RAM that the DMA cannot reach.
  std::array<std::uint8_t, kRxDmaSize> rx_dma_{};
  std::array<std::uint8_t, kTxDmaSize> tx_dma_{};
  std::size_t rx_last_position_ = 0;  // touched only in ISR context (and before RX starts)

  // +1: older FreeRTOS versions require one byte more than the usable size.
  std::array<std::uint8_t, kRxStreamSize + 1> rx_stream_storage_{};
  StaticStreamBuffer_t rx_stream_struct_{};
  StreamBufferHandle_t rx_stream_ = nullptr;

  os::Mutex tx_mutex_;
  StaticSemaphore_t tx_done_storage_{};
  SemaphoreHandle_t tx_done_ = nullptr;

  std::atomic<std::uint32_t> rx_bytes_{0};
  std::atomic<std::uint32_t> rx_dropped_{0};
  std::atomic<std::uint32_t> rx_errors_{0};
  std::atomic<std::uint32_t> tx_bytes_{0};
  std::atomic<std::uint32_t> tx_errors_{0};
};

}  // namespace drivers
