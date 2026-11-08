// Author: Raguram Kasthurisamy

#include "system/boot_report.hpp"

#include "bsp/board.hpp"
#include "system/crash_store.hpp"
#include "system/fault_log.hpp"

namespace sys::boot {
namespace {
std::optional<safety::CrashRecord> g_last_fault;
}  // namespace

void process() {
  std::optional<safety::CrashRecord> record = crash::take();
  if (!record) {
    return;
  }
  record->reset_cause = static_cast<std::uint8_t>(bsp::reset_cause());
  (void)fault_log::append(*record);  // sets the sequence number
  g_last_fault = record;
}

const std::optional<safety::CrashRecord>& last_fault() { return g_last_fault; }

}  // namespace sys::boot
