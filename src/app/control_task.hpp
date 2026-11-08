// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "FreeRTOS.h"
#include "control/motor_io.hpp"
#include "control/speed_controller.hpp"
#include "app/health.hpp"
#include "os/static_task.hpp"
#include "os/task_config.hpp"
#include "queue.h"

namespace app {

// Requests to the control task. Other tasks never touch the controller directly: they
// post a command, and the control task applies it at the start of its next period.
struct ControlCommand {
  enum class Type : std::uint8_t {
    kStop,
    kOpenLoop,     // value = duty, -1..1
    kSpeed,        // value = rpm
    kSetGains,     // gains
    kUseSimulation,  // flag; only honoured while the motor is stopped
    kClearFault,
    kResetTiming,
  };
  Type type = Type::kStop;
  float value = 0.0f;
  control::PidGains gains{};
  bool flag = false;
};
static_assert(std::is_trivially_copyable_v<ControlCommand>, "copied by value through a queue");

// Loop timing measured with the CPU cycle counter.
struct ControlTiming {
  std::uint32_t loops = 0;
  std::uint32_t missed_ticks = 0;                 // ticks that arrived while still busy
  std::uint32_t period_min_cycles = 0xFFFF'FFFFU;  // tick-to-tick spacing seen by the task
  std::uint32_t period_max_cycles = 0;
  std::uint32_t latency_max_cycles = 0;  // timer interrupt -> control task running
  std::uint32_t exec_max_cycles = 0;     // sensors + controller + PWM update
  std::uint64_t exec_total_cycles = 0;
};

struct ControlSnapshot {
  control::Status status{};
  control::PidGains gains{};
  const char* io_name = "";
};

// The 1 kHz motor control loop, at the highest application priority.
//
// A TIM7 interrupt wakes the task through a direct-to-task notification (the lightest
// FreeRTOS signalling primitive). Each period it applies pending commands, reads the
// sensors, runs the controller and writes the PWM, then publishes a snapshot for the
// other tasks. Snapshots and timing are copied under a short critical section rather
// than a mutex: the control task must never block waiting for a lower-priority task.
class ControlTask final : public os::StaticTask<ControlTask, os::task_config::kControlStackWords> {
 public:
  ControlTask(control::MotorIo& hardware, control::MotorIo& simulation,
              const control::ControllerConfig& config, std::uint32_t rate_hz);

  void init();  // creates the command queue; call before the scheduler starts
  [[nodiscard]] bool send(const ControlCommand& command);
  [[nodiscard]] ControlSnapshot snapshot() const;
  [[nodiscard]] ControlTiming timing() const;
  [[nodiscard]] std::uint32_t rate_hz() const { return rate_hz_; }
  void set_health_id(HealthId id) { health_id_ = id; }

 private:
  friend class os::StaticTask<ControlTask, os::task_config::kControlStackWords>;
  static constexpr std::size_t kQueueLength = 8;
  static constexpr std::uint32_t kTickTimeoutMs = 10;

  static void tick_isr();  // TIM7 interrupt context
  void run();
  void apply(const ControlCommand& command);
  void record_timing(std::uint32_t wake, std::uint32_t pending);
  void publish(std::uint32_t exec_cycles);
  void outputs_off();

  static ControlTask* instance_;

  control::MotorIo& hardware_;
  control::MotorIo& simulation_;
  control::MotorIo* io_;
  control::SpeedController controller_;
  const std::uint32_t rate_hz_;
  const float period_s_;

  StaticQueue_t queue_storage_{};
  std::array<ControlCommand, kQueueLength> queue_buffer_{};
  QueueHandle_t queue_ = nullptr;

  volatile std::uint32_t tick_stamp_ = 0;  // cycle count captured in the tick interrupt
  std::uint32_t last_wake_ = 0;
  bool timing_primed_ = false;
  ControlTiming timing_work_{};  // owned by the control task
  HealthId health_id_ = kNoHealthId;
  bool clear_pending_ = false;

  // Shared with other tasks; only accessed inside critical sections.
  ControlSnapshot shared_snapshot_{};
  ControlTiming shared_timing_{};
};

}  // namespace app
