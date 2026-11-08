// Author: Raguram Kasthurisamy

#include "FreeRTOS.h"
#include "app/app.hpp"
#include "bsp/board.hpp"
#include "system/boot_report.hpp"
#include "system/fatal.hpp"
#include "task.h"

// Start-up order is load-bearing:
//
// 1. bsp::init() configures clocks and peripherals. HAL timeouts during that work are
//    counted by the TIM6 interrupt, so interrupts must be running.
// 2. The first FreeRTOS call that creates an object (task, mutex, stream buffer...) masks
//    every interrupt with priority value >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
//    (including TIM6) until the scheduler starts. A HAL timeout after that point never
//    expires and the board hangs before printing anything.
//
// So: all hardware init first, then all RTOS objects, then the scheduler. And never a
// FreeRTOS call from a static constructor, because those run before main().
int main() {
  bsp::init();
  sys::boot::process();  // report the previous crash, if any (writes flash: before the RTOS)
  app::start();
  vTaskStartScheduler();
  sys::fatal(safety::FaultType::kFatal, "scheduler returned", __LINE__);  // bad config only
}
