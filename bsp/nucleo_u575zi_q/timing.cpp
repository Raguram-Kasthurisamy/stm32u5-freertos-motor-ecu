// Author: Raguram Kasthurisamy

// Control-loop tick on TIM7 and the DWT cycle counter.

#include "bsp/timing.hpp"

#include "bsp/hal.hpp"
#include "bsp/irq_priority.hpp"
#include "timer_clock.hpp"

namespace bsp {
namespace {
TickCallback g_tick_callback = nullptr;  // set before the interrupt is enabled
}  // namespace

void init_cycle_counter() {
#if defined(DCB)  // ARMv8-M naming in CMSIS 5.8+
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
#else
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#endif
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

std::uint32_t cycle_count() { return DWT->CYCCNT; }

void control_tick_start(std::uint32_t rate_hz, TickCallback callback) {
  g_tick_callback = callback;
  __HAL_RCC_TIM7_CLK_ENABLE();

  const uint32_t timer_clock = detail::apb1_timer_clock();
  TIM7->CR1 = 0U;
  TIM7->PSC = (timer_clock / 1'000'000U) - 1U;  // 1 MHz count
  TIM7->ARR = (1'000'000U / rate_hz) - 1U;
  TIM7->EGR = TIM_EGR_UG;
  TIM7->SR = 0U;
  TIM7->DIER = TIM_DIER_UIE;

  HAL_NVIC_SetPriority(TIM7_IRQn, irq_priority::kControlTick, 0U);
  HAL_NVIC_EnableIRQ(TIM7_IRQn);
  TIM7->CR1 = TIM_CR1_CEN;
}

}  // namespace bsp

extern "C" void TIM7_IRQHandler(void) {
  if ((TIM7->SR & TIM_SR_UIF) != 0U) {
    TIM7->SR = ~TIM_SR_UIF;
    if (bsp::g_tick_callback != nullptr) {
      bsp::g_tick_callback();
    }
  }
}
