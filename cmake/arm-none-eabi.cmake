# Author: Raghuram Kasturi Sami
# Copyright (c) 2026 Raghuram Kasturi Sami
# Toolchain file: Arm GNU Toolchain (arm-none-eabi-*), bare metal.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "objcopy")
set(CMAKE_SIZE    arm-none-eabi-size    CACHE FILEPATH "size")
set(CMAKE_NM      arm-none-eabi-nm      CACHE FILEPATH "nm")

# CMake's compiler checks must not try to link an executable: that needs our linker script.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Release keeps -g: debug info stays in the .elf on the PC, it never reaches flash.
set(CMAKE_C_FLAGS_DEBUG_INIT     "-Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG_INIT   "-Og -g3")
set(CMAKE_ASM_FLAGS_DEBUG_INIT   "-g3")
set(CMAKE_C_FLAGS_RELEASE_INIT   "-Os -g")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os -g")
set(CMAKE_ASM_FLAGS_RELEASE_INIT "-g")
