// Author: Raguram Kasthurisamy

#include "safety/fault_log_format.hpp"

#include <cstring>

namespace safety::fault_log {
namespace {

bool slot_erased(std::span<const std::uint8_t> region, std::size_t index) {
  std::uint32_t first_word = 0;
  std::memcpy(&first_word, region.data() + index * kSlotSize, sizeof(first_word));
  return first_word == 0xFFFF'FFFFU;
}

}  // namespace

std::size_t used_slots(std::span<const std::uint8_t> region) {
  const std::size_t slots = capacity(region);
  for (std::size_t i = 0; i < slots; ++i) {
    if (slot_erased(region, i)) {
      return i;
    }
  }
  return slots;
}

std::optional<CrashRecord> read_slot(std::span<const std::uint8_t> region, std::size_t index) {
  if (index >= capacity(region) || slot_erased(region, index)) {
    return std::nullopt;
  }
  CrashRecord record;
  std::memcpy(&record, region.data() + index * kSlotSize, sizeof(record));
  if (!is_valid(record)) {
    return std::nullopt;
  }
  return record;
}

std::uint32_t next_sequence(std::span<const std::uint8_t> region) {
  std::uint32_t highest = 0;
  const std::size_t used = used_slots(region);
  for (std::size_t i = 0; i < used; ++i) {
    if (const auto record = read_slot(region, i); record && record->sequence > highest) {
      highest = record->sequence;
    }
  }
  return highest + 1U;
}

}  // namespace safety::fault_log
