// Author: Raguram Kasthurisamy

#include "system/crash_store.hpp"

namespace sys::crash {
namespace {

// Not touched by startup code (see the linker script). Random after power-on; the magic
// number and checksum tell a real record from noise.
__attribute__((section(".noinit"))) safety::CrashRecord g_record;

safety::FaultType classify(const bsp::FaultInfo& info) {
  if ((info.cfsr & safety::kCfsrStackOverflow) != 0U) {
    return safety::FaultType::kStackOverflow;
  }
  switch (info.kind) {
    case bsp::CpuFault::kMemManage: return safety::FaultType::kMemManage;
    case bsp::CpuFault::kBus: return safety::FaultType::kBusFault;
    case bsp::CpuFault::kUsage: return safety::FaultType::kUsageFault;
    case bsp::CpuFault::kHard: break;
  }
  return safety::FaultType::kHardFault;
}

}  // namespace

void record(safety::FaultType type, std::string_view text, int line, std::uint32_t pc) {
  safety::CrashRecord record{};
  record.type = type;
  record.line = line;
  record.pc = pc;
  safety::set_text(record, text);
  safety::seal(record);
  g_record = record;
}

void record_cpu_fault(const bsp::FaultInfo& info) {
  safety::CrashRecord record{};
  record.type = classify(info);
  record.pc = info.pc;
  record.lr = info.lr;
  record.psr = info.psr;
  record.cfsr = info.cfsr;
  record.hfsr = info.hfsr;
  if ((info.cfsr & safety::kCfsrBfarValid) != 0U) {
    record.fault_address = info.bfar;
  } else if ((info.cfsr & safety::kCfsrMmarValid) != 0U) {
    record.fault_address = info.mmfar;
  }
  safety::set_text(record, safety::describe_cfsr(info.cfsr));
  safety::seal(record);
  g_record = record;
}

std::optional<safety::CrashRecord> take() {
  if (!safety::is_valid(g_record)) {
    return std::nullopt;
  }
  const safety::CrashRecord copy = g_record;
  g_record.magic = 0U;  // report each crash once
  return copy;
}

}  // namespace sys::crash
