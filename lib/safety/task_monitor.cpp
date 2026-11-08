// Author: Raguram Kasthurisamy

#include "safety/task_monitor.hpp"

namespace safety {

std::optional<TaskMonitor::Id> TaskMonitor::add(const char* name, std::uint32_t deadline_ms,
                                                std::uint32_t now_ms) {
  if (count_ >= kMaxTasks) {
    return std::nullopt;
  }
  Slot& slot = slots_[count_];
  slot.name = name;
  slot.deadline_ms = deadline_ms;
  slot.last_ms.store(now_ms, std::memory_order_relaxed);
  return static_cast<Id>(count_++);
}

std::uint32_t TaskMonitor::age_ms(Id id, std::uint32_t now_ms) const {
  if (id >= count_) {
    return 0;
  }
  return now_ms - slots_[id].last_ms.load(std::memory_order_relaxed);  // unsigned: wrap-safe
}

std::optional<TaskMonitor::Id> TaskMonitor::find_stalled(std::uint32_t now_ms) const {
  for (std::size_t i = 0; i < count_; ++i) {
    const auto id = static_cast<Id>(i);
    if (age_ms(id, now_ms) > slots_[i].deadline_ms) {
      return id;
    }
  }
  return std::nullopt;
}

}  // namespace safety
