// Author: Raguram Kasthurisamy

#include "app/heartbeat_task.hpp"

#include "bsp/board.hpp"

namespace app {

void HeartbeatTask::run() {
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    bsp::led_toggle();
    health_checkin(health_id_);
    // Two toggles per period. xTaskDelayUntil wakes at fixed instants, so the blink rate
    // does not drift with however long the loop body takes (unlike vTaskDelay).
    (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms() / 2U));
  }
}

}  // namespace app
