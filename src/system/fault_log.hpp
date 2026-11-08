// Author: Raguram Kasthurisamy

#pragma once

#include <cstddef>
#include <optional>

#include "safety/crash_record.hpp"

// Persistent fault log in the reserved flash page. Survives power cycles and reflashing.
// Uses HAL flash timeouts: call from a task, or from main() before any FreeRTOS object.
namespace sys::fault_log {

[[nodiscard]] std::size_t capacity();
[[nodiscard]] std::size_t count();
[[nodiscard]] std::optional<safety::CrashRecord> read(std::size_t index);
// Assigns the next sequence number, seals and stores the record. When the page is full it is
// erased first (oldest records are lost; the sequence keeps counting).
bool append(safety::CrashRecord& record);
bool erase();

}  // namespace sys::fault_log
