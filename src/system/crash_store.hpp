// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "bsp/fault.hpp"
#include "safety/crash_record.hpp"

// The crash record kept in .noinit RAM: written when the firmware gives up, read and cleared
// by the next boot. Writing it allocates nothing and takes no locks, so it works from fault
// handlers.
namespace sys::crash {

void record(safety::FaultType type, std::string_view text, int line, std::uint32_t pc);
void record_cpu_fault(const bsp::FaultInfo& info);
// Moves a valid record (if any) out of RAM. Call once, early in boot.
[[nodiscard]] std::optional<safety::CrashRecord> take();

}  // namespace sys::crash
