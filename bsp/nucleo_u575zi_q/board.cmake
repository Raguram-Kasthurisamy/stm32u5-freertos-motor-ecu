# Author: Raguram Kasthurisamy
# NUCLEO-U575ZI-Q: STM32U575ZIT6Q, Cortex-M33 with FPU and TrustZone, 160 MHz,
# 2 MiB flash, 784 KiB SRAM, internal SMPS ("-Q" package).
set(BOARD_DIR ${CMAKE_CURRENT_LIST_DIR})
set(BOARD_LINKER_SCRIPT ${BOARD_DIR}/STM32U575ZITXQ_FLASH.ld)
set(BOARD_OPENOCD_TARGET stm32u5x.cfg)

# FreeRTOS ARMv8-M port without TrustZone (the firmware runs with TZEN=0).
set(FREERTOS_PORT GCC/ARM_CM33_NTZ/non_secure)
set(FREERTOS_PORT_FILES port.c portasm.c)

# CPU flags apply to every target (C, C++, assembly) and to the link, so the right
# hard-float multilib of newlib and libstdc++ is selected.
set(BOARD_CPU_FLAGS -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
add_compile_options(${BOARD_CPU_FLAGS})
add_link_options(${BOARD_CPU_FLAGS})

set(THIRD_PARTY ${CMAKE_SOURCE_DIR}/third_party)
foreach(dep CMSIS_5 cmsis_device_u5 stm32u5xx_hal_driver)
  if(NOT EXISTS ${THIRD_PARTY}/${dep}/.git)
    message(FATAL_ERROR
      "third_party/${dep} is missing. First time: run scripts/bootstrap.sh. "
      "Fresh clone: git submodule update --init")
  endif()
endforeach()

set(CMSIS_DEVICE_DIR ${THIRD_PARTY}/cmsis_device_u5)
set(HAL_DIR ${THIRD_PARTY}/stm32u5xx_hal_driver)

# Vendor code: ST startup file, CMSIS system file and the HAL modules in use.
# OBJECT library so the BSP's strong overrides of weak HAL symbols always take effect.
add_library(stm32_hal OBJECT
  ${CMSIS_DEVICE_DIR}/Source/Templates/gcc/startup_stm32u575xx.s
  ${CMSIS_DEVICE_DIR}/Source/Templates/system_stm32u5xx.c
  ${HAL_DIR}/Src/stm32u5xx_hal.c
  ${HAL_DIR}/Src/stm32u5xx_hal_adc.c
  ${HAL_DIR}/Src/stm32u5xx_hal_adc_ex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_dma.c
  ${HAL_DIR}/Src/stm32u5xx_hal_dma_ex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_fdcan.c
  ${HAL_DIR}/Src/stm32u5xx_hal_flash.c
  ${HAL_DIR}/Src/stm32u5xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32u5xx_hal_icache.c
  ${HAL_DIR}/Src/stm32u5xx_hal_iwdg.c
  ${HAL_DIR}/Src/stm32u5xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32u5xx_hal_pwr_ex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32u5xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_tim.c
  ${HAL_DIR}/Src/stm32u5xx_hal_tim_ex.c
  ${HAL_DIR}/Src/stm32u5xx_hal_uart.c
  ${HAL_DIR}/Src/stm32u5xx_hal_uart_ex.c
)
target_include_directories(stm32_hal SYSTEM PUBLIC
  ${THIRD_PARTY}/CMSIS_5/CMSIS/Core/Include
  ${CMSIS_DEVICE_DIR}/Include
  ${HAL_DIR}/Inc
  ${BOARD_DIR}/include            # stm32u5xx_hal_conf.h and bsp/hal.hpp
)
target_compile_definitions(stm32_hal PUBLIC STM32U575xx USE_HAL_DRIVER)

# Our board code: power, clocks, pins, interrupt and fault handlers, HAL timebase,
# motor power stage and sensors, control tick and cycle counter, CAN, watchdog, fault-log flash.
add_library(board OBJECT
  ${BOARD_DIR}/board.cpp
  ${BOARD_DIR}/can_hw.cpp
  ${BOARD_DIR}/hal_timebase.cpp
  ${BOARD_DIR}/log_flash.cpp
  ${BOARD_DIR}/motor_hw.cpp
  ${BOARD_DIR}/timing.cpp
  ${BOARD_DIR}/watchdog.cpp
)
target_include_directories(board PUBLIC ${CMAKE_SOURCE_DIR}/bsp/include)
target_link_libraries(board PRIVATE ecu_lib)   # protocol::CanFrame in the CAN interface
target_link_libraries(board
  PUBLIC stm32_hal freertos_kernel   # usage requirements (headers, defines) only
  PRIVATE ecu_warnings)
