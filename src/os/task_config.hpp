// Author: Raguram Kasthurisamy

#pragma once

#include <cstddef>

#include "FreeRTOS.h"

// Every task's priority and stack size, in one table, so the scheduling design can be
// reviewed at a glance. Stack sizes are in words (4 bytes); check real usage with the
// CLI `tasks` command (minimum free stack) and trim once the code settles.
namespace os::task_config {

// Higher number = more urgent. Idle is 0.
inline constexpr UBaseType_t kHeartbeatPriority = 1;
inline constexpr UBaseType_t kTelemetryPriority = 1;
inline constexpr UBaseType_t kCliPriority = 2;
inline constexpr UBaseType_t kCanPriority = 4;
// The supervisor sits below the control loop on purpose: if the control task ever spins
// forever, the supervisor starves, stops feeding the watchdog, and the MCU resets.
inline constexpr UBaseType_t kSupervisorPriority = 5;
// The control loop has the highest application priority, so nothing can delay it.
inline constexpr UBaseType_t kControlPriority = 6;

inline constexpr std::size_t kHeartbeatStackWords = 160;
inline constexpr std::size_t kTelemetryStackWords = 384;
inline constexpr std::size_t kCliStackWords = 512;
inline constexpr std::size_t kCanStackWords = 512;
inline constexpr std::size_t kSupervisorStackWords = 384;
inline constexpr std::size_t kControlStackWords = 768;  // floats: FPU context is saved too

static_assert(kControlPriority < configMAX_PRIORITIES, "priority out of range");

}  // namespace os::task_config
