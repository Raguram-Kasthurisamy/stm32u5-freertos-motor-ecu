// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

namespace app {

// Liveness check-in for tasks watched by the supervisor. Each monitored task gets an id at
// start-up and calls this once per loop. A single atomic store: cheap enough at 1 kHz.
using HealthId = std::uint8_t;
inline constexpr HealthId kNoHealthId = 0xFF;
void health_checkin(HealthId id);

}  // namespace app
