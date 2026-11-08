// Author: Raguram Kasthurisamy

// HAL timebase on TIM6 instead of SysTick.
//
// FreeRTOS owns SysTick. The HAL still needs a 1 ms tick for its timeouts (clock setup,
// UART init...), including before the scheduler starts, so it gets a basic timer of its
// own. These functions override the weak SysTick-based versions in stm32u5xx_hal.c; this
// only works because the BSP is built as a CMake OBJECT library (see board.cmake).

#include "bsp/hal.hpp"
#include "timer_clock.hpp"

extern "C" HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority) {
  if (TickPriority >= (1UL << __NVIC_PRIO_BITS)) {
    return HAL_ERROR;
  }

  __HAL_RCC_TIM6_CLK_ENABLE();

  const uint32_t timer_clock = bsp::detail::apb1_timer_clock();
  const uint32_t tick_hz = 1000U / static_cast<uint32_t>(uwTickFreq);

  TIM6->CR1 = 0U;                               // stop while reconfiguring
  TIM6->PSC = (timer_clock / 1'000'000U) - 1U;  // count at 1 MHz
  TIM6->ARR = (1'000'000U / tick_hz) - 1U;      // update event every tick
  TIM6->EGR = TIM_EGR_UG;                       // load PSC/ARR now
  TIM6->SR = 0U;                                // clear the UIF that UG just set
  TIM6->DIER = TIM_DIER_UIE;

  HAL_NVIC_SetPriority(TIM6_IRQn, TickPriority, 0U);
  HAL_NVIC_EnableIRQ(TIM6_IRQn);
  uwTickPrio = TickPriority;

  TIM6->CR1 = TIM_CR1_CEN;
  return HAL_OK;
}

// The weak defaults of these two would stop SysTick, which would freeze FreeRTOS.
extern "C" void HAL_SuspendTick(void) { TIM6->DIER &= ~TIM_DIER_UIE; }
extern "C" void HAL_ResumeTick(void) { TIM6->DIER |= TIM_DIER_UIE; }

extern "C" void TIM6_IRQHandler(void) {
  if ((TIM6->SR & TIM_SR_UIF) != 0U) {
    TIM6->SR = ~TIM_SR_UIF;  // rc_w0: writing 0 clears only UIF
    HAL_IncTick();
  }
}
