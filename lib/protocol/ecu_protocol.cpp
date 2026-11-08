// Author: Raguram Kasthurisamy

#include "protocol/ecu_protocol.hpp"

#include <algorithm>
#include <cmath>

namespace protocol {
namespace {

using Bytes = std::array<std::uint8_t, 8>;

// Writes `length` bits of `value` starting at bit `start` (Intel numbering: bit 0 is the
// least significant bit of byte 0).
void put_bits(Bytes& data, unsigned start, unsigned length, std::uint64_t value) {
  for (unsigned i = 0; i < length; ++i) {
    const unsigned bit = start + i;
    const auto mask = static_cast<std::uint8_t>(1U << (bit % 8U));
    if (((value >> i) & 1U) != 0U) {
      data[bit / 8U] = static_cast<std::uint8_t>(data[bit / 8U] | mask);
    } else {
      data[bit / 8U] = static_cast<std::uint8_t>(data[bit / 8U] & static_cast<std::uint8_t>(~mask));
    }
  }
}

std::uint64_t get_bits(const Bytes& data, unsigned start, unsigned length) {
  std::uint64_t value = 0;
  for (unsigned i = 0; i < length; ++i) {
    const unsigned bit = start + i;
    if (((static_cast<unsigned>(data[bit / 8U]) >> (bit % 8U)) & 1U) != 0U) {
      value |= std::uint64_t{1} << i;
    }
  }
  return value;
}

std::int64_t sign_extend(std::uint64_t raw, unsigned length) {
  const std::uint64_t sign = std::uint64_t{1} << (length - 1U);
  return static_cast<std::int64_t>((raw ^ sign) - sign);
}

std::uint64_t to_raw_signed(float value, float factor, unsigned bits) {
  const float max = static_cast<float>((std::int64_t{1} << (bits - 1U)) - 1);
  const float min = -static_cast<float>(std::int64_t{1} << (bits - 1U));
  float scaled = std::round(value / factor);
  if (std::isnan(scaled)) {
    scaled = 0.0f;
  }
  scaled = std::clamp(scaled, min, max);
  const auto raw = static_cast<std::int64_t>(scaled);
  return static_cast<std::uint64_t>(raw) & ((std::uint64_t{1} << bits) - 1U);
}

std::uint64_t to_raw_unsigned(float value, float factor, unsigned bits) {
  const float max = static_cast<float>((std::uint64_t{1} << bits) - 1U);
  float scaled = std::round(value / factor);
  if (std::isnan(scaled)) {
    scaled = 0.0f;
  }
  return static_cast<std::uint64_t>(std::clamp(scaled, 0.0f, max));
}

float from_raw_signed(std::uint64_t raw, unsigned bits, float factor) {
  return static_cast<float>(sign_extend(raw, bits)) * factor;
}

CanFrame make_frame(std::uint32_t id, const Bytes& data) {
  return CanFrame{.id = id, .dlc = 8, .data = data};
}

bool matches(const CanFrame& frame, std::uint32_t id) { return frame.id == id && frame.dlc == 8; }

}  // namespace

std::uint8_t crc8_sae_j1850(std::span<const std::uint8_t> bytes) {
  std::uint8_t crc = 0xFF;
  for (const std::uint8_t byte : bytes) {
    crc = static_cast<std::uint8_t>(crc ^ byte);
    for (int i = 0; i < 8; ++i) {
      const unsigned shifted = static_cast<unsigned>(crc) << 1U;
      crc = static_cast<std::uint8_t>((crc & 0x80U) != 0U ? (shifted ^ 0x1DU) : shifted);
    }
  }
  return static_cast<std::uint8_t>(crc ^ 0xFFU);
}

CanFrame encode(const Command& command) {
  Bytes data{};
  put_bits(data, 0, 8, static_cast<std::uint8_t>(command.mode));
  put_bits(data, 8, 16, to_raw_signed(command.speed_rpm, 0.1f, 16));
  put_bits(data, 24, 16, to_raw_signed(command.duty * 100.0f, 0.01f, 16));
  put_bits(data, 40, 8, command.counter);
  data[7] = crc8_sae_j1850(std::span<const std::uint8_t>(data.data(), 7));
  return make_frame(kCommandId, data);
}

std::optional<Command> decode_command(const CanFrame& frame) {
  if (!matches(frame, kCommandId)) {
    return std::nullopt;
  }
  if (crc8_sae_j1850(std::span<const std::uint8_t>(frame.data.data(), 7)) != frame.data[7]) {
    return std::nullopt;
  }
  const auto mode = static_cast<std::uint8_t>(get_bits(frame.data, 0, 8));
  if (mode > static_cast<std::uint8_t>(CommandMode::kClearFault)) {
    return std::nullopt;
  }
  return Command{
      .mode = static_cast<CommandMode>(mode),
      .speed_rpm = from_raw_signed(get_bits(frame.data, 8, 16), 16, 0.1f),
      .duty = from_raw_signed(get_bits(frame.data, 24, 16), 16, 0.01f) / 100.0f,
      .counter = static_cast<std::uint8_t>(get_bits(frame.data, 40, 8)),
  };
}

CanFrame encode(const Status& status) {
  Bytes data{};
  put_bits(data, 0, 4, status.mode);
  put_bits(data, 4, 4, status.fault);
  put_bits(data, 8, 16, to_raw_signed(status.speed_rpm, 0.1f, 16));
  put_bits(data, 24, 16, to_raw_signed(status.setpoint_rpm, 0.1f, 16));
  put_bits(data, 40, 16, to_raw_signed(status.duty * 100.0f, 0.01f, 16));
  put_bits(data, 56, 8, status.counter);
  return make_frame(kStatusId, data);
}

std::optional<Status> decode_status(const CanFrame& frame) {
  if (!matches(frame, kStatusId)) {
    return std::nullopt;
  }
  return Status{
      .mode = static_cast<std::uint8_t>(get_bits(frame.data, 0, 4)),
      .fault = static_cast<std::uint8_t>(get_bits(frame.data, 4, 4)),
      .speed_rpm = from_raw_signed(get_bits(frame.data, 8, 16), 16, 0.1f),
      .setpoint_rpm = from_raw_signed(get_bits(frame.data, 24, 16), 16, 0.1f),
      .duty = from_raw_signed(get_bits(frame.data, 40, 16), 16, 0.01f) / 100.0f,
      .counter = static_cast<std::uint8_t>(get_bits(frame.data, 56, 8)),
  };
}

CanFrame encode(const Measurements& measurements) {
  Bytes data{};
  put_bits(data, 0, 16, to_raw_unsigned(measurements.current_a, 0.001f, 16));
  put_bits(data, 16, 16, to_raw_signed(measurements.target_rpm, 0.1f, 16));
  put_bits(data, 32, 32, static_cast<std::uint32_t>(measurements.encoder_count));
  return make_frame(kMeasurementsId, data);
}

std::optional<Measurements> decode_measurements(const CanFrame& frame) {
  if (!matches(frame, kMeasurementsId)) {
    return std::nullopt;
  }
  return Measurements{
      .current_a = static_cast<float>(get_bits(frame.data, 0, 16)) * 0.001f,
      .target_rpm = from_raw_signed(get_bits(frame.data, 16, 16), 16, 0.1f),
      .encoder_count = static_cast<std::int32_t>(sign_extend(get_bits(frame.data, 32, 32), 32)),
  };
}

CanFrame encode(const Heartbeat& heartbeat) {
  Bytes data{};
  put_bits(data, 0, 8, heartbeat.state);
  put_bits(data, 8, 8, heartbeat.reset_cause);
  put_bits(data, 16, 32, heartbeat.uptime_s);
  return make_frame(kHeartbeatId, data);
}

std::optional<Heartbeat> decode_heartbeat(const CanFrame& frame) {
  if (!matches(frame, kHeartbeatId)) {
    return std::nullopt;
  }
  return Heartbeat{
      .state = static_cast<std::uint8_t>(get_bits(frame.data, 0, 8)),
      .reset_cause = static_cast<std::uint8_t>(get_bits(frame.data, 8, 8)),
      .uptime_s = static_cast<std::uint32_t>(get_bits(frame.data, 16, 32)),
  };
}

}  // namespace protocol
