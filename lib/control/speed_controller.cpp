// Author: Raguram Kasthurisamy

#include "control/speed_controller.hpp"

#include <algorithm>
#include <cmath>

namespace control {

const char* to_string(Mode mode) {
  switch (mode) {
    case Mode::kDisabled: return "disabled";
    case Mode::kOpenLoop: return "open-loop";
    case Mode::kSpeed: return "speed";
    case Mode::kFault: return "FAULT";
  }
  return "?";
}

const char* to_string(Fault fault) {
  switch (fault) {
    case Fault::kNone: return "none";
    case Fault::kOvercurrent: return "overcurrent";
    case Fault::kDriverFault: return "driver fault";
    case Fault::kNoFeedback: return "no feedback (stalled or encoder lost)";
    case Fault::kEncoderReversed: return "encoder reversed";
    case Fault::kTickLost: return "control tick lost";
  }
  return "?";
}

SpeedController::SpeedController(const ControllerConfig& config)
    : config_(config),
      pid_(config.pid),
      speed_(config.counts_per_output_rev, config.speed_window),
      ramp_(config.ramp_rpm_per_s) {}

void SpeedController::reset_counters() {
  overcurrent_count_ = 0;
  no_feedback_count_ = 0;
  reversed_count_ = 0;
}

void SpeedController::stop() {
  if (status_.mode == Mode::kFault) {
    return;  // a fault stays latched until clear_fault()
  }
  status_.mode = Mode::kDisabled;
  status_.target_rpm = 0.0f;
  status_.setpoint_rpm = 0.0f;
  open_loop_duty_ = 0.0f;
  pid_.reset();
  reset_counters();
}

bool SpeedController::set_open_loop(float duty) {
  if (status_.mode == Mode::kFault) {
    return false;
  }
  open_loop_duty_ = std::clamp(duty, -1.0f, 1.0f);
  status_.mode = Mode::kOpenLoop;
  status_.target_rpm = 0.0f;
  status_.setpoint_rpm = 0.0f;
  return true;
}

bool SpeedController::set_speed(float rpm) {
  if (status_.mode == Mode::kFault) {
    return false;
  }
  status_.target_rpm = std::clamp(rpm, -config_.max_rpm, config_.max_rpm);
  if (status_.mode != Mode::kSpeed) {
    // Bumpless transfer: start the ramp from the measured speed and preload the integrator
    // with whatever duty is applied right now, so taking over does not jolt the motor.
    ramp_.reset(status_.speed_rpm);
    pid_.reset();
    pid_.set_integral(status_.duty - config_.feedforward_duty_per_rpm * status_.speed_rpm);
    status_.mode = Mode::kSpeed;
  }
  return true;
}

bool SpeedController::clear_fault() {
  if (status_.mode != Mode::kFault) {
    return true;
  }
  if (driver_fault_present_) {
    return false;
  }
  status_.mode = Mode::kDisabled;
  status_.fault = Fault::kNone;
  reset_counters();
  return true;
}

void SpeedController::trip(Fault fault) {
  status_.mode = Mode::kFault;
  status_.fault = fault;
  status_.target_rpm = 0.0f;
  status_.setpoint_rpm = 0.0f;
  open_loop_duty_ = 0.0f;
  pid_.reset();
  reset_counters();
}

void SpeedController::check_protections(const Sample& sample) {
  if (status_.mode == Mode::kFault) {
    return;
  }
  if (sample.driver_fault) {
    trip(Fault::kDriverFault);
    return;
  }

  overcurrent_count_ = sample.current_a > config_.overcurrent_a ? overcurrent_count_ + 1 : 0;
  if (overcurrent_count_ >= config_.overcurrent_cycles) {
    trip(Fault::kOvercurrent);
    return;
  }

  // Feedback plausibility, judged against the duty applied during the period just measured.
  const bool driving = (status_.mode == Mode::kOpenLoop || status_.mode == Mode::kSpeed) &&
                       std::fabs(status_.duty) >= config_.feedback_check_duty;
  if (!driving) {
    no_feedback_count_ = 0;
    reversed_count_ = 0;
    return;
  }
  const bool moving = std::fabs(status_.speed_rpm) >= config_.feedback_min_rpm;
  const bool against_drive = moving && (status_.speed_rpm * status_.duty) < 0.0f;
  no_feedback_count_ = moving ? 0 : no_feedback_count_ + 1;
  reversed_count_ = against_drive ? reversed_count_ + 1 : 0;
  if (no_feedback_count_ >= config_.feedback_cycles) {
    trip(Fault::kNoFeedback);
  } else if (reversed_count_ >= config_.feedback_cycles) {
    trip(Fault::kEncoderReversed);
  }
}

Output SpeedController::update(const Sample& sample, float dt) {
  status_.speed_rpm = speed_.update(sample.encoder_count, dt);
  status_.current_a = sample.current_a;
  status_.encoder_count = sample.encoder_count;
  driver_fault_present_ = sample.driver_fault;
  check_protections(sample);

  float duty = 0.0f;
  switch (status_.mode) {
    case Mode::kDisabled:
    case Mode::kFault:
      break;
    case Mode::kOpenLoop:
      duty = open_loop_duty_;
      break;
    case Mode::kSpeed: {
      status_.setpoint_rpm = ramp_.update(status_.target_rpm, dt);
      const float feedforward = config_.feedforward_duty_per_rpm * status_.setpoint_rpm;
      duty = pid_.update(status_.setpoint_rpm, status_.speed_rpm, dt, feedforward);
      break;
    }
  }
  status_.duty = duty;
  const bool driving = status_.mode == Mode::kOpenLoop || status_.mode == Mode::kSpeed;
  return Output{.duty = duty, .driver_enabled = driving};
}

}  // namespace control
