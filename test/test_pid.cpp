// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include "control/pid.hpp"

namespace {

constexpr float kDt = 0.001f;

control::PidConfig make_config(float kp, float ki, float kd) {
  control::PidConfig config;
  config.gains = {.kp = kp, .ki = ki, .kd = kd};
  return config;  // output limits -1..1, no derivative filter
}

}  // namespace

TEST(Pid, ProportionalTermScalesError) {
  control::Pid pid(make_config(0.5f, 0.0f, 0.0f));
  EXPECT_NEAR(pid.update(1.0f, 0.4f, kDt), 0.3f, 1e-6f);
}

TEST(Pid, OutputIsClampedToLimits) {
  control::Pid pid(make_config(10.0f, 0.0f, 0.0f));
  EXPECT_FLOAT_EQ(pid.update(1.0f, 0.0f, kDt), 1.0f);
  EXPECT_FLOAT_EQ(pid.update(-1.0f, 0.0f, kDt), -1.0f);
}

TEST(Pid, IntegralAccumulatesErrorOverTime) {
  control::Pid pid(make_config(0.0f, 10.0f, 0.0f));
  float out = 0.0f;
  for (int i = 0; i < 10; ++i) {
    out = pid.update(1.0f, 0.0f, kDt);
  }
  EXPECT_NEAR(out, 0.1f, 1e-5f);  // 10 steps x error 1 x 1 ms x ki 10
}

TEST(Pid, AntiWindupLetsOutputLeaveSaturationImmediately) {
  control::Pid pid(make_config(0.5f, 50.0f, 0.0f));
  for (int i = 0; i < 5000; ++i) {
    (void)pid.update(10.0f, 0.0f, kDt);  // 5 s pinned at the upper limit
  }
  EXPECT_LE(pid.integral(), 1.0f);
  // Error reverses: a wound-up integrator would hold the output high for seconds.
  EXPECT_LT(pid.update(0.0f, 1.0f, kDt), 0.0f);
}

TEST(Pid, SetpointStepCausesNoDerivativeKick) {
  control::Pid pid(make_config(0.0f, 0.0f, 1.0f));
  (void)pid.update(0.0f, 0.0f, kDt);
  EXPECT_FLOAT_EQ(pid.update(100.0f, 0.0f, kDt), 0.0f);
}

TEST(Pid, DerivativeOpposesMeasurementChange) {
  control::Pid pid(make_config(0.0f, 0.0f, 0.0005f));
  (void)pid.update(0.0f, 0.0f, kDt);
  EXPECT_NEAR(pid.update(0.0f, 1.0f, kDt), -0.5f, 1e-4f);  // -(1 - 0) / 1 ms x kd
}

TEST(Pid, FirstSampleHasNoDerivativeSpike) {
  control::Pid pid(make_config(0.0f, 0.0f, 1.0f));
  EXPECT_FLOAT_EQ(pid.update(0.0f, 50.0f, kDt), 0.0f);
}

TEST(Pid, RetuningKiDoesNotBumpOutput) {
  control::Pid pid(make_config(0.0f, 10.0f, 0.0f));
  for (int i = 0; i < 50; ++i) {
    (void)pid.update(1.0f, 0.0f, kDt);
  }
  const float before = pid.update(0.0f, 0.0f, kDt);
  pid.set_gains({.kp = 0.0f, .ki = 100.0f, .kd = 0.0f});
  EXPECT_FLOAT_EQ(pid.update(0.0f, 0.0f, kDt), before);
}

TEST(Pid, FeedforwardCountsTowardSaturation) {
  control::Pid pid(make_config(0.0f, 0.0f, 0.0f));
  EXPECT_FLOAT_EQ(pid.update(0.0f, 0.0f, kDt, 0.7f), 0.7f);
  EXPECT_FLOAT_EQ(pid.update(0.0f, 0.0f, kDt, 3.0f), 1.0f);
}

TEST(Pid, NonPositiveDtDoesNotIntegrate) {
  control::Pid pid(make_config(0.0f, 10.0f, 0.0f));
  (void)pid.update(1.0f, 0.0f, 0.0f);
  (void)pid.update(1.0f, 0.0f, -1.0f);
  EXPECT_FLOAT_EQ(pid.integral(), 0.0f);
}

TEST(Pid, ResetClearsIntegrator) {
  control::Pid pid(make_config(0.0f, 10.0f, 0.0f));
  for (int i = 0; i < 20; ++i) {
    (void)pid.update(1.0f, 0.0f, kDt);
  }
  pid.reset();
  EXPECT_FLOAT_EQ(pid.integral(), 0.0f);
}
