// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

namespace bsp {

using TickCallback = void (*)();

// Starts a hardware timer that calls `callback` at `rate_hz` from its interrupt
// (priority irq_priority::kControlTick). Used to pace the control loop.
void control_tick_start(std::uint32_t rate_hz, TickCallback callback);

// Free-running CPU cycle counter (DWT CYCCNT), for measuring latency and jitter.
// Wraps every 2^32 cycles (~26.8 s at 160 MHz); use differences.
[[nodiscard]] std::uint32_t cycle_count();

}  // namespace bsp
