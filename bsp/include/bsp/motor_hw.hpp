// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

// Motor power stage and sensors, as the board wires them: a PWM + direction H-bridge
// (e.g. DRV8874 in PH/EN mode), a quadrature encoder and the driver's current-sense output.
// The driver's nFAULT pin is also wired to TIM1's break input, so a driver overcurrent cuts
// the PWM in hardware without waiting for software.
// Initialized by bsp::init(); the driver starts asleep with 0 % duty.
namespace bsp::motor_hw {

// -1..+1. Magnitude sets the PWM duty, sign sets the direction pin.
void set_duty(float duty);
// H-bridge sleep pin: false = outputs off, motor coasts.
void set_driver_enabled(bool enabled);

// Raw 32-bit quadrature count (4 counts per encoder line). Wraps; use differences.
[[nodiscard]] std::int32_t encoder_count();
// Current-sense voltage averaged over the last control period of PWM-synchronous samples.
[[nodiscard]] float current_sense_volts();
// The driver reports a fault: its nFAULT pin is low now, or the hardware trip (TIM1 break)
// fired since the last clear_fault_latch(), even if nFAULT has recovered since.
[[nodiscard]] bool driver_fault();
void clear_fault_latch();

// Outputs off immediately at register level: PWM main output disabled, duty 0, driver
// asleep. Safe to call from any context, including fault handlers.
void emergency_stop();

}  // namespace bsp::motor_hw
