// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include "app/motor_config.hpp"
#include "control/speed_controller.hpp"

namespace {

using control::Fault;
using control::Mode;
constexpr float kDt = app::motor_config::kControlPeriodS;
const control::ControllerConfig& kConfig = app::motor_config::kController;

control::Sample sample(std::int32_t count = 0, float current = 0.0f, bool driver_fault = false) {
  return control::Sample{.encoder_count = count, .current_a = current, .driver_fault = driver_fault};
}

}  // namespace

TEST(SpeedController, StartsDisabledWithDriverOff) {
  control::SpeedController controller(kConfig);
  const control::Output out = controller.update(sample(), kDt);
  EXPECT_EQ(controller.status().mode, Mode::kDisabled);
  EXPECT_FALSE(out.driver_enabled);
  EXPECT_FLOAT_EQ(out.duty, 0.0f);
}

TEST(SpeedController, OpenLoopAppliesClampedDuty) {
  control::SpeedController controller(kConfig);
  ASSERT_TRUE(controller.set_open_loop(0.3f));
  control::Output out = controller.update(sample(), kDt);
  EXPECT_TRUE(out.driver_enabled);
  EXPECT_FLOAT_EQ(out.duty, 0.3f);
  ASSERT_TRUE(controller.set_open_loop(2.0f));
  out = controller.update(sample(), kDt);
  EXPECT_FLOAT_EQ(out.duty, 1.0f);
}

TEST(SpeedController, DriverFaultTripsAndStaysLatched) {
  control::SpeedController controller(kConfig);
  ASSERT_TRUE(controller.set_open_loop(0.5f));
  const control::Output out = controller.update(sample(0, 0.0f, true), kDt);
  EXPECT_EQ(controller.status().mode, Mode::kFault);
  EXPECT_EQ(controller.status().fault, Fault::kDriverFault);
  EXPECT_FALSE(out.driver_enabled);
  EXPECT_FLOAT_EQ(out.duty, 0.0f);

  EXPECT_FALSE(controller.set_open_loop(0.5f));  // commands refused while faulted
  controller.stop();                              // stop does not clear a fault
  EXPECT_EQ(controller.status().mode, Mode::kFault);
  EXPECT_FALSE(controller.clear_fault());         // driver still reports the fault

  (void)controller.update(sample(0, 0.0f, false), kDt);
  EXPECT_TRUE(controller.clear_fault());
  EXPECT_EQ(controller.status().mode, Mode::kDisabled);
  EXPECT_EQ(controller.status().fault, Fault::kNone);
}

TEST(SpeedController, OvercurrentMustPersistBeforeTripping) {
  control::SpeedController controller(kConfig);
  ASSERT_TRUE(controller.set_open_loop(0.2f));
  const float high = kConfig.overcurrent_a + 0.5f;
  for (std::uint32_t i = 0; i + 1 < kConfig.overcurrent_cycles; ++i) {
    (void)controller.update(sample(0, high), kDt);
  }
  EXPECT_EQ(controller.status().mode, Mode::kOpenLoop);
  (void)controller.update(sample(0, 0.0f), kDt);  // one normal sample resets the count
  for (std::uint32_t i = 0; i + 1 < kConfig.overcurrent_cycles; ++i) {
    (void)controller.update(sample(0, high), kDt);
  }
  EXPECT_EQ(controller.status().mode, Mode::kOpenLoop);
  (void)controller.update(sample(0, high), kDt);
  EXPECT_EQ(controller.status().fault, Fault::kOvercurrent);
}

TEST(SpeedController, SpeedTargetIsClampedToMaximum) {
  control::SpeedController controller(kConfig);
  ASSERT_TRUE(controller.set_speed(10'000.0f));
  EXPECT_FLOAT_EQ(controller.status().target_rpm, kConfig.max_rpm);
  ASSERT_TRUE(controller.set_speed(-10'000.0f));
  EXPECT_FLOAT_EQ(controller.status().target_rpm, -kConfig.max_rpm);
}

TEST(SpeedController, SetpointRampsAtConfiguredRate) {
  control::SpeedController controller(kConfig);
  ASSERT_TRUE(controller.set_speed(100.0f));
  for (int i = 0; i < 10; ++i) {
    (void)controller.update(sample(), kDt);
  }
  EXPECT_NEAR(controller.status().setpoint_rpm, kConfig.ramp_rpm_per_s * 10.0f * kDt, 1e-3f);
}

TEST(SpeedController, StopDisablesDriver) {
  control::SpeedController controller(kConfig);
  ASSERT_TRUE(controller.set_speed(50.0f));
  (void)controller.update(sample(), kDt);
  controller.stop();
  const control::Output out = controller.update(sample(), kDt);
  EXPECT_EQ(controller.status().mode, Mode::kDisabled);
  EXPECT_FALSE(out.driver_enabled);
  EXPECT_FLOAT_EQ(controller.status().target_rpm, 0.0f);
}
