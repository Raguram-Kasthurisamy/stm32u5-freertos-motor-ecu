// Author: Raguram Kasthurisamy

#pragma once

#include <algorithm>

namespace control {

// Limits how fast a value may change. Used to ramp the speed setpoint, so a step command
// does not demand a current spike from the motor and supply.
class SlewLimiter {
 public:
  explicit SlewLimiter(float max_rate_per_s) : max_rate_(max_rate_per_s) {}

  float update(float target, float dt) {
    const float max_step = max_rate_ * dt;
    value_ += std::clamp(target - value_, -max_step, max_step);
    return value_;
  }
  void reset(float value) { value_ = value; }
  [[nodiscard]] float value() const { return value_; }

 private:
  float max_rate_;
  float value_ = 0.0f;
};

}  // namespace control
