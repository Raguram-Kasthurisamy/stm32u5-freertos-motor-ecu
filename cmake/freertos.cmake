# Author: Raguram Kasthurisamy
# FreeRTOS kernel from the submodule, built with config/FreeRTOSConfig.h.
# FREERTOS_PORT and FREERTOS_PORT_FILES come from the board's board.cmake.
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/third_party/FreeRTOS-Kernel)
if(NOT EXISTS ${FREERTOS_DIR}/tasks.c)
  message(FATAL_ERROR
    "third_party/FreeRTOS-Kernel is missing. First time: run scripts/bootstrap.sh. "
    "Fresh clone: git submodule update --init")
endif()

list(TRANSFORM FREERTOS_PORT_FILES PREPEND ${FREERTOS_DIR}/portable/${FREERTOS_PORT}/
     OUTPUT_VARIABLE FREERTOS_PORT_SOURCES)

# OBJECT library: the port defines SVC_Handler, PendSV_Handler and SysTick_Handler, which
# must replace the weak defaults in the startup file. Objects of an OBJECT library are
# always linked, so the strong definitions always win.
add_library(freertos_kernel OBJECT
  ${FREERTOS_DIR}/tasks.c
  ${FREERTOS_DIR}/queue.c
  ${FREERTOS_DIR}/list.c
  ${FREERTOS_DIR}/timers.c
  ${FREERTOS_DIR}/stream_buffer.c
  ${FREERTOS_DIR}/event_groups.c
  ${FREERTOS_PORT_SOURCES}
  # No portable/MemMang/heap_N.c: configSUPPORT_DYNAMIC_ALLOCATION is 0, there is no heap.
)
target_include_directories(freertos_kernel SYSTEM PUBLIC
  ${FREERTOS_DIR}/include
  ${FREERTOS_DIR}/portable/${FREERTOS_PORT}
  ${CMAKE_SOURCE_DIR}/config
)
