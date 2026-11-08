// Author: Raguram Kasthurisamy

#pragma once

#include "bsp/board.hpp"

// Internal to this BSP: stop with a recorded reason if a hardware set-up step fails.
#define BSP_CHECK(expr)                                                         \
  do {                                                                          \
    if (!(expr)) {                                                              \
      bsp::fatal_error_hook("board init failed: " #expr, __FILE__, __LINE__);   \
    }                                                                           \
  } while (0)
