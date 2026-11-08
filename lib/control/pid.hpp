// Author: Raguram Kasthurisamy

#pragma once

namespace control {

struct PidGains {
  float kp = 0.0f;
  float ki = 0.0f;
  float kd = 0.0f;
};

struct PidConfig {
  PidGains gains{};
  float output_min = -1.0f;
  float output_max = 1.0f;
  float derivative_cutoff_hz = 0.0f;  // 0 = no filtering
};

// PID controller for a fixed-rate loop, written for real plants rather than textbooks:
//  - the derivative acts on the measurement, so a setpoint step does not kick the output;
//  - the derivative is low-pass filtered, because differentiating a quantized encoder
//    signal mostly amplifies noise;
//  - anti-windup by conditional integration: while the output is saturated, the
//    integrator stops growing in the direction that keeps it saturated;
//  - the integrator stores the integral *term* (ki already applied), so retuning ki
//    while running does not make the output jump;
//  - an optional feedforward term is included before saturation, so anti-windup sees
//    the real actuator command.
class Pid {
 public:
  explicit Pid(const PidConfig& config) : config_(config) {}

  [[nodiscard]] float update(float setpoint, float measurement, float dt, float feedforward = 0.0f);
  void reset();

  void set_gains(const PidGains& gains) { config_.gains = gains; }
  [[nodiscard]] const PidGains& gains() const { return config_.gains; }

  // Preloads the integral term, for bumpless transfer when the loop takes over.
  void set_integral(float value);
  [[nodiscard]] float integral() const { return integral_; }

 private:
  PidConfig config_;
  float integral_ = 0.0f;
  float derivative_filtered_ = 0.0f;
  float previous_measurement_ = 0.0f;
  bool primed_ = false;
};

}  // namespace control
