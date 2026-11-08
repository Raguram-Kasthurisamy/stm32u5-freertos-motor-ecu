// Author: Raguram Kasthurisamy

#include "control/speed_estimator.hpp"

#include <algorithm>

namespace control {

SpeedEstimator::SpeedEstimator(float counts_per_rev, std::size_t window)
    : counts_per_rev_(counts_per_rev), window_(std::clamp<std::size_t>(window, 1, kMaxWindow)) {}

void SpeedEstimator::reset() {
  head_ = 0;
  filled_ = 0;
  rpm_ = 0.0f;
}

float SpeedEstimator::update(std::int32_t count, float dt) {
  const auto now = static_cast<std::uint32_t>(count);
  history_[head_] = now;

  const std::size_t span = std::min(filled_, window_);  // samples between oldest and now
  if (span > 0 && dt > 0.0f && counts_per_rev_ > 0.0f) {
    const std::size_t oldest = (head_ + history_.size() - span) % history_.size();
    // Unsigned subtraction then conversion: correct across counter wrap-around (C++20).
    const auto delta = static_cast<std::int32_t>(now - history_[oldest]);
    const float revolutions = static_cast<float>(delta) / counts_per_rev_;
    rpm_ = revolutions / (static_cast<float>(span) * dt) * 60.0f;
  }

  head_ = (head_ + 1) % history_.size();
  if (filled_ < window_) {
    ++filled_;
  }
  return rpm_;
}

}  // namespace control
