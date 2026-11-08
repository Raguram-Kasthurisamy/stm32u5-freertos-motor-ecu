// Author: Raguram Kasthurisamy

#include "app/control_task.hpp"

#include "bsp/timing.hpp"
#include "task.h"

namespace app {

ControlTask* ControlTask::instance_ = nullptr;

ControlTask::ControlTask(control::MotorIo& hardware, control::MotorIo& simulation,
                         const control::ControllerConfig& config, std::uint32_t rate_hz)
    : hardware_(hardware),
      simulation_(simulation),
      io_(&hardware),
      controller_(config),
      rate_hz_(rate_hz),
      period_s_(1.0f / static_cast<float>(rate_hz)) {}

void ControlTask::init() {
  queue_ = xQueueCreateStatic(kQueueLength, sizeof(ControlCommand),
                              reinterpret_cast<std::uint8_t*>(queue_buffer_.data()), &queue_storage_);
  configASSERT(queue_ != nullptr);
  instance_ = this;
  shared_snapshot_.gains = controller_.gains();
  shared_snapshot_.io_name = io_->name();
}

bool ControlTask::send(const ControlCommand& command) {
  return xQueueSend(queue_, &command, pdMS_TO_TICKS(5)) == pdTRUE;
}

ControlSnapshot ControlTask::snapshot() const {
  taskENTER_CRITICAL();
  const ControlSnapshot copy = shared_snapshot_;
  taskEXIT_CRITICAL();
  return copy;
}

ControlTiming ControlTask::timing() const {
  taskENTER_CRITICAL();
  const ControlTiming copy = shared_timing_;
  taskEXIT_CRITICAL();
  return copy;
}

void ControlTask::tick_isr() {
  ControlTask* const self = instance_;
  self->tick_stamp_ = bsp::cycle_count();
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(self->handle(), &woken);
  portYIELD_FROM_ISR(woken);
}

void ControlTask::outputs_off() {
  io_->write(0.0f);
  io_->set_enabled(false);
}

void ControlTask::apply(const ControlCommand& command) {
  using Type = ControlCommand::Type;
  switch (command.type) {
    case Type::kStop:
      controller_.stop();
      break;
    case Type::kOpenLoop:
      (void)controller_.set_open_loop(command.value);
      break;
    case Type::kSpeed:
      (void)controller_.set_speed(command.value);
      break;
    case Type::kSetGains:
      controller_.set_gains(command.gains);
      break;
    case Type::kUseSimulation: {
      const control::Mode mode = controller_.status().mode;
      if (mode == control::Mode::kDisabled || mode == control::Mode::kFault) {
        outputs_off();  // leave the old backend safe before switching
        io_ = command.flag ? &simulation_ : &hardware_;
      }
      break;
    }
    case Type::kClearFault:
      // Release the hardware trip latch now; the controller fault is cleared after the next
      // sample confirms the driver is healthy again.
      io_->reset_fault_latch();
      clear_pending_ = true;
      break;
    case Type::kResetTiming:
      timing_work_ = ControlTiming{};
      timing_primed_ = false;
      break;
  }
}

void ControlTask::record_timing(std::uint32_t wake, std::uint32_t pending) {
  ControlTiming& t = timing_work_;
  ++t.loops;
  t.missed_ticks += pending - 1U;  // more than one notification = a tick was missed
  const std::uint32_t latency = wake - tick_stamp_;  // unsigned: correct across counter wrap
  if (latency > t.latency_max_cycles) {
    t.latency_max_cycles = latency;
  }
  if (timing_primed_) {
    const std::uint32_t period = wake - last_wake_;
    if (period < t.period_min_cycles) {
      t.period_min_cycles = period;
    }
    if (period > t.period_max_cycles) {
      t.period_max_cycles = period;
    }
  }
  last_wake_ = wake;
  timing_primed_ = true;
}

void ControlTask::publish(std::uint32_t exec_cycles) {
  timing_work_.exec_total_cycles += exec_cycles;
  if (exec_cycles > timing_work_.exec_max_cycles) {
    timing_work_.exec_max_cycles = exec_cycles;
  }
  taskENTER_CRITICAL();
  shared_snapshot_.status = controller_.status();
  shared_snapshot_.gains = controller_.gains();
  shared_snapshot_.io_name = io_->name();
  shared_timing_ = timing_work_;
  taskEXIT_CRITICAL();
}

void ControlTask::run() {
  outputs_off();
  bsp::control_tick_start(rate_hz_, &ControlTask::tick_isr);

  for (;;) {
    // Wait for the tick. The return value is how many ticks arrived since last time.
    const std::uint32_t pending = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kTickTimeoutMs));
    const std::uint32_t wake = bsp::cycle_count();
    if (pending == 0U) {
      // The timer stopped: without a tick there is no control, so fail safe.
      controller_.trip(control::Fault::kTickLost);
      outputs_off();
      publish(0);
      continue;
    }
    record_timing(wake, pending);

    ControlCommand command;
    while (xQueueReceive(queue_, &command, 0) == pdTRUE) {
      apply(command);
    }

    const control::Sample sample = io_->read();
    const control::Output output = controller_.update(sample, period_s_);
    io_->write(output.duty);
    io_->set_enabled(output.driver_enabled);
    if (clear_pending_) {
      clear_pending_ = false;
      (void)controller_.clear_fault();
    }
    health_checkin(health_id_);

    publish(bsp::cycle_count() - wake);
  }
}

}  // namespace app
