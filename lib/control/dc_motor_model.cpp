// Author: Raguram Kasthurisamy

#include "control/dc_motor_model.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace control {
namespace {

constexpr int kSubsteps = 4;
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr float kStandstillRadPerS = 1.0e-3f;

}  // namespace

void DcMotorModel::reset() {
  omega_ = 0.0f;
  current_ = 0.0f;
  counts_ = 0;
  count_fraction_ = 0.0f;
}

float DcMotorModel::output_rpm() const {
  return omega_ * 60.0f / kTwoPi / params_.gear_ratio;
}

void DcMotorModel::step(float duty, float dt, bool driven) {
  const float h = dt / static_cast<float>(kSubsteps);
  const float voltage = std::clamp(duty, -1.0f, 1.0f) * params_.supply_v;

  for (int i = 0; i < kSubsteps; ++i) {
    current_ = driven ? (voltage - params_.torque_constant * omega_) / params_.resistance_ohm : 0.0f;
    const float drive = params_.torque_constant * current_ - params_.viscous_friction * omega_;

    if (std::fabs(omega_) < kStandstillRadPerS && std::fabs(drive) <= load_) {
      omega_ = 0.0f;  // the load holds the shaft
    } else {
      const float direction = std::fabs(omega_) < kStandstillRadPerS ? drive : omega_;
      const float net = drive - std::copysign(load_, direction);
      const float next = omega_ + net / params_.inertia * h;
      // Friction-like load can stop the shaft but never reverse it within a step.
      const bool reversed = std::signbit(next) != std::signbit(omega_) &&
                            std::fabs(omega_) >= kStandstillRadPerS && std::fabs(drive) <= load_;
      omega_ = reversed ? 0.0f : next;
    }
    advance_encoder(omega_ * h);
  }
}

void DcMotorModel::advance_encoder(float angle_rad) {
  const float counts = angle_rad / kTwoPi * params_.counts_per_motor_rev + count_fraction_;
  const float whole = std::floor(counts);
  count_fraction_ = counts - whole;
  // Unsigned addition wraps exactly like a hardware counter.
  counts_ += static_cast<std::uint32_t>(static_cast<std::int32_t>(whole));
}

}  // namespace control
