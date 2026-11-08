// Author: Raguram Kasthurisamy

#pragma once

#include <cstdint>

#include "FreeRTOS.h"

// Every NVIC priority used by the firmware, in one place (0 = most urgent).
namespace bsp::irq_priority {

// Control-loop tick: the most urgent level still allowed to call FreeRTOS, so console
// traffic can never delay the motor controller.
inline constexpr std::uint32_t kControlTick = 5;

// CAN receive interrupt: pushes frames into a FreeRTOS queue.
inline constexpr std::uint32_t kCan = 6;

// Console UART and its DMA streams call FreeRTOS FromISR APIs.
inline constexpr std::uint32_t kConsoleUart = 6;
inline constexpr std::uint32_t kConsoleDma = 6;

// An ISR that calls a FreeRTOS FromISR API with a priority more urgent than this ceiling
// can corrupt kernel data structures. The bug is intermittent and miserable to debug,
// so it is rejected at compile time instead.
static_assert(kControlTick >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
              "control tick IRQ calls FreeRTOS APIs: priority must be >= the syscall ceiling");
static_assert(kCan >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
              "CAN IRQ calls FreeRTOS APIs: priority must be >= the syscall ceiling");
static_assert(kConsoleUart >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
              "console UART IRQ calls FreeRTOS APIs: priority must be >= the syscall ceiling");
static_assert(kConsoleDma >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
              "console DMA IRQ calls FreeRTOS APIs: priority must be >= the syscall ceiling");

}  // namespace bsp::irq_priority
