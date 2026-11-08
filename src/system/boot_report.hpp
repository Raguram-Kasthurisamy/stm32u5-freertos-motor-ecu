// Author: Raguram Kasthurisamy

#pragma once

#include <optional>

#include "safety/crash_record.hpp"

namespace sys::boot {

// Picks up a crash record left by the previous run, stamps it with this boot's reset cause
// and appends it to the flash log. Call once from main(), after bsp::init() and before any
// FreeRTOS object exists (flash programming waits on HAL timeouts).
void process();
// The fault that caused the previous reset, if there was one.
[[nodiscard]] const std::optional<safety::CrashRecord>& last_fault();

}  // namespace sys::boot
