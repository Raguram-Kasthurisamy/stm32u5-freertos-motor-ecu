// Author: Raguram Kasthurisamy

#pragma once

#include <cstddef>
#include <cstdint>

#include "control/motor_io.hpp"
#include "control/pid.hpp"
#include "control/slew_limiter.hpp"
#include "control/speed_estimator.hpp"

namespace control {

enum class Mode : std::uint8_t {
  kDisabled,  // driver asleep, motor coasts
  kOpenLoop,  // fixed duty, no feedback
  kSpeed,     // closed-loop speed control
  kFault,     // latched after a protection trip; needs clear_fault()
};

enum class Fault : std::uint8_t {
  kNone,
  kOvercurrent,      // current above the limit for too long
  kDriverFault,      // the H-bridge reported a fault
  kNoFeedback,       // driving hard but the shaft does not move: stalled, or encoder lost
  kEncoderReversed,  // the shaft moves against the drive: encoder wired backwards
  kTickLost,         // the control tick stopped arriving
};

[[nodiscard]] const char* to_string(Mode mode);
[[nodiscard]] const char* to_string(Fault fault);

struct ControllerConfig {
  float counts_per_output_rev = 1.0f;
  std::size_t speed_window = 10;       // control periods averaged by the speed estimator
  float max_rpm = 0.0f;                // setpoints are clamped to +/- this
  float ramp_rpm_per_s = 0.0f;         // setpoint slew rate
  float feedforward_duty_per_rpm = 0.0f;
  PidConfig pid{};
  float overcurrent_a = 0.0f;
  std::uint32_t overcurrent_cycles = 0;  // consecutive periods above the limit before tripping
  float feedback_check_duty = 0.0f;      // feedback plausibility is checked above this |duty|
  float feedback_min_rpm = 0.0f;         // below this the shaft counts as "not moving"
  std::uint32_t feedback_cycles = 0;     // consecutive implausible periods before tripping
};

struct Status {
  Mode mode = Mode::kDisabled;
  Fault fault = Fault::kNone;
  float target_rpm = 0.0f;    // requested speed
  float setpoint_rpm = 0.0f;  // ramped speed the PID is tracking
  float speed_rpm = 0.0f;     // measured
  float duty = 0.0f;          // applied
  float current_a = 0.0f;     // measured
  std::int32_t encoder_count = 0;
};

struct Output {
  float duty = 0.0f;
  bool driver_enabled = false;
};

// Motor speed controller: mode logic, setpoint ramp, PID with feedforward, and the
// protections that make it safe to leave running. Pure logic, no hardware: it is unit
// tested on the PC against DcMotorModel.
class SpeedController {
 public:
  explicit SpeedController(const ControllerConfig& config);

  void stop();
  bool set_open_loop(float duty);  // false while faulted
  bool set_speed(float rpm);       // false while faulted
  bool clear_fault();              // false if the fault condition is still present
  void trip(Fault fault);

  void set_gains(const PidGains& gains) { pid_.set_gains(gains); }
  [[nodiscard]] const PidGains& gains() const { return pid_.gains(); }

  // Runs one control period: estimate speed, check protections, compute the duty.
  Output update(const Sample& sample, float dt);

  [[nodiscard]] const Status& status() const { return status_; }
  [[nodiscard]] const ControllerConfig& config() const { return config_; }

 private:
  void check_protections(const Sample& sample);
  void reset_counters();

  ControllerConfig config_;
  Pid pid_;
  SpeedEstimator speed_;
  SlewLimiter ramp_;
  Status status_{};
  float open_loop_duty_ = 0.0f;
  bool driver_fault_present_ = false;
  std::uint32_t overcurrent_count_ = 0;
  std::uint32_t no_feedback_count_ = 0;
  std::uint32_t reversed_count_ = 0;
};

}  // namespace control
