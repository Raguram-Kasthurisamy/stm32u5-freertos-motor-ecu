// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "FreeRTOS.h"
#include "app/control_task.hpp"
#include "app/health.hpp"
#include "os/static_task.hpp"
#include "os/task_config.hpp"
#include "protocol/can_frame.hpp"
#include "protocol/command_monitor.hpp"
#include "queue.h"

namespace app {

// The ECU's CAN node: receives commands, transmits status every 10 ms and a heartbeat every
// 100 ms, recovers from bus-off, and stops the motor if a CAN-commanded session goes silent.
//
// The CAN interrupt only copies frames into a queue; all protocol work happens here, in task
// context, at a priority below the control loop.
class CanTask final : public os::StaticTask<CanTask, os::task_config::kCanStackWords> {
 public:
  static constexpr std::uint32_t kFastPeriodMs = 10;      // status + measurements
  static constexpr std::uint32_t kHeartbeatDivider = 10;  // heartbeat every 100 ms
  static constexpr std::uint32_t kCommandTimeoutMs = 250;

  struct Stats {
    bool loopback;
    bool bus_off;
    bool error_passive;
    std::uint8_t tx_errors;
    std::uint8_t rx_errors;
    std::uint32_t rx_frames;
    std::uint32_t rx_dropped;
    std::uint32_t tx_frames;
    std::uint32_t tx_dropped;
    std::uint32_t bus_off_events;
    bool session_active;
    protocol::CommandMonitor::Stats commands;
  };

  explicit CanTask(ControlTask& control) : control_(control), monitor_(kCommandTimeoutMs) {}

  void init();  // creates the queues; call before the scheduler starts
  void set_health_id(HealthId id) { health_id_ = id; }

  // Thread-safe requests from other tasks (the console).
  void request_release() { release_requested_.store(true, std::memory_order_relaxed); }
  void request_loopback(bool on) { mode_request_.store(on ? 1 : 0, std::memory_order_relaxed); }
  [[nodiscard]] bool inject(const protocol::CanFrame& frame);  // transmit a test frame
  [[nodiscard]] Stats stats() const;

 private:
  friend class os::StaticTask<CanTask, os::task_config::kCanStackWords>;
  static constexpr std::size_t kRxQueueLength = 16;
  static constexpr std::size_t kTxQueueLength = 4;

  static void on_rx_isr(const protocol::CanFrame& frame);
  void run();
  void start_controller(bool loopback);
  void handle_frame(const protocol::CanFrame& frame);
  void transmit_cycle();
  void service_requests();
  void send(const protocol::CanFrame& frame);

  static CanTask* instance_;

  ControlTask& control_;
  protocol::CommandMonitor monitor_;
  HealthId health_id_ = kNoHealthId;

  StaticQueue_t rx_queue_storage_{};
  std::array<protocol::CanFrame, kRxQueueLength> rx_queue_buffer_{};
  QueueHandle_t rx_queue_ = nullptr;
  StaticQueue_t tx_queue_storage_{};
  std::array<protocol::CanFrame, kTxQueueLength> tx_queue_buffer_{};
  QueueHandle_t tx_queue_ = nullptr;

  std::atomic<bool> release_requested_{false};
  std::atomic<int> mode_request_{-1};
  std::atomic<std::uint32_t> rx_dropped_{0};
  std::uint32_t cycle_ = 0;
  std::uint8_t status_counter_ = 0;

  Stats work_{};    // owned by the CAN task
  Stats shared_{};  // copied under a critical section
};

}  // namespace app
