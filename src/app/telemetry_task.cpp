// Author: Raguram Kasthurisamy

#include "app/telemetry_task.hpp"

#include "task.h"

namespace app {

void TelemetryTask::run() {
  TickType_t last_wake = xTaskGetTickCount();
  for (;;) {
    health_checkin(health_id_);
    const std::uint32_t hz = rate_hz();
    if (hz == 0U) {
      vTaskDelay(pdMS_TO_TICKS(100));
      last_wake = xTaskGetTickCount();
      continue;
    }
    (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000U / hz));

    const ControlSnapshot snap = control_.snapshot();
    const control::Status& s = snap.status;
    out_.str("$T,")
        .u32(xTaskGetTickCount())
        .chr(',').fixed(s.target_rpm, 2)
        .chr(',').fixed(s.setpoint_rpm, 2)
        .chr(',').fixed(s.speed_rpm, 2)
        .chr(',').fixed(s.duty, 4)
        .chr(',').fixed(s.current_a, 3)
        .chr(',').u32(static_cast<std::uint32_t>(s.mode))
        .nl();  // one UART write per line, so lines never interleave with other output
  }
}

}  // namespace app
