// Author: Raguram Kasthurisamy

#pragma once

#include <span>

#include "cli/cli.hpp"

namespace drivers {
class UartDma;
}

namespace app {

class CanTask;
class ControlTask;
class SupervisorTask;
class HeartbeatTask;
class TelemetryTask;

// Initializes driver objects and creates every task. Call once, after bsp::init() and
// before vTaskStartScheduler().
void start();

drivers::UartDma& console();
HeartbeatTask& heartbeat();
ControlTask& control_task();
TelemetryTask& telemetry_task();
CanTask& can_task();
SupervisorTask& supervisor_task();
std::span<const cli::Command> cli_commands();

}  // namespace app
