// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

#include "control/dc_motor_model.hpp"
#include "control/motor_io.hpp"

namespace control {

enum class EncoderFault : std::uint8_t {
  kNone,
  kInverted,      // A/B channels swapped: counts run backwards
  kDisconnected,  // no edges: count frozen
};

// MotorIo backed by DcMotorModel. Each read() advances the model by one control period,
// so the controller sees exactly what it would see from hardware sampled at that rate.
class SimulatedMotorIo final : public MotorIo {
 public:
  SimulatedMotorIo(const DcMotorParams& params, float control_period_s)
      : model_(params), period_s_(control_period_s) {}

  Sample read() override;
  void write(float duty) override { duty_ = duty; }
  void set_enabled(bool enabled) override { enabled_ = enabled; }
  void reset_fault_latch() override {}
  [[nodiscard]] const char* name() const override { return "simulation"; }

  DcMotorModel& model() { return model_; }
  void set_encoder_fault(EncoderFault fault) { encoder_fault_ = fault; }

 private:
  DcMotorModel model_;
  float period_s_;
  float duty_ = 0.0f;
  bool enabled_ = false;
  EncoderFault encoder_fault_ = EncoderFault::kNone;
  std::int32_t frozen_count_ = 0;
};

}  // namespace control
