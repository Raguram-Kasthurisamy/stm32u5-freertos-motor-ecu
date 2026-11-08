// Author: Raguram Kasthurisamy

#include "control/pid.hpp"

#include <algorithm>
#include <numbers>

namespace control {
namespace {

// Smoothing factor of a first-order low-pass filter sampled every dt seconds.
float lowpass_alpha(float cutoff_hz, float dt) {
  if (cutoff_hz <= 0.0f) {
    return 1.0f;
  }
  const float rc = 1.0f / (2.0f * std::numbers::pi_v<float> * cutoff_hz);
  return dt / (rc + dt);
}

}  // namespace

void Pid::reset() {
  integral_ = 0.0f;
  derivative_filtered_ = 0.0f;
  primed_ = false;
}

void Pid::set_integral(float value) {
  integral_ = std::clamp(value, config_.output_min, config_.output_max);
}

float Pid::update(float setpoint, float measurement, float dt, float feedforward) {
  if (!(dt > 0.0f)) {  // also rejects NaN
    return std::clamp(feedforward + integral_, config_.output_min, config_.output_max);
  }

  const float error = setpoint - measurement;
  const float proportional = config_.gains.kp * error;

  if (!primed_) {
    previous_measurement_ = measurement;  // no derivative spike on the first sample
    primed_ = true;
  }
  const float derivative_raw = -(measurement - previous_measurement_) / dt;
  previous_measurement_ = measurement;
  derivative_filtered_ +=
      lowpass_alpha(config_.derivative_cutoff_hz, dt) * (derivative_raw - derivative_filtered_);
  const float derivative = config_.gains.kd * derivative_filtered_;

  const float candidate = std::clamp(integral_ + config_.gains.ki * error * dt,
                                     config_.output_min, config_.output_max);
  const float unsaturated = feedforward + proportional + candidate + derivative;
  const bool winding_up = unsaturated > config_.output_max && error > 0.0f;
  const bool winding_down = unsaturated < config_.output_min && error < 0.0f;
  if (!winding_up && !winding_down) {
    integral_ = candidate;
  }

  return std::clamp(feedforward + proportional + integral_ + derivative, config_.output_min,
                    config_.output_max);
}

}  // namespace control
