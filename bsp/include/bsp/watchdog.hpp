// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

// Independent watchdog (IWDG): clocked from its own oscillator, cannot be stopped once
// started, resets the MCU if not fed within the timeout.
namespace bsp::watchdog {

void start(std::uint32_t timeout_ms);  // up to 4096 ms
void feed();

}  // namespace bsp::watchdog
