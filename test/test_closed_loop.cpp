// Author: Raguram Kasthurisamy

// Validates the firmware's shipped motor_config.hpp against the simulated motor: these
// tests fail in CI if someone retunes the gains into something unstable or sluggish.
#include <gtest/gtest.h>

#include <algorithm>

#include "app/motor_config.hpp"
#include "control/simulated_motor_io.hpp"
#include "control/speed_controller.hpp"

namespace {

using app::motor_config::kControlPeriodS;
using app::motor_config::kController;
using app::motor_config::kSimulatedMotor;
using control::Fault;
using control::Mode;

class ClosedLoop : public ::testing::Test {
 protected:
  void run(int cycles) {
    for (int i = 0; i < cycles; ++i) {
      const control::Sample s = io.read();
      last_output = controller.update(s, kControlPeriodS);
      io.write(last_output.duty);
      io.set_enabled(last_output.driver_enabled);
    }
  }
  // True shaft speed from the model (no encoder quantization), averaged.
  float average_true_rpm(int cycles) {
    float sum = 0.0f;
    for (int i = 0; i < cycles; ++i) {
      run(1);
      sum += io.model().output_rpm();
    }
    return sum / static_cast<float>(cycles);
  }

  control::SimulatedMotorIo io{kSimulatedMotor, kControlPeriodS};
  control::SpeedController controller{kController};
  control::Output last_output{};
};

}  // namespace

TEST_F(ClosedLoop, StepResponseSettlesWithLimitedOvershoot) {
  ASSERT_TRUE(controller.set_speed(120.0f));
  float peak = 0.0f;
  for (int i = 0; i < 1000; ++i) {
    run(1);
    peak = std::max(peak, io.model().output_rpm());
  }
  EXPECT_LT(peak, 120.0f * 1.06f);
  EXPECT_NEAR(average_true_rpm(200), 120.0f, 1.0f);
  EXPECT_EQ(controller.status().mode, Mode::kSpeed);
}

TEST_F(ClosedLoop, RecoversFromLoadDisturbance) {
  ASSERT_TRUE(controller.set_speed(120.0f));
  run(1000);
  io.model().set_load_torque(0.015f);  // a quarter of the motor's stall torque, applied instantly
  run(800);
  EXPECT_NEAR(average_true_rpm(300), 120.0f, 1.0f);
  EXPECT_EQ(controller.status().mode, Mode::kSpeed);
}

TEST_F(ClosedLoop, TracksReverseSpeed) {
  ASSERT_TRUE(controller.set_speed(-100.0f));
  run(1200);
  EXPECT_NEAR(average_true_rpm(200), -100.0f, 1.0f);
}

TEST_F(ClosedLoop, TakeoverFromOpenLoopIsBumpless) {
  ASSERT_TRUE(controller.set_open_loop(0.5f));
  run(1000);
  const float duty_before = controller.status().duty;
  ASSERT_TRUE(controller.set_speed(controller.status().speed_rpm));
  run(1);
  EXPECT_NEAR(controller.status().duty, duty_before, 0.05f);
}

TEST_F(ClosedLoop, StalledMotorTripsOvercurrent) {
  io.model().set_load_torque(1.0f);  // jammed shaft
  ASSERT_TRUE(controller.set_open_loop(1.0f));
  run(100);
  EXPECT_EQ(controller.status().fault, Fault::kOvercurrent);
  EXPECT_FALSE(last_output.driver_enabled);
}

TEST_F(ClosedLoop, DisconnectedEncoderTripsInsteadOfRunningAway) {
  io.set_encoder_fault(control::EncoderFault::kDisconnected);
  ASSERT_TRUE(controller.set_speed(120.0f));
  run(1000);
  EXPECT_EQ(controller.status().fault, Fault::kNoFeedback);
  EXPECT_FALSE(last_output.driver_enabled);
}

TEST_F(ClosedLoop, ReversedEncoderTripsInsteadOfRunningAway) {
  io.set_encoder_fault(control::EncoderFault::kInverted);
  ASSERT_TRUE(controller.set_speed(120.0f));
  run(1000);
  EXPECT_EQ(controller.status().fault, Fault::kEncoderReversed);
  EXPECT_FALSE(last_output.driver_enabled);
}
