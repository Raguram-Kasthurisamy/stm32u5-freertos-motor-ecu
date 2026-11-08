// Author: Raguram Kasthurisamy

// Console commands. Each handler is a plain function: easy to read, easy to add to.

#include <array>
#include <cstdint>

#include "FreeRTOS.h"
#include "app/app.hpp"
#include "app/heartbeat_task.hpp"
#include "app/motor_commands.hpp"
#include "app/system_commands.hpp"
#include "bsp/board.hpp"
#include "cli/parse.hpp"
#include "drivers/uart_dma.hpp"
#include "task.h"

namespace app {
namespace {

using cli::Args;
using io::TextWriter;

static_assert(configTICK_RATE_HZ == 1000, "uptime math assumes a 1 ms tick");

void print_uptime(TextWriter& out) {
  const std::uint32_t total_ms = xTaskGetTickCount();  // 32-bit: wraps after ~49.7 days
  const std::uint32_t total_s = total_ms / 1000U;
  out.u32(total_s / 86400U)
      .str("d ")
      .u32((total_s / 3600U) % 24U, 2, '0')
      .chr(':')
      .u32((total_s / 60U) % 60U, 2, '0')
      .chr(':')
      .u32(total_s % 60U, 2, '0')
      .chr('.')
      .u32(total_ms % 1000U, 3, '0');
}

const char* state_name(eTaskState state) {
  switch (state) {
    case eRunning: return "run";
    case eReady: return "ready";
    case eBlocked: return "block";
    case eSuspended: return "susp";
    case eDeleted: return "del";
    case eInvalid: break;
  }
  return "?";
}

void cmd_version(TextWriter& out, Args) {
  out.str("firmware: ").str(ECU_FW_VERSION).nl();
  out.str("kernel:   FreeRTOS ").str(tskKERNEL_VERSION_NUMBER).nl();
  out.str("compiler: GCC ").str(__VERSION__).nl();
}

void cmd_sysinfo(TextWriter& out, Args) {
  out.str("board:       ").str(bsp::board_name()).nl();
  out.str("core clock:  ").u32(bsp::core_clock_hz()).str(" Hz").nl();
  out.str("clock tree:  ").str(bsp::clock_description()).nl();
  if (bsp::clock_source() == bsp::ClockSource::kInternalFallback) {
    out.str("warning:     no crystal reference, timing accuracy reduced").nl();
  }
  out.str("reset cause: ").str(bsp::to_string(bsp::reset_cause())).nl();
  out.str("uptime:      ");
  print_uptime(out);
  out.nl();
}

void cmd_uptime(TextWriter& out, Args) {
  print_uptime(out);
  out.nl();
}

void cmd_tasks(TextWriter& out, Args) {
  // static: eight TaskStatus_t would otherwise take ~300 bytes of the CLI task's stack.
  static std::array<TaskStatus_t, 8> status{};
  const UBaseType_t count =
      uxTaskGetSystemState(status.data(), static_cast<UBaseType_t>(status.size()), nullptr);
  if (count == 0) {
    out.str("error: more than ").u32(static_cast<std::uint32_t>(status.size()))
        .str(" tasks, enlarge the table").nl();
    return;
  }
  out.str("name", 13).str("state", 7).str("prio", 6).str("min free stack (words)").nl();
  for (UBaseType_t i = 0; i < count; ++i) {
    const TaskStatus_t& task = status[i];
    out.str(task.pcTaskName, 13)
        .str(state_name(task.eCurrentState), 7)
        .u32(static_cast<std::uint32_t>(task.uxCurrentPriority), 4)
        .str("  ")
        .u32(static_cast<std::uint32_t>(task.usStackHighWaterMark))
        .nl();
  }
}

void cmd_uart(TextWriter& out, Args) {
  const drivers::UartDma::Stats stats = console().stats();
  out.str("rx bytes:   ").u32(stats.rx_bytes).nl();
  out.str("rx dropped: ").u32(stats.rx_dropped).nl();
  out.str("rx errors:  ").u32(stats.rx_errors).nl();
  out.str("tx bytes:   ").u32(stats.tx_bytes).nl();
  out.str("tx errors:  ").u32(stats.tx_errors).nl();
}

void cmd_led(TextWriter& out, Args args) {
  if (args.size() == 1) {
    out.str("blink period: ").u32(heartbeat().period_ms()).str(" ms").nl();
    return;
  }
  const auto period = cli::parse_u32(args[1]);
  if (args.size() != 2 || !period || *period < HeartbeatTask::kMinPeriodMs ||
      *period > HeartbeatTask::kMaxPeriodMs) {
    out.str("usage: led [period_ms]   (")
        .u32(HeartbeatTask::kMinPeriodMs)
        .chr('-')
        .u32(HeartbeatTask::kMaxPeriodMs)
        .str(" ms)")
        .nl();
    return;
  }
  heartbeat().set_period_ms(*period);
  out.str("blink period set to ").u32(*period).str(" ms").nl();
}

void cmd_reset(TextWriter& out, Args) {
  out.str("resetting...").nl();   // nl() flushes; write() returns once the UART is done
  vTaskDelay(pdMS_TO_TICKS(10));  // give the ST-LINK's USB bridge time to forward it
  bsp::system_reset();
}

constexpr std::array kCommands{
    cli::Command{"version", "firmware, kernel and compiler versions", &cmd_version},
    cli::Command{"sysinfo", "board, clock source, reset cause, uptime", &cmd_sysinfo},
    cli::Command{"uptime", "time since boot", &cmd_uptime},
    cli::Command{"tasks", "task states, priorities and stack headroom", &cmd_tasks},
    cli::Command{"uart", "console driver statistics", &cmd_uart},
    cli::Command{"led", "show or set the LED blink period: led [ms]", &cmd_led},
    cli::Command{"reset", "software reset", &cmd_reset},
    cli::Command{"motor", "controller mode, setpoint, speed, duty, current", &motor_commands::motor},
    cli::Command{"duty", "open-loop drive: duty <-100..100 %>", &motor_commands::duty},
    cli::Command{"speed", "closed-loop speed: speed <rpm>", &motor_commands::speed},
    cli::Command{"stop", "disable the motor (coast)", &motor_commands::stop},
    cli::Command{"pid", "show or set gains: pid [kp ki kd]", &motor_commands::pid},
    cli::Command{"sim", "simulated motor instead of hardware: sim [on|off]", &motor_commands::sim},
    cli::Command{"clear", "clear a latched fault", &motor_commands::clear},
    cli::Command{"ctrl", "control loop timing: ctrl [reset]", &motor_commands::ctrl},
    cli::Command{"stream", "telemetry lines for the plot tool: stream [hz|off]", &motor_commands::stream},
    cli::Command{"can", "CAN bus state and counters: can [loopback on|off]", &system_commands::can},
    cli::Command{"cantx", "send a test command frame: cantx stop|clear|speed|duty", &system_commands::cantx},
    cli::Command{"health", "task check-ins watched by the supervisor", &system_commands::health},
    cli::Command{"faults", "last fault and the flash fault log: faults [clear]", &system_commands::faults},
    cli::Command{"fault", "inject a fault on purpose: fault hang|stack|div0|assert", &system_commands::fault},
};

}  // namespace

std::span<const cli::Command> cli_commands() { return kCommands; }

}  // namespace app
