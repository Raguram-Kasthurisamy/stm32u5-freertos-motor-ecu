// Author: Raguram Kasthurisamy

#pragma once

#include <atomic>
#include <cstdint>

#include "app/control_task.hpp"
#include "io/text_writer.hpp"
#include "os/static_task.hpp"
#include "os/task_config.hpp"
#include "app/health.hpp"

namespace app {

// Streams control-loop telemetry as machine-readable lines for tools/telemetry_plot.py:
//   $T,<ms>,<target_rpm>,<setpoint_rpm>,<speed_rpm>,<duty>,<current_a>,<mode>
// The "$T," prefix lets the tool pick these lines out of normal console output.
class TelemetryTask final
    : public os::StaticTask<TelemetryTask, os::task_config::kTelemetryStackWords> {
 public:
  static constexpr std::uint32_t kMaxRateHz = 100;

  TelemetryTask(io::ByteSink& out, const ControlTask& control) : out_(out), control_(control) {}

  // 0 stops the stream. Safe to call from any task.
  void set_rate_hz(std::uint32_t hz) {
    rate_hz_.store(hz > kMaxRateHz ? kMaxRateHz : hz, std::memory_order_relaxed);
  }
  [[nodiscard]] std::uint32_t rate_hz() const { return rate_hz_.load(std::memory_order_relaxed); }
  void set_health_id(HealthId id) { health_id_ = id; }

 private:
  friend class os::StaticTask<TelemetryTask, os::task_config::kTelemetryStackWords>;
  void run();

  io::TextWriter out_;
  const ControlTask& control_;
  std::atomic<std::uint32_t> rate_hz_{0};
  HealthId health_id_ = kNoHealthId;
};

}  // namespace app
