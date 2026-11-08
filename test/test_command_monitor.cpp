// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include "protocol/command_monitor.hpp"

namespace {

using protocol::CommandMode;
using Result = protocol::CommandMonitor::Result;

protocol::CanFrame command_frame(CommandMode mode, std::uint8_t counter) {
  return protocol::encode(protocol::Command{.mode = mode, .speed_rpm = 100.0f, .duty = 0.0f,
                                            .counter = counter});
}

}  // namespace

TEST(CommandMonitor, AcceptsValidCommandAndStartsSession) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  EXPECT_EQ(monitor.on_frame(command_frame(CommandMode::kSpeed, 1), 0, command), Result::kAccepted);
  EXPECT_TRUE(monitor.active());
  EXPECT_EQ(command.mode, CommandMode::kSpeed);
}

TEST(CommandMonitor, RejectsRepeatedAliveCounter) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  (void)monitor.on_frame(command_frame(CommandMode::kSpeed, 5), 0, command);
  EXPECT_EQ(monitor.on_frame(command_frame(CommandMode::kSpeed, 5), 10, command), Result::kRepeated);
  EXPECT_EQ(monitor.stats().repeated, 1U);
}

TEST(CommandMonitor, FrozenSenderTimesOutEvenWhileStillTransmitting) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  (void)monitor.on_frame(command_frame(CommandMode::kSpeed, 9), 0, command);
  for (std::uint32_t t = 10; t <= 300; t += 10) {
    (void)monitor.on_frame(command_frame(CommandMode::kSpeed, 9), t, command);  // stuck counter
  }
  EXPECT_TRUE(monitor.check_timeout(300));
  EXPECT_FALSE(monitor.active());
}

TEST(CommandMonitor, TimeoutIsReportedOnce) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  (void)monitor.on_frame(command_frame(CommandMode::kOpenLoop, 1), 1000, command);
  EXPECT_FALSE(monitor.check_timeout(1250));
  EXPECT_TRUE(monitor.check_timeout(1251));
  EXPECT_FALSE(monitor.check_timeout(1400));
  EXPECT_EQ(monitor.stats().timeouts, 1U);
}

TEST(CommandMonitor, StopCommandEndsSessionWithoutTimeout) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  (void)monitor.on_frame(command_frame(CommandMode::kSpeed, 1), 0, command);
  (void)monitor.on_frame(command_frame(CommandMode::kStop, 2), 10, command);
  EXPECT_FALSE(monitor.active());
  EXPECT_FALSE(monitor.check_timeout(10'000));
}

TEST(CommandMonitor, CountsInvalidFrames) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  protocol::CanFrame frame = command_frame(CommandMode::kSpeed, 1);
  frame.data[7] ^= 0xFFU;
  EXPECT_EQ(monitor.on_frame(frame, 0, command), Result::kInvalid);
  EXPECT_FALSE(monitor.active());
  EXPECT_EQ(monitor.stats().invalid, 1U);
}

TEST(CommandMonitor, TimeoutWorksAcrossMillisecondWrap) {
  protocol::CommandMonitor monitor(250);
  protocol::Command command;
  (void)monitor.on_frame(command_frame(CommandMode::kSpeed, 1), 0xFFFF'FF80U, command);
  EXPECT_FALSE(monitor.check_timeout(0x0000'0010U));  // 144 ms after the counter wrapped
  EXPECT_TRUE(monitor.check_timeout(0x0000'0080U));   // 256 ms: past the 250 ms limit
}
