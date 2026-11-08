// Author: Raguram Kasthurisamy

#include "app/app.hpp"

#include "app/can_task.hpp"
#include "app/cli_task.hpp"
#include "app/health.hpp"
#include "app/supervisor_task.hpp"
#include "app/control_task.hpp"
#include "app/heartbeat_task.hpp"
#include "app/motor_config.hpp"
#include "app/telemetry_task.hpp"
#include "bsp/peripherals.hpp"
#include "control/simulated_motor_io.hpp"
#include "drivers/motor_hardware_io.hpp"
#include "drivers/uart_dma.hpp"
#include "os/task_config.hpp"

namespace app {
namespace {

// The whole system, allocated statically. These constructors only store references:
// no hardware access and no FreeRTOS calls happen before main() (see main.cpp).
drivers::UartDma g_console{bsp::console_uart()};
drivers::MotorHardwareIo g_motor_hardware{motor_config::kCurrentSenseVoltsPerAmp,
                                          motor_config::kEncoderSign};
control::SimulatedMotorIo g_motor_simulation{motor_config::kSimulatedMotor,
                                             motor_config::kControlPeriodS};
ControlTask g_control{g_motor_hardware, g_motor_simulation, motor_config::kController,
                      motor_config::kControlRateHz};
TelemetryTask g_telemetry{g_console, g_control};
CanTask g_can{g_control};
SupervisorTask g_supervisor;
HeartbeatTask g_heartbeat;
CliTask g_cli{g_console, cli_commands()};

}  // namespace

drivers::UartDma& console() { return g_console; }
HeartbeatTask& heartbeat() { return g_heartbeat; }
ControlTask& control_task() { return g_control; }
TelemetryTask& telemetry_task() { return g_telemetry; }
CanTask& can_task() { return g_can; }
SupervisorTask& supervisor_task() { return g_supervisor; }
void health_checkin(HealthId id) { g_supervisor.checkin(id); }

void start() {
  g_console.init();
  g_control.init();
  g_can.init();

  // Liveness deadlines, generous compared with each task's normal loop period.
  safety::TaskMonitor& monitor = g_supervisor.monitor();
  const auto watch = [&monitor](const char* name, std::uint32_t deadline_ms) {
    const auto id = monitor.add(name, deadline_ms, 0);
    configASSERT(id.has_value());
    return *id;
  };
  g_control.set_health_id(watch("control", 100));      // runs every 1 ms
  g_can.set_health_id(watch("can", 200));              // every 10 ms
  g_cli.set_health_id(watch("cli", 1000));             // read timeout 200 ms
  g_telemetry.set_health_id(watch("telemetry", 2500)); // at most every 1 s
  g_heartbeat.set_health_id(watch("heartbeat", 6000)); // half blink period, up to 2.5 s

  g_control.start("control", os::task_config::kControlPriority);
  g_supervisor.start("supervisor", os::task_config::kSupervisorPriority);
  g_can.start("can", os::task_config::kCanPriority);
  g_telemetry.start("telemetry", os::task_config::kTelemetryPriority);
  g_heartbeat.start("heartbeat", os::task_config::kHeartbeatPriority);
  g_cli.start("cli", os::task_config::kCliPriority);
}

}  // namespace app
