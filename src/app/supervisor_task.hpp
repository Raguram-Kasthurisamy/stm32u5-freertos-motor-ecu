// Author: Raguram Kasthurisamy

#pragma once

#include <atomic>
#include <cstdint>

#include "os/static_task.hpp"
#include "os/task_config.hpp"
#include "safety/task_monitor.hpp"

namespace app {

// Feeds the independent watchdog only while every monitored task is alive.
//
// If a task misses its deadline (deadlock, infinite loop, starvation), the supervisor stops
// the motor, records which task stalled in the crash record, and stops feeding the watchdog.
// The IWDG then resets the MCU, and the next boot reports "task stalled: <name>" with reset
// cause "independent watchdog". If the supervisor itself stops running, the watchdog resets
// the MCU anyway: the hardware is the last line of defence, software only decides when.
class SupervisorTask final
    : public os::StaticTask<SupervisorTask, os::task_config::kSupervisorStackWords> {
 public:
  static constexpr std::uint32_t kPeriodMs = 100;
  static constexpr std::uint32_t kWatchdogTimeoutMs = 1000;

  safety::TaskMonitor& monitor() { return monitor_; }
  const safety::TaskMonitor& monitor() const { return monitor_; }
  void checkin(safety::TaskMonitor::Id id);
  [[nodiscard]] bool watchdog_running() const { return running_.load(std::memory_order_relaxed); }

 private:
  friend class os::StaticTask<SupervisorTask, os::task_config::kSupervisorStackWords>;
  void run();

  safety::TaskMonitor monitor_;
  std::atomic<bool> running_{false};
  bool stall_handled_ = false;
};

}  // namespace app
