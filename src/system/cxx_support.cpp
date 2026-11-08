// Author: Raguram Kasthurisamy

// Minimal C++ runtime support for a firmware with no heap and no exceptions.

#include <cstddef>

#include "system/fatal.hpp"

// Called if a pure virtual function is invoked (e.g. through a half-constructed object).
extern "C" void __cxa_pure_virtual() { sys::fatal(safety::FaultType::kFatal, "pure virtual call", __LINE__); }

// Registers destructors of static objects to run at exit(). Firmware never exits, so
// nothing is registered; this also keeps newlib's atexit bookkeeping out of the image.
extern "C" int __aeabi_atexit(void* object, void (*destructor)(void*), void* dso_handle) {
  (void)object;
  (void)destructor;
  (void)dso_handle;
  return 0;
}

// Dynamic allocation is a design error in this firmware. Any accidental use (a std::vector,
// a std::function holding a large capture...) stops here immediately and visibly instead
// of failing months later from heap fragmentation.
void* operator new(std::size_t) { sys::fatal(safety::FaultType::kHeapUse, "operator new", __LINE__); }
void* operator new[](std::size_t) { sys::fatal(safety::FaultType::kHeapUse, "operator new[]", __LINE__); }
void operator delete(void*) noexcept { sys::fatal(safety::FaultType::kHeapUse, "operator delete", __LINE__); }
void operator delete[](void*) noexcept { sys::fatal(safety::FaultType::kHeapUse, "operator delete[]", __LINE__); }
void operator delete(void*, std::size_t) noexcept {
  sys::fatal(safety::FaultType::kHeapUse, "operator delete", __LINE__);
}
void operator delete[](void*, std::size_t) noexcept {
  sys::fatal(safety::FaultType::kHeapUse, "operator delete[]", __LINE__);
}

// Even with -fno-exceptions, libstdc++ headers call these helpers when a checked operation
// fails (string_view::substr out of range, array::at...). The library versions construct
// and throw exception objects, which pulls in the unwinder, std::string and malloc. These
// definitions replace them with the fatal handler. Including the libstdc++ header makes a
// signature mismatch a compile error instead of a silent non-override.
#include <bits/functexcept.h>

namespace std {
void __throw_bad_alloc() { sys::fatal(safety::FaultType::kFatal, "std::bad_alloc", __LINE__); }
void __throw_length_error(const char*) { sys::fatal(safety::FaultType::kFatal, "std::length_error", __LINE__); }
void __throw_out_of_range(const char*) { sys::fatal(safety::FaultType::kFatal, "std::out_of_range", __LINE__); }
void __throw_out_of_range_fmt(const char*, ...) {
  sys::fatal(safety::FaultType::kFatal, "std::out_of_range", __LINE__);
}
void __throw_invalid_argument(const char*) {
  sys::fatal(safety::FaultType::kFatal, "std::invalid_argument", __LINE__);
}
void __throw_logic_error(const char*) { sys::fatal(safety::FaultType::kFatal, "std::logic_error", __LINE__); }
void __throw_bad_function_call() { sys::fatal(safety::FaultType::kFatal, "std::bad_function_call", __LINE__); }
}  // namespace std
