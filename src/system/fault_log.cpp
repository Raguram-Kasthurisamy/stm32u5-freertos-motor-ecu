// Author: Raguram Kasthurisamy

#include "system/fault_log.hpp"

#include <array>
#include <cstring>

#include "bsp/log_flash.hpp"
#include "safety/fault_log_format.hpp"

namespace sys::fault_log {

static_assert(safety::fault_log::kSlotSize % bsp::log_flash::kWriteUnit == 0U,
              "records must be a whole number of flash write units");

std::size_t capacity() { return safety::fault_log::capacity(bsp::log_flash::region()); }
std::size_t count() { return safety::fault_log::used_slots(bsp::log_flash::region()); }

std::optional<safety::CrashRecord> read(std::size_t index) {
  return safety::fault_log::read_slot(bsp::log_flash::region(), index);
}

bool erase() { return bsp::log_flash::erase(); }

bool append(safety::CrashRecord& record) {
  const std::uint32_t sequence = safety::fault_log::next_sequence(bsp::log_flash::region());
  std::size_t used = count();
  if (used >= capacity()) {
    if (!erase()) {
      return false;
    }
    used = 0;
  }
  record.sequence = sequence;
  safety::seal(record);
  std::array<std::uint8_t, sizeof(safety::CrashRecord)> bytes{};
  std::memcpy(bytes.data(), &record, bytes.size());
  return bsp::log_flash::program(used * safety::fault_log::kSlotSize, bytes);
}

}  // namespace sys::fault_log
