/*
 * Author: Raguram Kasthurisamy
 */
/*
 * Linker script for STM32U575ZIT6Q (2 MiB flash, 768 KiB contiguous SRAM1-3), written for
 * this project. SRAM4 (16 KiB at 0x28000000, retained in Stop 3) is unused for now.
 *
 * Layout decisions:
 *  - The main stack (MSP, used by startup code and interrupt handlers; tasks have their own
 *    stacks in .bss) sits at the BOTTOM of RAM, and bsp::init() loads its lower bound into
 *    the Cortex-M33 MSPLIM register. An overflow raises a UsageFault before a single byte
 *    below the stack is written, and even without MSPLIM it would run off the start of
 *    SRAM instead of silently corrupting .bss.
 *  - The last flash page holds the fault log; a .noinit RAM section carries a crash record
 *    across a reset.
 *  - There is no heap section. All memory is allocated statically; the C library's _sbrk
 *    is stubbed to fail (src/system/syscalls.cpp) and the build fails if malloc is linked.
 */

ENTRY(Reset_Handler)

/* Persistent fault log: the last 8 KiB flash page, kept out of the program image so that
 * reflashing the firmware does not erase it (a full-chip erase does). */
_fault_log_start = ORIGIN(FAULTLOG);
_fault_log_size = LENGTH(FAULTLOG);

_msp_stack_size = 0x1000;  /* 4 KiB for startup code + nested interrupts */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2040K
  FAULTLOG (r): ORIGIN = 0x081FE000, LENGTH = 8K    /* last page of bank 2 */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 768K
  SRAM4 (xrw) : ORIGIN = 0x28000000, LENGTH = 16K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  /* Main stack first in RAM. _smsp_stack = MSPLIM value, _estack = initial SP. */
  .msp_stack (NOLOAD) :
  {
    . = ALIGN(8);
    _smsp_stack = .;
    . = . + _msp_stack_size;
    . = ALIGN(8);
    _estack = .;
  } >RAM

  /* Initialized data: stored in flash, copied to RAM by Reset_Handler. */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* Zero-initialized data: task stacks, TCBs, DMA buffers and DMA linked-list nodes. */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* Survives a warm reset: startup code neither copies nor zeroes it. Holds the crash
   * record that the next boot reports. Contents are random after power-on, which is why
   * the record carries a magic number and a checksum. */
  .noinit (NOLOAD) :
  {
    . = ALIGN(8);
    *(.noinit)
    *(.noinit*)
    . = ALIGN(8);
  } >RAM

  _end_of_static_ram = .;

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

ASSERT(_end_of_static_ram <= ORIGIN(RAM) + LENGTH(RAM), "static RAM usage exceeds SRAM1-3")
