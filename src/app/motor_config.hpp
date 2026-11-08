// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

#include "control/dc_motor_model.hpp"
#include "control/speed_controller.hpp"

// Motor and controller parameters: the one file to edit for your gearmotor and driver.
// It depends only on lib/, so the host tests validate these exact values against the
// simulated motor in CI (test/test_motor_config.cpp).
namespace app::motor_config {

inline constexpr std::uint32_t kControlRateHz = 1000;
inline constexpr float kControlPeriodS = 1.0f / static_cast<float>(kControlRateHz);

// ---- Gearmotor (example: 12 V, ~190 rpm, 30:1, 11-pulse Hall encoder) ------------------
inline constexpr float kEncoderCountsPerMotorRev = 44.0f;  // 11 pulses x 4 (both edges, both channels)
inline constexpr float kGearRatio = 30.0f;
inline constexpr float kCountsPerOutputRev = kEncoderCountsPerMotorRev * kGearRatio;
inline constexpr float kNoLoadRpm = 190.0f;  // output shaft at full supply
// +1 or -1. If `duty 30` makes the speed read negative, flip this (or swap encoder A/B).
inline constexpr std::int32_t kEncoderSign = 1;

// ---- Driver current sense ---------------------------------------------------------------
// Volts at the ADC pin per amp of motor current. Example: ~1.1 V/A for a DRV8874-based
// carrier (450 uA/A current mirror into 2.49 kOhm). Check your driver board's datasheet.
inline constexpr float kCurrentSenseVoltsPerAmp = 1.1f;

// ---- Controller ---------------------------------------------------------------------------
inline constexpr control::ControllerConfig kController{
    .counts_per_output_rev = kCountsPerOutputRev,
    .speed_window = 10,  // 10 ms window: 4.5 rpm resolution, 5 ms average delay
    .max_rpm = 180.0f,
    .ramp_rpm_per_s = 400.0f,
    .feedforward_duty_per_rpm = 1.0f / kNoLoadRpm,
    .pid = {.gains = {.kp = 0.003f, .ki = 0.05f, .kd = 0.0f},
            .output_min = -1.0f,
            .output_max = 1.0f,
            .derivative_cutoff_hz = 50.0f},
    .overcurrent_a = 2.5f,
    .overcurrent_cycles = 20,  // 20 ms above the limit
    .feedback_check_duty = 0.4f,
    .feedback_min_rpm = 10.0f,
    .feedback_cycles = 300,  // 300 ms of implausible feedback
};

// ---- Simulated motor (used by `sim on` and by the host tests) ----------------------------
// Roughly a 12 V 370-size gearmotor: ~3 A stall, ~190 rpm no-load at the output shaft.
inline constexpr control::DcMotorParams kSimulatedMotor{
    .supply_v = 12.0f,
    .resistance_ohm = 4.0f,
    .torque_constant = 0.019f,
    .inertia = 2.0e-6f,
    .viscous_friction = 1.0e-6f,
    .gear_ratio = kGearRatio,
    .counts_per_motor_rev = kEncoderCountsPerMotorRev,
};

}  // namespace app::motor_config
