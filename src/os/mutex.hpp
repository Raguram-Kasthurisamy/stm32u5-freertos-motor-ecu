// Author: Raguram Kasthurisamy

#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

namespace os {

// Statically allocated FreeRTOS mutex (priority inheritance included). init() is separate
// from the constructor so global instances never call into the kernel before main().
class Mutex {
 public:
  Mutex() = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void init() {
    handle_ = xSemaphoreCreateMutexStatic(&storage_);
    configASSERT(handle_ != nullptr);
  }

  void lock() { (void)xSemaphoreTake(handle_, portMAX_DELAY); }
  void unlock() { (void)xSemaphoreGive(handle_); }

 private:
  StaticSemaphore_t storage_{};
  SemaphoreHandle_t handle_ = nullptr;
};

// RAII: the mutex is released on every path out of the scope, including early returns.
class LockGuard {
 public:
  explicit LockGuard(Mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
  ~LockGuard() { mutex_.unlock(); }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

 private:
  Mutex& mutex_;
};

}  // namespace os
