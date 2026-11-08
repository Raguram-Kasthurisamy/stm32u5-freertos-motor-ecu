// Author: Raguram Kasthurisamy

#include "system/fatal.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "bsp/board.hpp"
#include "bsp/fault.hpp"
#include "bsp/hal.hpp"
#include "bsp/motor_hw.hpp"
#include "system/crash_store.hpp"

namespace {

bool debugger_attached() {
#if defined(DCB)  // ARMv8-M name (Cortex-M33) in CMSIS 5.8+
  return (DCB->DHCSR & DCB_DHCSR_C_DEBUGEN_Msk) != 0U;
#else
  return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U;
#endif
}

void busy_wait_cycles(std::uint32_t loops) {
  for (std::uint32_t i = 0; i < loops; ++i) {
    __NOP();  // an asm statement, so the loop is not optimized away
  }
}

std::uint32_t caller_address(void* address) {
  return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address));
}

}  // namespace

namespace sys {

void reboot_after_fault() {
  __disable_irq();
  bsp::motor_hw::emergency_stop();
  if (debugger_attached()) {
    __BKPT(0);  // without a debugger BKPT would escalate to HardFault, hence the check
  }
  for (int i = 0; i < 12; ++i) {  // a short burst of fast blinks, visible on the board
    bsp::led_toggle();
    busy_wait_cycles(1'000'000U);
  }
  bsp::system_reset();
}

void fatal(safety::FaultType type, const char* where, int line) {
  __disable_irq();
  crash::record(type, where != nullptr ? where : "", line,
                caller_address(__builtin_return_address(0)));
  reboot_after_fault();
}

}  // namespace sys

// The BSP reports failed hardware set-up through this hook.
void bsp::fatal_error_hook(const char* reason, const char* where, int line) {
  (void)reason;  // the file and line are what fit in the record
  sys::fatal(safety::FaultType::kFatal, where, line);
}

// CPU faults (HardFault, MemManage, BusFault, UsageFault) arrive here with the stacked frame.
void bsp::cpu_fault_hook(const bsp::FaultInfo& info) {
  sys::crash::record_cpu_fault(info);
  sys::reboot_after_fault();
}

// Backend for configASSERT() (declared in FreeRTOSConfig.h).
extern "C" void app_assert_failed(const char* file, int line) {
  sys::fatal(safety::FaultType::kAssert, file, line);
}
