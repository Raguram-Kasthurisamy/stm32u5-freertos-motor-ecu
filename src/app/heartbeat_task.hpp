// Author: Raguram Kasthurisamy

#pragma once

#include <atomic>
#include <cstdint>

#include "os/static_task.hpp"
#include "os/task_config.hpp"
#include "app/health.hpp"

namespace app {

// Blinks the user LED: a visible "the scheduler is alive" signal. A later phase turns
// this into the supervisor that checks every task's heartbeat before kicking the watchdog.
class HeartbeatTask final
    : public os::StaticTask<HeartbeatTask, os::task_config::kHeartbeatStackWords> {
 public:
  static constexpr std::uint32_t kMinPeriodMs = 50;
  static constexpr std::uint32_t kMaxPeriodMs = 5000;

  // Full blink period. Safe to call from any task.
  void set_period_ms(std::uint32_t period_ms) { period_ms_.store(period_ms, std::memory_order_relaxed); }
  [[nodiscard]] std::uint32_t period_ms() const { return period_ms_.load(std::memory_order_relaxed); }
  void set_health_id(HealthId id) { health_id_ = id; }

 private:
  friend class os::StaticTask<HeartbeatTask, os::task_config::kHeartbeatStackWords>;
  void run();

  std::atomic<std::uint32_t> period_ms_{1000};
  HealthId health_id_ = kNoHealthId;
};

}  // namespace app
