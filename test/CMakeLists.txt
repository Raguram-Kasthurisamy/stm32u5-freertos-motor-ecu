# Author: Raguram Kasthurisamy
# Host-side unit tests for the hardware-independent code in lib/.
#   cmake -S test -B build/test -G Ninja && cmake --build build/test && ctest --test-dir build/test
cmake_minimum_required(VERSION 3.22)
project(ecu_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ECU_WERROR "Treat warnings in project code as errors" OFF)
option(ECU_SANITIZE "Build tests with AddressSanitizer and UBSan" ON)

if(ECU_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib ${CMAKE_BINARY_DIR}/lib)

add_executable(host_tests
  test_cli.cpp
  test_closed_loop.cpp
  test_command_monitor.cpp
  test_crash_record.cpp
  test_dc_motor_model.cpp
  test_parse.cpp
  test_pid.cpp
  test_protocol.cpp
  test_speed_controller.cpp
  test_speed_estimator.cpp
  test_task_monitor.cpp
  test_text_writer.cpp
)
# src/ for app/motor_config.hpp: the tests validate the firmware's real tuning values.
target_include_directories(host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(host_tests PRIVATE ecu_lib GTest::gtest_main)
target_compile_options(host_tests PRIVATE -Wall -Wextra $<$<BOOL:${ECU_WERROR}>:-Werror>)

include(GoogleTest)
gtest_discover_tests(host_tests)
