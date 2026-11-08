// Author: Raguram Kasthurisamy

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "protocol/ecu_protocol.hpp"

using protocol::CanFrame;

TEST(Crc8, MatchesStandardCheckValue) {
  const std::array<std::uint8_t, 9> ascii{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(protocol::crc8_sae_j1850(ascii), 0x4BU);  // published check value for SAE J1850
}

TEST(Protocol, CommandRoundTrips) {
  const protocol::Command command{.mode = protocol::CommandMode::kSpeed,
                                  .speed_rpm = -123.4f,
                                  .duty = 0.25f,
                                  .counter = 42};
  const CanFrame frame = protocol::encode(command);
  EXPECT_EQ(frame.id, protocol::kCommandId);
  EXPECT_EQ(frame.dlc, 8U);
  const auto decoded = protocol::decode_command(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->mode, protocol::CommandMode::kSpeed);
  EXPECT_NEAR(decoded->speed_rpm, -123.4f, 0.051f);
  EXPECT_NEAR(decoded->duty, 0.25f, 0.0001f);
  EXPECT_EQ(decoded->counter, 42U);
}

TEST(Protocol, CommandWithBadCrcIsRejected) {
  CanFrame frame = protocol::encode(protocol::Command{.mode = protocol::CommandMode::kSpeed,
                                                      .speed_rpm = 50.0f, .duty = 0.0f, .counter = 1});
  frame.data[1] ^= 0x01U;  // one flipped bit on the wire
  EXPECT_FALSE(protocol::decode_command(frame).has_value());
}

TEST(Protocol, CommandWithWrongIdOrLengthIsRejected) {
  CanFrame frame = protocol::encode(protocol::Command{});
  frame.dlc = 7;
  EXPECT_FALSE(protocol::decode_command(frame).has_value());
  frame = protocol::encode(protocol::Command{});
  frame.id = 0x101;
  EXPECT_FALSE(protocol::decode_command(frame).has_value());
}

TEST(Protocol, CommandWithUnknownModeIsRejected) {
  CanFrame frame = protocol::encode(protocol::Command{});
  frame.data[0] = 9;
  frame.data[7] = protocol::crc8_sae_j1850(std::span<const std::uint8_t>(frame.data.data(), 7));
  EXPECT_FALSE(protocol::decode_command(frame).has_value());
}

TEST(Protocol, StatusMatchesDbcByteLayout) {
  const protocol::Status status{.mode = 2, .fault = 0, .speed_rpm = 120.0f,
                                .setpoint_rpm = 118.4f, .duty = 0.6123f, .counter = 7};
  const CanFrame frame = protocol::encode(status);
  EXPECT_EQ(frame.id, protocol::kStatusId);
  // mode | fault<<4, speed 1200 = 0x04B0, setpoint 1184 = 0x04A0, duty 6123 = 0x17EB, counter
  const std::array<std::uint8_t, 8> expected{0x02, 0xB0, 0x04, 0xA0, 0x04, 0xEB, 0x17, 0x07};
  EXPECT_EQ(frame.data, expected);
}

TEST(Protocol, NegativeValuesUseTwosComplement) {
  const CanFrame frame = protocol::encode(protocol::Status{.mode = 2, .fault = 0, .speed_rpm = -100.0f,
                                                           .setpoint_rpm = 0.0f, .duty = 0.0f, .counter = 0});
  EXPECT_EQ(frame.data[1], 0x18U);  // -1000 = 0xFC18
  EXPECT_EQ(frame.data[2], 0xFCU);
  const auto decoded = protocol::decode_status(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_NEAR(decoded->speed_rpm, -100.0f, 0.001f);
}

TEST(Protocol, OutOfRangeValuesSaturate) {
  const CanFrame frame = protocol::encode(protocol::Status{.mode = 1, .fault = 3, .speed_rpm = 5000.0f,
                                                           .setpoint_rpm = -5000.0f, .duty = 4.0f, .counter = 0});
  const auto decoded = protocol::decode_status(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_NEAR(decoded->speed_rpm, 3276.7f, 0.01f);
  EXPECT_NEAR(decoded->setpoint_rpm, -3276.8f, 0.01f);
  EXPECT_NEAR(decoded->duty, 3.2767f, 0.001f);  // 16-bit limit in 0.01 % steps
  EXPECT_EQ(decoded->mode, 1U);
  EXPECT_EQ(decoded->fault, 3U);
}

TEST(Protocol, MeasurementsRoundTrip) {
  const CanFrame frame = protocol::encode(protocol::Measurements{.current_a = 1.234f,
                                                                 .target_rpm = -50.0f,
                                                                 .encoder_count = -1});
  EXPECT_EQ(frame.data[0], 0xD2U);  // 1234 mA = 0x04D2
  EXPECT_EQ(frame.data[1], 0x04U);
  const auto decoded = protocol::decode_measurements(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_NEAR(decoded->current_a, 1.234f, 0.0006f);
  EXPECT_NEAR(decoded->target_rpm, -50.0f, 0.001f);
  EXPECT_EQ(decoded->encoder_count, -1);
}

TEST(Protocol, HeartbeatRoundTrips) {
  const CanFrame frame = protocol::encode(protocol::Heartbeat{.state = 3, .reset_cause = 1,
                                                              .uptime_s = 123456U});
  EXPECT_EQ(frame.id, protocol::kHeartbeatId);
  const auto decoded = protocol::decode_heartbeat(frame);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->state, 3U);
  EXPECT_EQ(decoded->reset_cause, 1U);
  EXPECT_EQ(decoded->uptime_s, 123456U);
}
