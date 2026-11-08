// Author: Raguram Kasthurisamy

#include "app/supervisor_task.hpp"

#include "bsp/motor_hw.hpp"
#include "bsp/watchdog.hpp"
#include "system/crash_store.hpp"
#include "task.h"

namespace app {
namespace {
std::uint32_t now_ms() { return xTaskGetTickCount(); }  // 1 ms tick
}  // namespace

void SupervisorTask::checkin(safety::TaskMonitor::Id id) { monitor_.checkin(id, now_ms()); }

void SupervisorTask::run() {
  bsp::watchdog::start(kWatchdogTimeoutMs);
  running_.store(true, std::memory_order_relaxed);

  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kPeriodMs));
    if (stall_handled_) {
      continue;  // not feeding: the watchdog reset is on its way
    }
    const std::optional<safety::TaskMonitor::Id> stalled = monitor_.find_stalled(now_ms());
    if (!stalled) {
      bsp::watchdog::feed();
      continue;
    }
    stall_handled_ = true;
    bsp::motor_hw::emergency_stop();
    sys::crash::record(safety::FaultType::kTaskStall, monitor_.name(*stalled),
                       static_cast<int>(monitor_.age_ms(*stalled, now_ms())), 0U);
  }
}

}  // namespace app
