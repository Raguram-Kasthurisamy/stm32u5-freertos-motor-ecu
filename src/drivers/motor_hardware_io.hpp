// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

#include "control/motor_io.hpp"

namespace drivers {

// control::MotorIo on the real power stage: converts BSP-level signals (raw counts,
// sense volts) into the units the controller works in.
class MotorHardwareIo final : public control::MotorIo {
 public:
  MotorHardwareIo(float sense_volts_per_amp, std::int32_t encoder_sign)
      : sense_volts_per_amp_(sense_volts_per_amp), encoder_sign_(encoder_sign) {}

  control::Sample read() override;
  void write(float duty) override;
  void set_enabled(bool enabled) override;
  void reset_fault_latch() override;
  [[nodiscard]] const char* name() const override { return "hardware"; }

 private:
  float sense_volts_per_amp_;
  std::int32_t encoder_sign_;
};

}  // namespace drivers
