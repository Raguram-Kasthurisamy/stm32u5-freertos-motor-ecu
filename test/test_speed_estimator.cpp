// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include <cstdint>

#include "control/speed_estimator.hpp"

namespace {
constexpr float kDt = 0.001f;
constexpr float kCountsPerRev = 1000.0f;  // 5 counts per ms = 5 rev/s = 300 rpm
}  // namespace

TEST(SpeedEstimator, MeasuresConstantRate) {
  control::SpeedEstimator estimator(kCountsPerRev, 10);
  std::uint32_t count = 0;
  float rpm = 0.0f;
  for (int i = 0; i < 50; ++i) {
    count += 5;
    rpm = estimator.update(static_cast<std::int32_t>(count), kDt);
  }
  EXPECT_NEAR(rpm, 300.0f, 1e-3f);
}

TEST(SpeedEstimator, MeasuresReverseDirection) {
  control::SpeedEstimator estimator(kCountsPerRev, 10);
  std::uint32_t count = 0;
  float rpm = 0.0f;
  for (int i = 0; i < 50; ++i) {
    count -= 5;
    rpm = estimator.update(static_cast<std::int32_t>(count), kDt);
  }
  EXPECT_NEAR(rpm, -300.0f, 1e-3f);
}

TEST(SpeedEstimator, StaysCorrectAcrossCounterWrapAround) {
  control::SpeedEstimator estimator(kCountsPerRev, 10);
  std::uint32_t count = 0x7FFF'FFE0U;  // just below INT32_MAX: wraps to negative mid-test
  float rpm = 0.0f;
  for (int i = 0; i < 50; ++i) {
    count += 5;
    rpm = estimator.update(static_cast<std::int32_t>(count), kDt);
  }
  EXPECT_NEAR(rpm, 300.0f, 1e-3f);
}

TEST(SpeedEstimator, UsesAvailableHistoryBeforeWindowFills) {
  control::SpeedEstimator estimator(kCountsPerRev, 10);
  EXPECT_FLOAT_EQ(estimator.update(0, kDt), 0.0f);  // no history yet
  EXPECT_NEAR(estimator.update(5, kDt), 300.0f, 1e-3f);
}

TEST(SpeedEstimator, WiderWindowGivesFinerResolution) {
  // One count in the window: resolution = 60 / (counts_per_rev x window x dt).
  control::SpeedEstimator narrow(kCountsPerRev, 1);
  control::SpeedEstimator wide(kCountsPerRev, 20);
  for (int i = 0; i <= 20; ++i) {
    (void)narrow.update(i == 20 ? 1 : 0, kDt);
    (void)wide.update(i == 20 ? 1 : 0, kDt);
  }
  EXPECT_NEAR(narrow.rpm(), 60.0f, 1e-3f);
  EXPECT_NEAR(wide.rpm(), 3.0f, 1e-3f);
}

TEST(SpeedEstimator, ResetForgetsHistory) {
  control::SpeedEstimator estimator(kCountsPerRev, 10);
  (void)estimator.update(0, kDt);
  (void)estimator.update(5, kDt);
  estimator.reset();
  EXPECT_FLOAT_EQ(estimator.update(1000, kDt), 0.0f);
}
