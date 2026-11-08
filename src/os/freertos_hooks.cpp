// Author: Raguram Kasthurisamy

// Callbacks the FreeRTOS kernel requires from the application.

#include "FreeRTOS.h"
#include "task.h"
#include "system/fatal.hpp"

namespace {

StaticTask_t g_idle_tcb;
StackType_t g_idle_stack[configMINIMAL_STACK_SIZE];

}  // namespace

extern "C" {

// With static allocation the kernel cannot allocate the idle task itself.
void vApplicationGetIdleTaskMemory(StaticTask_t** tcb, StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* stack_words) {
  *tcb = &g_idle_tcb;
  *stack = g_idle_stack;
  *stack_words = configMINIMAL_STACK_SIZE;
}

// configCHECK_FOR_STACK_OVERFLOW == 2: called from the context switch when a task's stack
// watermark pattern has been overwritten. The task's memory is already corrupt, so the
// only safe response is to stop.
void vApplicationStackOverflowHook(TaskHandle_t task, char* task_name) {
  (void)task;
  sys::fatal(safety::FaultType::kStackOverflow, task_name, 0);
}

}  // extern "C"
