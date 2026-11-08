// Author: Raguram Kasthurisamy

#include "app/motor_commands.hpp"

#include "app/app.hpp"
#include "app/can_task.hpp"
#include "app/control_task.hpp"
#include "app/telemetry_task.hpp"
#include "bsp/board.hpp"
#include "cli/parse.hpp"
#include "control/speed_controller.hpp"

namespace app::motor_commands {
namespace {

using io::TextWriter;

// A console motor command takes control away from a CAN session, so that the CAN command
// timeout does not stop a motor the operator is now driving by hand.
void send_or_report(TextWriter& out, const ControlCommand& command) {
  can_task().request_release();
  if (!control_task().send(command)) {
    out.str("error: control task busy, try again").nl();
  }
}

float cycles_to_us(std::uint64_t cycles) {
  return static_cast<float>(cycles) / (static_cast<float>(bsp::core_clock_hz()) / 1.0e6f);
}

bool is_stopped(const control::Status& status) {
  return status.mode == control::Mode::kDisabled || status.mode == control::Mode::kFault;
}

}  // namespace

void motor(TextWriter& out, cli::Args) {
  const ControlSnapshot snap = control_task().snapshot();
  const control::Status& s = snap.status;
  out.str("mode:     ").str(control::to_string(s.mode)).str("   (io: ").str(snap.io_name).chr(')').nl();
  if (s.mode == control::Mode::kFault) {
    out.str("fault:    ").str(control::to_string(s.fault)).str("   ('clear' to reset)").nl();
  }
  out.str("target:   ").fixed(s.target_rpm, 1).str(" rpm   setpoint: ").fixed(s.setpoint_rpm, 1).str(" rpm").nl();
  out.str("speed:    ").fixed(s.speed_rpm, 1).str(" rpm").nl();
  out.str("duty:     ").fixed(s.duty * 100.0f, 1).str(" %").nl();
  out.str("current:  ").fixed(s.current_a, 3).str(" A").nl();
}

void duty(TextWriter& out, cli::Args args) {
  const auto percent = args.size() == 2 ? cli::parse_float(args[1]) : std::nullopt;
  if (!percent || *percent < -100.0f || *percent > 100.0f) {
    out.str("usage: duty <-100..100>   (open loop, percent)").nl();
    return;
  }
  ControlCommand command;
  command.type = ControlCommand::Type::kOpenLoop;
  command.value = *percent / 100.0f;
  send_or_report(out, command);
}

void speed(TextWriter& out, cli::Args args) {
  const auto rpm = args.size() == 2 ? cli::parse_float(args[1]) : std::nullopt;
  if (!rpm) {
    out.str("usage: speed <rpm>   (closed loop; negative = reverse)").nl();
    return;
  }
  ControlCommand command;
  command.type = ControlCommand::Type::kSpeed;
  command.value = *rpm;
  send_or_report(out, command);
}

void stop(TextWriter& out, cli::Args) {
  send_or_report(out, ControlCommand{});  // default command is kStop
}

void pid(TextWriter& out, cli::Args args) {
  if (args.size() == 1) {
    const control::PidGains g = control_task().snapshot().gains;
    out.str("kp ").fixed(g.kp, 5).str("  ki ").fixed(g.ki, 5).str("  kd ").fixed(g.kd, 6).nl();
    return;
  }
  const auto kp = args.size() == 4 ? cli::parse_float(args[1]) : std::nullopt;
  const auto ki = args.size() == 4 ? cli::parse_float(args[2]) : std::nullopt;
  const auto kd = args.size() == 4 ? cli::parse_float(args[3]) : std::nullopt;
  if (!kp || !ki || !kd || *kp < 0.0f || *ki < 0.0f || *kd < 0.0f) {
    out.str("usage: pid [kp ki kd]   (duty per rpm; non-negative)").nl();
    return;
  }
  ControlCommand command;
  command.type = ControlCommand::Type::kSetGains;
  command.gains = {.kp = *kp, .ki = *ki, .kd = *kd};
  send_or_report(out, command);
}

void sim(TextWriter& out, cli::Args args) {
  if (args.size() == 1) {
    out.str("motor io: ").str(control_task().snapshot().io_name).nl();
    return;
  }
  const bool on = args.size() == 2 && args[1] == "on";
  const bool off = args.size() == 2 && args[1] == "off";
  if (!on && !off) {
    out.str("usage: sim [on|off]").nl();
    return;
  }
  if (!is_stopped(control_task().snapshot().status)) {
    out.str("error: stop the motor first").nl();
    return;
  }
  ControlCommand command;
  command.type = ControlCommand::Type::kUseSimulation;
  command.flag = on;
  send_or_report(out, command);
}

void clear(TextWriter& out, cli::Args) {
  ControlCommand command;
  command.type = ControlCommand::Type::kClearFault;
  send_or_report(out, command);
}

void ctrl(TextWriter& out, cli::Args args) {
  if (args.size() == 2 && args[1] == "reset") {
    ControlCommand command;
    command.type = ControlCommand::Type::kResetTiming;
    send_or_report(out, command);
    return;
  }
  const ControlTiming t = control_task().timing();
  const float nominal_us = 1.0e6f / static_cast<float>(control_task().rate_hz());
  out.str("loops:        ").u32(t.loops).str("   missed ticks: ").u32(t.missed_ticks).nl();
  if (t.loops < 2U) {
    return;
  }
  out.str("period:       ").fixed(cycles_to_us(t.period_min_cycles), 2).str(" .. ")
      .fixed(cycles_to_us(t.period_max_cycles), 2).str(" us   (nominal ").fixed(nominal_us, 0).str(")").nl();
  out.str("jitter:       +/- ")
      .fixed((cycles_to_us(t.period_max_cycles) - cycles_to_us(t.period_min_cycles)) / 2.0f, 2)
      .str(" us").nl();
  out.str("irq->task:    ").fixed(cycles_to_us(t.latency_max_cycles), 2).str(" us max").nl();
  out.str("execution:    ").fixed(cycles_to_us(t.exec_total_cycles / t.loops), 2).str(" us avg, ")
      .fixed(cycles_to_us(t.exec_max_cycles), 2).str(" us max").nl();
}

void stream(TextWriter& out, cli::Args args) {
  if (args.size() == 1) {
    out.str("telemetry: ").u32(telemetry_task().rate_hz()).str(" Hz").nl();
    return;
  }
  if (args.size() == 2 && args[1] == "off") {
    telemetry_task().set_rate_hz(0);
    return;
  }
  const auto hz = args.size() == 2 ? cli::parse_u32(args[1]) : std::nullopt;
  if (!hz || *hz == 0U || *hz > TelemetryTask::kMaxRateHz) {
    out.str("usage: stream [1..").u32(TelemetryTask::kMaxRateHz).str(" | off]").nl();
    return;
  }
  telemetry_task().set_rate_hz(*hz);
}

}  // namespace app::motor_commands
