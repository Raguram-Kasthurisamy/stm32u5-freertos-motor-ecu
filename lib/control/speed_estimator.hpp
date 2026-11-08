// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace control {

// Shaft speed from a quadrature encoder count, sampled at a fixed rate.
//
// Differentiating over a single 1 ms period gives terrible resolution (one count of a
// 1320-count/rev encoder in 1 ms is 45 rpm). Instead the speed is the count difference
// across a window of the last N samples (the "M method"), which trades N/2 samples of
// delay for N times better resolution. Count differences use unsigned arithmetic, so the
// estimate stays correct when the hardware counter wraps around.
class SpeedEstimator {
 public:
  static constexpr std::size_t kMaxWindow = 32;

  SpeedEstimator(float counts_per_rev, std::size_t window);

  // Returns rpm. `count` is the raw counter value; it may wrap.
  float update(std::int32_t count, float dt);
  void reset();
  [[nodiscard]] float rpm() const { return rpm_; }

 private:
  float counts_per_rev_;
  std::size_t window_;
  std::array<std::uint32_t, kMaxWindow + 1> history_{};
  std::size_t head_ = 0;
  std::size_t filled_ = 0;
  float rpm_ = 0.0f;
};

}  // namespace control
