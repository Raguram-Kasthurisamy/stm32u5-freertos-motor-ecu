// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include <numbers>

#include "app/motor_config.hpp"
#include "control/dc_motor_model.hpp"

namespace {
constexpr float kDt = 0.001f;
const control::DcMotorParams& kParams = app::motor_config::kSimulatedMotor;
}  // namespace

TEST(DcMotorModel, ReachesAnalyticNoLoadSpeed) {
  control::DcMotorModel motor(kParams);
  for (int i = 0; i < 2000; ++i) {
    motor.step(1.0f, kDt);
  }
  // Steady state: Kt * (V - Kt w) / R = b w
  const float kt = kParams.torque_constant;
  const float omega = kParams.supply_v * kt / (kt * kt + kParams.resistance_ohm * kParams.viscous_friction);
  const float expected_rpm = omega * 60.0f / (2.0f * std::numbers::pi_v<float>) / kParams.gear_ratio;
  EXPECT_NEAR(motor.output_rpm(), expected_rpm, expected_rpm * 0.01f);
}

TEST(DcMotorModel, EncoderCountFollowsDirection) {
  control::DcMotorModel forward(kParams);
  control::DcMotorModel reverse(kParams);
  for (int i = 0; i < 200; ++i) {
    forward.step(0.5f, kDt);
    reverse.step(-0.5f, kDt);
  }
  EXPECT_GT(forward.encoder_count(), 0);
  EXPECT_LT(reverse.encoder_count(), 0);
}

TEST(DcMotorModel, StalledMotorDrawsSupplyOverResistance) {
  control::DcMotorModel motor(kParams);
  motor.set_load_torque(1.0f);  // far above stall torque
  for (int i = 0; i < 100; ++i) {
    motor.step(1.0f, kDt);
  }
  EXPECT_FLOAT_EQ(motor.output_rpm(), 0.0f);
  EXPECT_NEAR(motor.current_a(), kParams.supply_v / kParams.resistance_ohm, 1e-3f);
}

TEST(DcMotorModel, CoastingDrawsNoCurrent) {
  control::DcMotorModel motor(kParams);
  for (int i = 0; i < 500; ++i) {
    motor.step(1.0f, kDt);
  }
  const float spinning = motor.output_rpm();
  motor.step(0.0f, kDt, false);
  EXPECT_FLOAT_EQ(motor.current_a(), 0.0f);
  EXPECT_GT(motor.output_rpm(), 0.9f * spinning);  // coasts down slowly, no braking
}
