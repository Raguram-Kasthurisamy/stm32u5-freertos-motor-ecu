// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

namespace control {

// Brushed DC gearmotor parameters, all at the motor shaft unless stated otherwise.
struct DcMotorParams {
  float supply_v = 12.0f;
  float resistance_ohm = 4.0f;
  float torque_constant = 0.02f;   // N·m/A, numerically equal to the back-EMF constant in V·s/rad
  float inertia = 2.0e-6f;         // kg·m² (rotor + reflected load)
  float viscous_friction = 1.0e-6f;  // N·m·s/rad
  float gear_ratio = 30.0f;          // motor revolutions per output revolution
  float counts_per_motor_rev = 44.0f;  // encoder counts with 4x decoding
};

// A simulated DC motor with an encoder, for host tests and for running the real control
// loop on a board with no motor attached. Electrical inductance is neglected (its time
// constant is far below the 1 ms control period); the mechanics are integrated with
// sub-steps. The load torque behaves like friction: it opposes motion and holds a stalled
// shaft until the drive torque exceeds it.
class DcMotorModel {
 public:
  explicit DcMotorModel(const DcMotorParams& params) : params_(params) {}

  // Advances the model by dt seconds. driven = false models a sleeping H-bridge (coast).
  void step(float duty, float dt, bool driven = true);
  void set_load_torque(float newton_metres) { load_ = newton_metres < 0.0f ? 0.0f : newton_metres; }
  void reset();

  [[nodiscard]] std::int32_t encoder_count() const { return static_cast<std::int32_t>(counts_); }
  [[nodiscard]] float current_a() const { return current_ < 0.0f ? -current_ : current_; }
  [[nodiscard]] float output_rpm() const;
  [[nodiscard]] const DcMotorParams& params() const { return params_; }

 private:
  void advance_encoder(float angle_rad);

  DcMotorParams params_;
  float omega_ = 0.0f;  // rad/s at the motor shaft
  float current_ = 0.0f;
  float load_ = 0.0f;
  std::uint32_t counts_ = 0;
  float count_fraction_ = 0.0f;
};

}  // namespace control
