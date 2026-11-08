// Author: Raguram Kasthurisamy

#include "safety/crash_record.hpp"

#include <cstring>

namespace safety {

const char* to_string(FaultType type) {
  switch (type) {
    case FaultType::kNone: return "none";
    case FaultType::kHardFault: return "hard fault";
    case FaultType::kMemManage: return "memory management fault";
    case FaultType::kBusFault: return "bus fault";
    case FaultType::kUsageFault: return "usage fault";
    case FaultType::kStackOverflow: return "stack overflow";
    case FaultType::kAssert: return "assert failed";
    case FaultType::kTaskStall: return "task stalled";
    case FaultType::kHeapUse: return "heap use";
    case FaultType::kFatal: return "fatal error";
  }
  return "unknown";
}

const char* describe_cfsr(std::uint32_t cfsr) {
  struct Cause {
    std::uint32_t bit;
    const char* text;
  };
  // Ordered from most to least specific.
  static constexpr Cause kCauses[] = {
      {kCfsrStackOverflow, "stack limit hit"},
      {1U << 25, "divide by zero"},
      {1U << 24, "unaligned access"},
      {1U << 16, "undefined instruction"},
      {1U << 17, "invalid state"},
      {1U << 18, "invalid return"},
      {1U << 19, "no coprocessor"},
      {1U << 9, "precise bus error"},
      {1U << 10, "imprecise bus error"},
      {1U << 8, "instruction bus error"},
      {1U << 12, "stacking bus error"},
      {1U << 11, "unstacking bus error"},
      {1U << 1, "data access violation"},
      {1U << 0, "instruction access violation"},
      {1U << 4, "stacking mem fault"},
  };
  for (const Cause& cause : kCauses) {
    if ((cfsr & cause.bit) != 0U) {
      return cause.text;
    }
  }
  return "no fault bits";
}

void set_text(CrashRecord& record, std::string_view value) {
  record.text.fill('\0');
  const std::size_t capacity = record.text.size() - 1U;
  if (value.size() > capacity) {
    value = std::string_view(value.data() + (value.size() - capacity), capacity);
  }
  std::memcpy(record.text.data(), value.data(), value.size());
}

std::string_view text(const CrashRecord& record) {
  const std::size_t length = ::strnlen(record.text.data(), record.text.size());
  return std::string_view(record.text.data(), length);
}

std::uint32_t compute_checksum(const CrashRecord& record) {
  std::uint32_t hash = 2166136261U;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&record);
  for (std::size_t i = 0; i < offsetof(CrashRecord, checksum); ++i) {
    hash = (hash ^ bytes[i]) * 16777619U;
  }
  return hash;
}

void seal(CrashRecord& record) {
  record.magic = kCrashMagic;
  record.checksum = compute_checksum(record);
}

bool is_valid(const CrashRecord& record) {
  return record.magic == kCrashMagic && record.checksum == compute_checksum(record);
}

}  // namespace safety
