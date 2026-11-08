// Author: Raguram Kasthurisamy

#pragma once

#include "bsp/hal.hpp"

// Internal to this BSP: the input clock of timers on each APB bus. A timer runs at 2x the
// bus clock whenever that bus's prescaler is not 1 (RM0456, RCC chapter).
namespace bsp::detail {

inline uint32_t apb1_timer_clock() {
  RCC_ClkInitTypeDef clocks{};
  uint32_t flash_latency = 0;
  HAL_RCC_GetClockConfig(&clocks, &flash_latency);
  const uint32_t pclk = HAL_RCC_GetPCLK1Freq();
  return clocks.APB1CLKDivider == RCC_HCLK_DIV1 ? pclk : 2U * pclk;
}

inline uint32_t apb2_timer_clock() {
  RCC_ClkInitTypeDef clocks{};
  uint32_t flash_latency = 0;
  HAL_RCC_GetClockConfig(&clocks, &flash_latency);
  const uint32_t pclk = HAL_RCC_GetPCLK2Freq();
  return clocks.APB2CLKDivider == RCC_HCLK_DIV1 ? pclk : 2U * pclk;
}

}  // namespace bsp::detail
