// Author: Raguram Kasthurisamy

#include "control/simulated_motor_io.hpp"

namespace control {

Sample SimulatedMotorIo::read() {
  model_.step(enabled_ ? duty_ : 0.0f, period_s_, enabled_);

  std::int32_t count = model_.encoder_count();
  switch (encoder_fault_) {
    case EncoderFault::kNone:
      frozen_count_ = count;
      break;
    case EncoderFault::kInverted:
      // Negate in unsigned arithmetic: well defined even for INT32_MIN.
      count = static_cast<std::int32_t>(0U - static_cast<std::uint32_t>(count));
      break;
    case EncoderFault::kDisconnected:
      count = frozen_count_;
      break;
  }
  return Sample{.encoder_count = count, .current_a = model_.current_a(), .driver_fault = false};
}

}  // namespace control
