// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

namespace control {

// One control-period snapshot of the motor's sensors.
struct Sample {
  std::int32_t encoder_count = 0;  // raw quadrature count; may wrap
  float current_a = 0.0f;          // magnitude, averaged over the last control period
  bool driver_fault = false;       // H-bridge reports a fault (overtemperature, short...)
};

// The control loop's only view of the motor. Implemented by real hardware (TIM/ADC/GPIO)
// and by a simulated motor, so the same controller code runs against either.
class MotorIo {
 public:
  virtual Sample read() = 0;
  virtual void write(float duty) = 0;  // -1..+1, sign = direction
  virtual void set_enabled(bool enabled) = 0;
  // Clears a latched hardware fault indication once the operator acknowledges it.
  virtual void reset_fault_latch() = 0;
  [[nodiscard]] virtual const char* name() const = 0;

 protected:
  MotorIo() = default;
  ~MotorIo() = default;
  MotorIo(const MotorIo&) = default;
  MotorIo& operator=(const MotorIo&) = default;
};

}  // namespace control
