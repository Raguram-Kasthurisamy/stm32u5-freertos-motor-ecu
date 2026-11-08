# Author: Raghuram Kasturi Sami
# Copyright (c) 2026 Raghuram Kasturi Sami
# Warning set for firmware code we own. Vendor code (HAL, CMSIS, FreeRTOS) is compiled
# without it and its headers are included as SYSTEM, so its warnings do not drown ours.
add_library(ecu_warnings INTERFACE)
target_compile_options(ecu_warnings INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wdouble-promotion -Wformat=2 -Wundef -Wcast-align
  $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
  $<$<BOOL:${ECU_WERROR}>:-Werror>
)
