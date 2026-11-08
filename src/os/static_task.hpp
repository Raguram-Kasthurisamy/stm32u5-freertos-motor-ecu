// Author: Raguram Kasthurisamy

#pragma once

#include <array>
#include <cstddef>

#include "FreeRTOS.h"
#include "task.h"

namespace os {

// A FreeRTOS task whose stack and control block live inside this object, so a task
// declared at namespace scope costs exactly sizeof(Derived) of .bss and nothing else.
//
// CRTP instead of a virtual run(): the entry trampoline calls Derived::run() directly,
// so there is no vtable and no indirect call. Derived must provide `void run()`, which
// must never return (befriend the base class if run() is private).
template <typename Derived, std::size_t StackWords>
class StaticTask {
 public:
  static_assert(StackWords >= configMINIMAL_STACK_SIZE, "task stack below the kernel minimum");

  StaticTask(const StaticTask&) = delete;
  StaticTask& operator=(const StaticTask&) = delete;

  // Creates the task. Call after bsp::init() and before vTaskStartScheduler(), never from
  // a static constructor (main.cpp explains why).
  void start(const char* name, UBaseType_t priority) {
    configASSERT(handle_ == nullptr);  // starting twice would corrupt the TCB
    handle_ = xTaskCreateStatic(&StaticTask::entry, name,
                                static_cast<configSTACK_DEPTH_TYPE>(StackWords), this, priority,
                                stack_.data(), &tcb_);
    configASSERT(handle_ != nullptr);
  }

  [[nodiscard]] TaskHandle_t handle() const { return handle_; }

 protected:
  StaticTask() = default;
  ~StaticTask() = default;

 private:
  static void entry(void* param) {
    // Cast through the base type first: converting void* straight to Derived* is only
    // correct if the base subobject happens to sit at offset 0.
    static_cast<Derived*>(static_cast<StaticTask*>(param))->run();
    configASSERT(false);  // run() returned
  }

  std::array<StackType_t, StackWords> stack_{};
  StaticTask_t tcb_{};
  TaskHandle_t handle_ = nullptr;
};

}  // namespace os
