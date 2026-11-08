// Author: Raguram Kasthurisamy

// Independent watchdog on the U5's 32 kHz LSI oscillator.

#include "bsp/watchdog.hpp"

#include "bsp/hal.hpp"
#include "bsp_check.hpp"

namespace {
IWDG_HandleTypeDef g_iwdg{};
}  // namespace

namespace bsp::watchdog {

void start(std::uint32_t timeout_ms) {
#ifdef __HAL_RCC_DBGMCU_CLK_ENABLE
  __HAL_RCC_DBGMCU_CLK_ENABLE();
#endif
#ifdef __HAL_DBGMCU_FREEZE_IWDG
  __HAL_DBGMCU_FREEZE_IWDG();  // a debugger breakpoint must not trigger a reset
#endif
  if (timeout_ms < 1U) {
    timeout_ms = 1U;
  }
  if (timeout_ms > 4096U) {
    timeout_ms = 4096U;
  }
  g_iwdg.Instance = IWDG;
  g_iwdg.Init.Prescaler = IWDG_PRESCALER_32;  // 32 kHz / 32 = 1 count per millisecond
  g_iwdg.Init.Reload = timeout_ms - 1U;
  g_iwdg.Init.Window = IWDG_WINDOW_DISABLE;
  BSP_CHECK(HAL_IWDG_Init(&g_iwdg) == HAL_OK);
}

void feed() { (void)HAL_IWDG_Refresh(&g_iwdg); }

}  // namespace bsp::watchdog
