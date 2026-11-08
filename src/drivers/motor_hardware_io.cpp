// Author: Raguram Kasthurisamy

#include "drivers/motor_hardware_io.hpp"

#include "bsp/motor_hw.hpp"

namespace drivers {

control::Sample MotorHardwareIo::read() {
  auto count = static_cast<std::uint32_t>(bsp::motor_hw::encoder_count());
  if (encoder_sign_ < 0) {
    count = 0U - count;  // unsigned negation: well defined for every value
  }
  return control::Sample{
      .encoder_count = static_cast<std::int32_t>(count),
      .current_a = bsp::motor_hw::current_sense_volts() / sense_volts_per_amp_,
      .driver_fault = bsp::motor_hw::driver_fault(),
  };
}

void MotorHardwareIo::write(float duty) { bsp::motor_hw::set_duty(duty); }

void MotorHardwareIo::set_enabled(bool enabled) {
  if (!enabled) {
    bsp::motor_hw::set_duty(0.0f);
  }
  bsp::motor_hw::set_driver_enabled(enabled);
}

void MotorHardwareIo::reset_fault_latch() { bsp::motor_hw::clear_fault_latch(); }

}  // namespace drivers
