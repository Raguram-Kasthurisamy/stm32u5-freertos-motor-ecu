// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "protocol/can_frame.hpp"

// The ECU's CAN protocol, matching protocol/ecu.dbc signal for signal.
//
// All signals use Intel (little-endian) bit numbering, as in DBC "@1". Physical values are
// stored as scaled integers (e.g. speed in 0.1 rpm steps), rounded and clamped to the range
// of the signal, so an out-of-range value saturates instead of wrapping around.
namespace protocol {

inline constexpr std::uint32_t kCommandId = 0x100;
inline constexpr std::uint32_t kStatusId = 0x180;
inline constexpr std::uint32_t kMeasurementsId = 0x181;
inline constexpr std::uint32_t kHeartbeatId = 0x701;  // CANopen convention: 0x700 + node id 1

enum class CommandMode : std::uint8_t {
  kStop = 0,
  kOpenLoop = 1,
  kSpeed = 2,
  kClearFault = 3,
};

struct Command {
  CommandMode mode = CommandMode::kStop;
  float speed_rpm = 0.0f;
  float duty = 0.0f;  // -1..+1 (sent as percent)
  std::uint8_t counter = 0;
};

struct Status {
  std::uint8_t mode = 0;   // control::Mode
  std::uint8_t fault = 0;  // control::Fault
  float speed_rpm = 0.0f;
  float setpoint_rpm = 0.0f;
  float duty = 0.0f;  // -1..+1
  std::uint8_t counter = 0;
};

struct Measurements {
  float current_a = 0.0f;
  float target_rpm = 0.0f;
  std::int32_t encoder_count = 0;
};

struct Heartbeat {
  std::uint8_t state = 0;
  std::uint8_t reset_cause = 0;
  std::uint32_t uptime_s = 0;
};

// CRC-8 SAE J1850: polynomial 0x1D, initial value 0xFF, final XOR 0xFF.
[[nodiscard]] std::uint8_t crc8_sae_j1850(std::span<const std::uint8_t> bytes);

[[nodiscard]] CanFrame encode(const Command& command);  // fills in the CRC
[[nodiscard]] CanFrame encode(const Status& status);
[[nodiscard]] CanFrame encode(const Measurements& measurements);
[[nodiscard]] CanFrame encode(const Heartbeat& heartbeat);

// Each returns std::nullopt for the wrong identifier or length; commands are also rejected
// for a bad CRC or an unknown mode.
[[nodiscard]] std::optional<Command> decode_command(const CanFrame& frame);
[[nodiscard]] std::optional<Status> decode_status(const CanFrame& frame);
[[nodiscard]] std::optional<Measurements> decode_measurements(const CanFrame& frame);
[[nodiscard]] std::optional<Heartbeat> decode_heartbeat(const CanFrame& frame);

}  // namespace protocol
