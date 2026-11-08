// Author: Raguram Kasthurisamy

#pragma once

#include "bsp/hal.hpp"

// HAL handles owned by the BSP. Drivers receive these by reference instead of reaching
// for globals, which keeps "which UART is the console" a board decision.
namespace bsp {

UART_HandleTypeDef& console_uart();

}  // namespace bsp
