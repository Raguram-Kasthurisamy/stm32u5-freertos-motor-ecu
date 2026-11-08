# Author: Raghuram Kasturi Sami
# Copyright (c) 2026 Raghuram Kasturi Sami
# Post-build check: fail the build if a heap allocator was linked into the firmware.
# Usage: cmake -DNM=<arm-none-eabi-nm> -DELF=<firmware.elf> -P check_no_heap.cmake
execute_process(
  COMMAND ${NM} ${ELF}
  OUTPUT_VARIABLE symbols
  RESULT_VARIABLE nm_result)
if(NOT nm_result EQUAL 0)
  message(FATAL_ERROR "check_no_heap: '${NM} ${ELF}' failed")
endif()

string(REGEX MATCHALL " (malloc|_malloc_r|calloc|_calloc_r|realloc|_realloc_r)\n" found "${symbols}")
if(found)
  string(REPLACE "\n" "" found "${found}")
  message(FATAL_ERROR
    "Heap allocator linked into ${ELF}:${found}\n"
    "Something called malloc. Find the caller in the cross-reference of the .map file, "
    "or link once with -Wl,--trace-symbol=malloc.")
endif()
