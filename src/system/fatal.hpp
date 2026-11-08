// Author: Raguram Kasthurisamy

#pragma once

#include "safety/crash_record.hpp"

namespace sys {

// The single place the firmware goes when it cannot continue safely: failed asserts,
// stack overflows, heap use, failed hardware init. Stops the motor, records what happened
// in no-init RAM, halts if a debugger is attached, otherwise reboots. The next boot reports
// the record and appends it to the flash fault log.
[[noreturn]] void fatal(safety::FaultType type, const char* where, int line);

// Common tail of every fatal path: outputs off, debugger break or reset.
[[noreturn]] void reboot_after_fault();

}  // namespace sys
