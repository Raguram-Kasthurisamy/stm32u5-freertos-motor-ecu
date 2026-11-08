// Author: Raguram Kasthurisamy

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "safety/crash_record.hpp"

// Layout of the persistent fault log: one erased flash page used as an array of 64-byte
// CrashRecord slots, filled in order. Erased flash reads 0xFF, so a slot whose first word is
// 0xFFFFFFFF has never been written. These helpers only read; writing is the firmware's job.
namespace safety::fault_log {

inline constexpr std::size_t kSlotSize = sizeof(CrashRecord);

[[nodiscard]] constexpr std::size_t capacity(std::span<const std::uint8_t> region) {
  return region.size() / kSlotSize;
}
// Slots written so far (valid or not), i.e. the index of the first erased slot.
[[nodiscard]] std::size_t used_slots(std::span<const std::uint8_t> region);
// The valid record in a slot, or std::nullopt for an erased or corrupted slot.
[[nodiscard]] std::optional<CrashRecord> read_slot(std::span<const std::uint8_t> region,
                                                   std::size_t index);
// One more than the highest sequence number among the valid records (1 for an empty log).
[[nodiscard]] std::uint32_t next_sequence(std::span<const std::uint8_t> region);

}  // namespace safety::fault_log
