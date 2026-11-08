// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

#include "protocol/can_frame.hpp"
#include "protocol/ecu_protocol.hpp"

namespace protocol {

// Guards the CAN command channel, in the spirit of automotive end-to-end protection:
//  - a frame is accepted only with a correct CRC, a known mode, and an alive counter that
//    differs from the previous frame's (a frozen sender repeats itself and is ignored);
//  - while a CAN command keeps the motor running, a valid frame must arrive at least every
//    `timeout_ms`, otherwise check_timeout() reports it once and the session ends.
class CommandMonitor {
 public:
  enum class Result : std::uint8_t { kAccepted, kInvalid, kRepeated };

  struct Stats {
    std::uint32_t accepted = 0;
    std::uint32_t invalid = 0;   // wrong length, CRC or mode
    std::uint32_t repeated = 0;  // alive counter did not change
    std::uint32_t timeouts = 0;
  };

  explicit CommandMonitor(std::uint32_t timeout_ms) : timeout_ms_(timeout_ms) {}

  Result on_frame(const CanFrame& frame, std::uint32_t now_ms, Command& command);
  // True exactly once when an active CAN session has gone silent for too long.
  bool check_timeout(std::uint32_t now_ms);
  // Another source (the console) took over: stop expecting CAN commands.
  void release() { active_ = false; }

  [[nodiscard]] bool active() const { return active_; }
  [[nodiscard]] const Stats& stats() const { return stats_; }

 private:
  std::uint32_t timeout_ms_;
  std::uint32_t last_ms_ = 0;
  bool active_ = false;
  bool have_counter_ = false;
  std::uint8_t last_counter_ = 0;
  Stats stats_{};
};

}  // namespace protocol
