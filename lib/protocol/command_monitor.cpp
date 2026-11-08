// Author: Raguram Kasthurisamy

#include "protocol/command_monitor.hpp"

namespace protocol {

CommandMonitor::Result CommandMonitor::on_frame(const CanFrame& frame, std::uint32_t now_ms,
                                                Command& command) {
  const std::optional<Command> decoded = decode_command(frame);
  if (!decoded) {
    ++stats_.invalid;
    return Result::kInvalid;
  }
  if (have_counter_ && decoded->counter == last_counter_) {
    ++stats_.repeated;
    return Result::kRepeated;
  }
  have_counter_ = true;
  last_counter_ = decoded->counter;
  last_ms_ = now_ms;
  active_ = decoded->mode == CommandMode::kOpenLoop || decoded->mode == CommandMode::kSpeed;
  ++stats_.accepted;
  command = *decoded;
  return Result::kAccepted;
}

bool CommandMonitor::check_timeout(std::uint32_t now_ms) {
  if (!active_ || (now_ms - last_ms_) <= timeout_ms_) {  // unsigned: safe across wrap
    return false;
  }
  active_ = false;
  ++stats_.timeouts;
  return true;
}

}  // namespace protocol
