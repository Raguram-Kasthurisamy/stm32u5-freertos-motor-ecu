// Author: Raguram Kasthurisamy

#include "app/can_task.hpp"

#include "bsp/board.hpp"
#include "bsp/can_hw.hpp"
#include "protocol/ecu_protocol.hpp"
#include "task.h"

namespace app {
namespace {
std::uint32_t now_ms() { return xTaskGetTickCount(); }
}  // namespace

CanTask* CanTask::instance_ = nullptr;

void CanTask::init() {
  rx_queue_ = xQueueCreateStatic(kRxQueueLength, sizeof(protocol::CanFrame),
                                 reinterpret_cast<std::uint8_t*>(rx_queue_buffer_.data()),
                                 &rx_queue_storage_);
  tx_queue_ = xQueueCreateStatic(kTxQueueLength, sizeof(protocol::CanFrame),
                                 reinterpret_cast<std::uint8_t*>(tx_queue_buffer_.data()),
                                 &tx_queue_storage_);
  configASSERT(rx_queue_ != nullptr && tx_queue_ != nullptr);
  instance_ = this;
}

void CanTask::on_rx_isr(const protocol::CanFrame& frame) {
  CanTask* const self = instance_;
  BaseType_t woken = pdFALSE;
  if (xQueueSendFromISR(self->rx_queue_, &frame, &woken) != pdTRUE) {
    self->rx_dropped_.fetch_add(1, std::memory_order_relaxed);
  }
  portYIELD_FROM_ISR(woken);
}

bool CanTask::inject(const protocol::CanFrame& frame) {
  return xQueueSend(tx_queue_, &frame, pdMS_TO_TICKS(5)) == pdTRUE;
}

CanTask::Stats CanTask::stats() const {
  taskENTER_CRITICAL();
  Stats copy = shared_;
  taskEXIT_CRITICAL();
  copy.rx_dropped = rx_dropped_.load(std::memory_order_relaxed);
  return copy;
}

void CanTask::start_controller(bool loopback) {
  const bool ok = bsp::can_hw::start(
      loopback ? bsp::can_hw::Mode::kInternalLoopback : bsp::can_hw::Mode::kNormal,
      protocol::kCommandId, &CanTask::on_rx_isr);
  configASSERT(ok);
  work_.loopback = loopback;
}

void CanTask::send(const protocol::CanFrame& frame) {
  if (bsp::can_hw::send(frame)) {
    ++work_.tx_frames;
  } else {
    ++work_.tx_dropped;  // TX FIFO full: bus saturated, disconnected, or bus-off
  }
}

void CanTask::handle_frame(const protocol::CanFrame& frame) {
  ++work_.rx_frames;
  protocol::Command command;
  if (monitor_.on_frame(frame, now_ms(), command) != protocol::CommandMonitor::Result::kAccepted) {
    return;
  }
  ControlCommand request;
  switch (command.mode) {
    case protocol::CommandMode::kStop:
      request.type = ControlCommand::Type::kStop;
      break;
    case protocol::CommandMode::kOpenLoop:
      request.type = ControlCommand::Type::kOpenLoop;
      request.value = command.duty;
      break;
    case protocol::CommandMode::kSpeed:
      request.type = ControlCommand::Type::kSpeed;
      request.value = command.speed_rpm;
      break;
    case protocol::CommandMode::kClearFault:
      request.type = ControlCommand::Type::kClearFault;
      break;
  }
  (void)control_.send(request);
}

void CanTask::transmit_cycle() {
  const ControlSnapshot snap = control_.snapshot();
  const control::Status& s = snap.status;
  send(protocol::encode(protocol::Status{
      .mode = static_cast<std::uint8_t>(s.mode),
      .fault = static_cast<std::uint8_t>(s.fault),
      .speed_rpm = s.speed_rpm,
      .setpoint_rpm = s.setpoint_rpm,
      .duty = s.duty,
      .counter = status_counter_++,
  }));
  send(protocol::encode(protocol::Measurements{
      .current_a = s.current_a, .target_rpm = s.target_rpm, .encoder_count = s.encoder_count}));
  if (cycle_ % kHeartbeatDivider == 0U) {
    send(protocol::encode(protocol::Heartbeat{
        .state = static_cast<std::uint8_t>(s.mode),
        .reset_cause = static_cast<std::uint8_t>(bsp::reset_cause()),
        .uptime_s = now_ms() / 1000U,
    }));
  }
  ++cycle_;
}

void CanTask::service_requests() {
  const int mode = mode_request_.exchange(-1, std::memory_order_relaxed);
  if (mode >= 0) {
    start_controller(mode == 1);
  }
  if (release_requested_.exchange(false, std::memory_order_relaxed)) {
    monitor_.release();
  }
  protocol::CanFrame frame;
  while (xQueueReceive(tx_queue_, &frame, 0) == pdTRUE) {
    send(frame);
  }
}

void CanTask::run() {
  start_controller(false);
  const TickType_t period = pdMS_TO_TICKS(kFastPeriodMs);
  TickType_t next_cycle = xTaskGetTickCount() + period;

  for (;;) {
    const TickType_t now = xTaskGetTickCount();
    const auto remaining = static_cast<std::int32_t>(next_cycle - now);  // wrap-safe
    protocol::CanFrame frame;
    if (remaining > 0 && xQueueReceive(rx_queue_, &frame, static_cast<TickType_t>(remaining)) == pdTRUE) {
      handle_frame(frame);
      continue;
    }

    next_cycle += period;
    if (static_cast<std::int32_t>(xTaskGetTickCount() - next_cycle) > 0) {
      next_cycle = xTaskGetTickCount() + period;  // fell behind: skip, do not burst
    }

    service_requests();
    transmit_cycle();

    const bsp::can_hw::BusState bus = bsp::can_hw::bus_state();
    if (bus.bus_off) {
      ++work_.bus_off_events;
      bsp::can_hw::recover_from_bus_off();
    }
    if (monitor_.check_timeout(now_ms())) {
      (void)control_.send(ControlCommand{});  // default command: stop
    }

    work_.bus_off = bus.bus_off;
    work_.error_passive = bus.error_passive;
    work_.tx_errors = bus.tx_errors;
    work_.rx_errors = bus.rx_errors;
    work_.session_active = monitor_.active();
    work_.commands = monitor_.stats();
    taskENTER_CRITICAL();
    shared_ = work_;
    taskEXIT_CRITICAL();
    health_checkin(health_id_);
  }
}

}  // namespace app
