// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace safety {

enum class FaultType : std::uint8_t {
  kNone = 0,
  kHardFault,
  kMemManage,
  kBusFault,
  kUsageFault,
  kStackOverflow,
  kAssert,
  kTaskStall,
  kHeapUse,
  kFatal,
};

[[nodiscard]] const char* to_string(FaultType type);

// The most specific cause named in a Cortex-M Configurable Fault Status Register value.
[[nodiscard]] const char* describe_cfsr(std::uint32_t cfsr);
inline constexpr std::uint32_t kCfsrMmarValid = 1U << 7;
inline constexpr std::uint32_t kCfsrBfarValid = 1U << 15;
inline constexpr std::uint32_t kCfsrStackOverflow = 1U << 20;  // UFSR.STKOF (ARMv8-M)

// What the firmware knew at the moment it gave up, in a fixed 64-byte layout.
//
// The same record is kept in RAM that survives a reset (so the next boot can report it) and
// appended to the flash fault log (so it survives power cycles). It is a plain aggregate with
// no constructors on purpose: an object in the .noinit section must not be initialized by
// startup code, or the record would be wiped before anyone could read it.
struct CrashRecord {
  std::uint32_t magic;
  std::uint32_t sequence;       // position in the flash log
  FaultType type;
  std::uint8_t reset_cause;     // filled in on the boot that reports it
  std::uint16_t reserved;
  std::uint32_t pc;             // where the CPU was (stacked PC for CPU faults)
  std::uint32_t lr;
  std::uint32_t psr;
  std::uint32_t cfsr;           // configurable fault status register
  std::uint32_t hfsr;           // hard fault status register
  std::uint32_t fault_address;  // MMFAR or BFAR when valid
  std::int32_t line;            // source line for asserts
  std::array<char, 20> text;    // file name tail, task name or reason
  std::uint32_t checksum;
};
static_assert(sizeof(CrashRecord) == 64, "record layout is shared with the flash log");
static_assert(std::is_trivially_copyable_v<CrashRecord>);
static_assert(std::is_aggregate_v<CrashRecord>);

inline constexpr std::uint32_t kCrashMagic = 0x4352'5348U;  // "CRSH"

// Stores `value`, keeping its end if it is too long (for file paths the end is the useful
// part). Always NUL-terminated.
void set_text(CrashRecord& record, std::string_view value);
[[nodiscard]] std::string_view text(const CrashRecord& record);

// FNV-1a over every byte before the checksum field.
[[nodiscard]] std::uint32_t compute_checksum(const CrashRecord& record);
void seal(CrashRecord& record);  // sets magic and checksum
[[nodiscard]] bool is_valid(const CrashRecord& record);

}  // namespace safety
