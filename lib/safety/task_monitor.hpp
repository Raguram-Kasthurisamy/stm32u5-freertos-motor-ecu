// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace safety {

// Task-level liveness monitor, the software half of a watchdog scheme.
//
// Every monitored task calls checkin() from its main loop. A supervisor calls find_stalled()
// periodically and only feeds the hardware watchdog when no task has missed its deadline.
// This catches what a plain watchdog cannot: one deadlocked task while the others (and the
// supervisor) keep running. Check-ins are single atomic stores, so they are cheap enough for
// a 1 kHz loop and safe to call from any task.
class TaskMonitor {
 public:
  static constexpr std::size_t kMaxTasks = 8;
  using Id = std::uint8_t;

  // Registers a task before it starts running. Returns std::nullopt when full.
  std::optional<Id> add(const char* name, std::uint32_t deadline_ms, std::uint32_t now_ms);

  void checkin(Id id, std::uint32_t now_ms) {
    if (id < count_) {
      slots_[id].last_ms.store(now_ms, std::memory_order_relaxed);
    }
  }

  // The first task whose last check-in is older than its deadline, if any.
  [[nodiscard]] std::optional<Id> find_stalled(std::uint32_t now_ms) const;

  [[nodiscard]] std::size_t size() const { return count_; }
  [[nodiscard]] const char* name(Id id) const { return id < count_ ? slots_[id].name : "?"; }
  [[nodiscard]] std::uint32_t deadline_ms(Id id) const {
    return id < count_ ? slots_[id].deadline_ms : 0U;
  }
  [[nodiscard]] std::uint32_t age_ms(Id id, std::uint32_t now_ms) const;

 private:
  struct Slot {
    const char* name = "";
    std::uint32_t deadline_ms = 0;
    std::atomic<std::uint32_t> last_ms{0};
  };

  std::array<Slot, kMaxTasks> slots_{};
  std::size_t count_ = 0;
};

}  // namespace safety
