// Author: Raguram Kasthurisamy

#include "app/system_commands.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "app/app.hpp"
#include "app/can_task.hpp"
#include "app/supervisor_task.hpp"
#include "bsp/board.hpp"
#include "cli/parse.hpp"
#include "protocol/ecu_protocol.hpp"
#include "system/boot_report.hpp"
#include "system/fault_log.hpp"
#include "task.h"

namespace app::system_commands {
namespace {

using io::TextWriter;

std::uint8_t g_cantx_counter = 0;  // console-only test sender

void print_frame_counters(TextWriter& out, const CanTask::Stats& s) {
  out.str("mode:        ").str(s.loopback ? "internal loopback" : "normal").nl();
  out.str("bus:         ")
      .str(s.bus_off ? "BUS-OFF" : s.error_passive ? "error passive" : "error active")
      .str("   TEC ").u32(s.tx_errors).str("  REC ").u32(s.rx_errors).nl();
  out.str("frames:      rx ").u32(s.rx_frames).str("  tx ").u32(s.tx_frames)
      .str("  rx dropped ").u32(s.rx_dropped).str("  tx dropped ").u32(s.tx_dropped).nl();
  out.str("bus-off:     ").u32(s.bus_off_events).str(" recoveries").nl();
  out.str("commands:    accepted ").u32(s.commands.accepted).str("  invalid ")
      .u32(s.commands.invalid).str("  repeated ").u32(s.commands.repeated).str("  timeouts ")
      .u32(s.commands.timeouts).nl();
  out.str("CAN control: ").str(s.session_active ? "active" : "idle").nl();
}

// Recursion that the compiler cannot prove infinite (the limit is volatile) and cannot
// turn into a loop (work happens after the call), so every level really uses stack.
volatile std::uint32_t g_recursion_limit = 0xFFFF'FFFFU;
[[gnu::noinline]] std::uint32_t use_stack(std::uint32_t depth) {
  volatile std::uint8_t frame[64];
  frame[0] = static_cast<std::uint8_t>(depth);
  if (depth >= g_recursion_limit) {
    return frame[0];
  }
  return use_stack(depth + 1U) + frame[0];
}

}  // namespace

void print_fault_record(TextWriter& out, const safety::CrashRecord& r) {
  out.str("#").u32(r.sequence).str(" ").str(safety::to_string(r.type));
  if (!safety::text(r).empty()) {
    out.str(": ").str(safety::text(r));
  }
  if (r.line != 0) {
    out.str(" (").i32(r.line).chr(')');
  }
  out.nl();
  out.str("   pc 0x").hex32(r.pc).str("  lr 0x").hex32(r.lr).str("  cfsr 0x").hex32(r.cfsr);
  if (r.fault_address != 0U) {
    out.str("  addr 0x").hex32(r.fault_address);
  }
  out.str("  reset: ").str(bsp::to_string(static_cast<bsp::ResetCause>(r.reset_cause))).nl();
}

void can(TextWriter& out, cli::Args args) {
  if (args.size() == 3 && args[1] == "loopback" && (args[2] == "on" || args[2] == "off")) {
    can_task().request_loopback(args[2] == "on");
    out.str("CAN controller restarting in ").str(args[2] == "on" ? "internal loopback" : "normal")
        .str(" mode").nl();
    return;
  }
  if (args.size() != 1) {
    out.str("usage: can [loopback on|off]").nl();
    return;
  }
  print_frame_counters(out, can_task().stats());
}

void cantx(TextWriter& out, cli::Args args) {
  protocol::Command command;
  bool ok = false;
  if (args.size() == 2 && args[1] == "stop") {
    command.mode = protocol::CommandMode::kStop;
    ok = true;
  } else if (args.size() == 2 && args[1] == "clear") {
    command.mode = protocol::CommandMode::kClearFault;
    ok = true;
  } else if (args.size() == 3 && args[1] == "speed") {
    const auto rpm = cli::parse_float(args[2]);
    ok = rpm.has_value();
    command.mode = protocol::CommandMode::kSpeed;
    command.speed_rpm = rpm.value_or(0.0f);
  } else if (args.size() == 3 && args[1] == "duty") {
    const auto percent = cli::parse_float(args[2]);
    ok = percent.has_value() && *percent >= -100.0f && *percent <= 100.0f;
    command.mode = protocol::CommandMode::kOpenLoop;
    command.duty = percent.value_or(0.0f) / 100.0f;
  }
  if (!ok) {
    out.str("usage: cantx stop | clear | speed <rpm> | duty <-100..100>").nl();
    out.str("       sends an ECU_Command frame; with 'can loopback on' it comes back to this node")
        .nl();
    return;
  }
  command.counter = ++g_cantx_counter;
  if (!can_task().inject(protocol::encode(command))) {
    out.str("error: CAN task busy").nl();
  }
}

void health(TextWriter& out, cli::Args) {
  const safety::TaskMonitor& monitor = supervisor_task().monitor();
  const std::uint32_t now = xTaskGetTickCount();
  out.str("watchdog: ").str(supervisor_task().watchdog_running() ? "running" : "not started")
      .str(", timeout ").u32(SupervisorTask::kWatchdogTimeoutMs).str(" ms").nl();
  out.str("task", 12).str("last check-in", 16).str("deadline").nl();
  for (std::size_t i = 0; i < monitor.size(); ++i) {
    const auto id = static_cast<safety::TaskMonitor::Id>(i);
    out.str(monitor.name(id), 12)
        .u32(monitor.age_ms(id, now), 6).str(" ms ago     ")
        .u32(monitor.deadline_ms(id)).str(" ms").nl();
  }
}

void faults(TextWriter& out, cli::Args args) {
  if (args.size() == 2 && args[1] == "clear") {
    out.str(sys::fault_log::erase() ? "fault log erased" : "error: erase failed").nl();
    return;
  }
  if (const auto& last = sys::boot::last_fault()) {
    out.str("previous reset was caused by:").nl();
    print_fault_record(out, *last);
  } else {
    out.str("previous reset: ").str(bsp::to_string(bsp::reset_cause())).str(", no fault recorded").nl();
  }
  const std::size_t count = sys::fault_log::count();
  out.str("flash log: ").u32(static_cast<std::uint32_t>(count)).str(" of ")
      .u32(static_cast<std::uint32_t>(sys::fault_log::capacity())).str(" slots used").nl();
  constexpr std::size_t kShowLast = 8;
  const std::size_t first = count > kShowLast ? count - kShowLast : 0;
  for (std::size_t i = first; i < count; ++i) {
    if (const auto record = sys::fault_log::read(i)) {
      print_fault_record(out, *record);
    }
  }
}

void fault(TextWriter& out, cli::Args args) {
  if (args.size() != 2) {
    out.str("usage: fault hang | stack | div0 | assert").nl();
    out.str("       each one resets the board on purpose; 'faults' shows the record after").nl();
    return;
  }
  if (args[1] == "hang") {
    out.str("console task spinning forever: the supervisor should catch it within ~1 s").nl();
    volatile bool spin = true;
    while (spin) {
    }
  } else if (args[1] == "stack") {
    out.str("recursing until the task stack limit (PSPLIM) trips").nl();
    (void)use_stack(0);
  } else if (args[1] == "div0") {
    out.str("dividing by zero (trapped by CCR.DIV_0_TRP)").nl();
    volatile std::int32_t divisor = 0;
    volatile std::int32_t result = 100 / divisor;
    (void)result;
  } else if (args[1] == "assert") {
    out.str("failing an assert").nl();
    configASSERT(false);
  } else {
    out.str("unknown fault: ").str(args[1]).nl();
  }
}

}  // namespace app::system_commands
