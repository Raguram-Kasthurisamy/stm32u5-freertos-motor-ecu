/*
 * Author: Raghuram Kasturi Sami
 * Copyright (c) 2026 Raghuram Kasturi Sami
 */
/*
 * HAL configuration for NUCLEO-U575ZI-Q, based on ST's stm32u5xx_hal_conf_template.h.
 * Only the modules the firmware uses are enabled; each phase adds what it needs.
 */
#ifndef STM32U5xx_HAL_CONF_H
#define STM32U5xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Module selection ---------------------------------------------------------------- */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FDCAN_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_ICACHE_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ---- Oscillator values --------------------------------------------------------------- */
#define HSE_VALUE                   16000000UL  /* no HSE is used on this board */
#define HSE_STARTUP_TIMEOUT         100UL       /* ms */
#define MSI_VALUE                   4000000UL
#define HSI_VALUE                   16000000UL
#define HSI48_VALUE                 48000000UL
#define LSI_VALUE                   32000UL
#define LSE_VALUE                   32768UL     /* X2 crystal on the Nucleo */
#define LSE_STARTUP_TIMEOUT         5000UL      /* ms: crystals can take a second or more */
#define EXTERNAL_SAI1_CLOCK_VALUE   48000UL
#define EXTERNAL_SAI2_CLOCK_VALUE   48000UL

/* ---- System configuration ------------------------------------------------------------ */
#define VDD_VALUE                   3300UL      /* mV */
#define TICK_INT_PRIORITY           15UL        /* HAL timebase (TIM6): least urgent */
#define USE_RTOS                    0U          /* must stay 0: the HAL's RTOS hook is unimplemented */
#define PREFETCH_ENABLE             1U

/* Weak global callbacks (HAL_UART_TxCpltCallback etc.) instead of registered ones. */
#define USE_HAL_ADC_REGISTER_CALLBACKS      0U
#define USE_HAL_FDCAN_REGISTER_CALLBACKS    0U
#define USE_HAL_I2C_REGISTER_CALLBACKS      0U
#define USE_HAL_SPI_REGISTER_CALLBACKS      0U
#define USE_HAL_TIM_REGISTER_CALLBACKS      0U
#define USE_HAL_UART_REGISTER_CALLBACKS     0U
#define USE_HAL_USART_REGISTER_CALLBACKS    0U

#define USE_HAL_SECURE_CHECK_PARAM          0U
#define USE_SPI_CRC                         1U

/* ---- Assert --------------------------------------------------------------------------- */
#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

/* ---- Module headers (order follows the ST template) ---------------------------------- */
#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32u5xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32u5xx_hal_gpio.h"
#endif
#ifdef HAL_ICACHE_MODULE_ENABLED
#include "stm32u5xx_hal_icache.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32u5xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32u5xx_hal_cortex.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32u5xx_hal_adc.h"
#endif
#ifdef HAL_FDCAN_MODULE_ENABLED
#include "stm32u5xx_hal_fdcan.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32u5xx_hal_flash.h"
#endif
#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32u5xx_hal_iwdg.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32u5xx_hal_pwr.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32u5xx_hal_tim.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32u5xx_hal_uart.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32U5xx_HAL_CONF_H */
