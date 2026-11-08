# Author: Raguram Kasthurisamy
# Hardware-independent code. Built twice: for the target (as part of the firmware) and for
# the host (by test/CMakeLists.txt), so it must not include HAL, CMSIS or FreeRTOS headers.
add_library(ecu_lib STATIC
  cli/cli.cpp
  cli/parse.cpp
  control/dc_motor_model.cpp
  control/pid.cpp
  control/simulated_motor_io.cpp
  control/speed_controller.cpp
  control/speed_estimator.cpp
  io/text_writer.cpp
  protocol/command_monitor.cpp
  protocol/ecu_protocol.cpp
  safety/crash_record.cpp
  safety/fault_log_format.cpp
  safety/task_monitor.cpp
)

target_include_directories(ecu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(ecu_lib PUBLIC cxx_std_20)

# This code is verified on the host with the full warning set, so hold it to it everywhere.
target_compile_options(ecu_lib PRIVATE
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
  $<$<BOOL:${ECU_WERROR}>:-Werror>
)
