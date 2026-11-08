// Author: Raguram Kasthurisamy

// newlib system call overrides.
//
// _sbrk is how malloc obtains memory. There is deliberately no heap region in the linker
// script, so it always fails: if something manages to call malloc it gets NULL instead of
// quietly scribbling over the stack. CI additionally fails the build if malloc is linked.

#include <cerrno>
#include <cstddef>

extern "C" void* _sbrk(std::ptrdiff_t increment) {
  (void)increment;
  errno = ENOMEM;
  return reinterpret_cast<void*>(-1);
}
