# Author: Raguram Kasthurisamy
# Firmware build. Configure with a preset:
#   cmake --preset debug && cmake --build --preset debug
# Host unit tests are a separate project in test/.
cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR
    "No toolchain file. Configure with a preset (cmake --preset debug) or pass "
    "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake. Host tests live in test/.")
endif()

project(ecu_firmware VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ECU_BOARD "nucleo_u575zi_q" CACHE STRING "Board support package to build (a folder under bsp/)")
option(ECU_WERROR "Treat warnings in project code as errors" OFF)

# Flags for every target, vendor code included.
add_compile_options(
  $<$<COMPILE_LANGUAGE:C,CXX>:-ffunction-sections>
  $<$<COMPILE_LANGUAGE:C,CXX>:-fdata-sections>
  $<$<COMPILE_LANGUAGE:C,CXX>:--specs=nano.specs>
  # Short __FILE__ paths in assert messages: "src/drivers/uart_dma.cpp", not "/home/...".
  $<$<COMPILE_LANGUAGE:C,CXX>:-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  # No locks around function-local statics: they are only initialized from one task.
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)

include(cmake/warnings.cmake)
include(bsp/${ECU_BOARD}/board.cmake)   # CPU flags, vendor HAL, BSP, FREERTOS_PORT
include(cmake/freertos.cmake)
add_subdirectory(lib)

# Version string baked into the image, from `git describe` (refreshed on re-configure).
set(ECU_FW_VERSION "${PROJECT_VERSION}-nogit")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE git_describe
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE git_result)
  if(git_result EQUAL 0 AND git_describe)
    set(ECU_FW_VERSION "${git_describe}")
  endif()
endif()
message(STATUS "Firmware version: ${ECU_FW_VERSION}")

add_executable(ecu_firmware
  src/main.cpp
  src/app/app.cpp
  src/app/cli_commands.cpp
  src/app/can_task.cpp
  src/app/cli_task.cpp
  src/app/control_task.cpp
  src/app/heartbeat_task.cpp
  src/app/motor_commands.cpp
  src/app/supervisor_task.cpp
  src/app/system_commands.cpp
  src/app/telemetry_task.cpp
  src/drivers/motor_hardware_io.cpp
  src/drivers/uart_dma.cpp
  src/os/freertos_hooks.cpp
  src/system/boot_report.cpp
  src/system/crash_store.cpp
  src/system/cxx_support.cpp
  src/system/fatal.cpp
  src/system/fault_log.cpp
  src/system/syscalls.cpp
)
set_target_properties(ecu_firmware PROPERTIES
  SUFFIX ".elf"
  LINK_DEPENDS "${BOARD_LINKER_SCRIPT}")
target_include_directories(ecu_firmware PRIVATE src)
target_compile_definitions(ecu_firmware PRIVATE ECU_FW_VERSION="${ECU_FW_VERSION}")

# board, stm32_hal and freertos_kernel are OBJECT libraries and must be listed here
# directly: only directly linked OBJECT libraries contribute their object files.
target_link_libraries(ecu_firmware PRIVATE board stm32_hal freertos_kernel ecu_lib ecu_warnings)
target_link_options(ecu_firmware PRIVATE
  -T${BOARD_LINKER_SCRIPT}
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--gc-sections
  -Wl,-Map=$<TARGET_FILE_DIR:ecu_firmware>/ecu_firmware.map
  -Wl,--print-memory-usage
)

add_custom_command(TARGET ecu_firmware POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DELF=$<TARGET_FILE:ecu_firmware>
          -P ${CMAKE_SOURCE_DIR}/cmake/check_no_heap.cmake
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:ecu_firmware>
          $<TARGET_FILE_DIR:ecu_firmware>/ecu_firmware.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:ecu_firmware>
          $<TARGET_FILE_DIR:ecu_firmware>/ecu_firmware.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:ecu_firmware>
  VERBATIM)

find_program(OPENOCD openocd)
if(OPENOCD)
  add_custom_target(flash
    COMMAND ${OPENOCD} -f interface/stlink.cfg -f target/${BOARD_OPENOCD_TARGET}
            -c "program $<TARGET_FILE:ecu_firmware> verify reset exit"
    DEPENDS ecu_firmware
    USES_TERMINAL
    VERBATIM)
endif()