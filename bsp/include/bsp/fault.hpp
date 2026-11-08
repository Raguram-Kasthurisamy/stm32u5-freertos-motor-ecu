// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

// CPU fault reporting. The BSP's fault handlers capture the exception stack frame and the
// fault status registers, stop the motor outputs, then hand everything to the application.
namespace bsp {

enum class CpuFault : std::uint8_t { kHard = 0, kMemManage = 1, kBus = 2, kUsage = 3 };

struct FaultInfo {
  CpuFault kind;
  std::uint32_t r0, r1, r2, r3, r12, lr, pc, psr;  // stacked by the CPU on exception entry
  std::uint32_t cfsr, hfsr, mmfar, bfar;
  std::uint32_t exc_return;  // which stack the faulting code was using
};

// Implemented by the application (src/system/fatal.cpp). Must not return.
[[noreturn]] void cpu_fault_hook(const FaultInfo& info);

}  // namespace bsp
