/*
 * Author: Raghuram Kasturi Sami
 * Copyright (c) 2026 Raghuram Kasturi Sami
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*
 * FreeRTOS configuration for the ECU firmware (FreeRTOS-Kernel V11.x, Cortex-M33 / ARMv8-M).
 * Reference: https://www.freertos.org/a00110.html
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t SystemCoreClock;  /* defined by the CMSIS system_<family>.c file */
void app_assert_failed(const char *file, int line) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif

/* ---- Scheduler ---------------------------------------------------------------------- */
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0   /* generic C selection: fine for 8 priorities */
#define configUSE_TICKLESS_IDLE                  0
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       1000
#define configMAX_PRIORITIES                     8
#define configMINIMAL_STACK_SIZE                 128  /* words */
#define configMAX_TASK_NAME_LEN                  12
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                  1
#define configSTACK_DEPTH_TYPE                   uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

/* ---- Memory: everything is statically allocated, there is no FreeRTOS heap ---------- */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      0   /* we provide idle-task memory ourselves */

/* ---- Features ------------------------------------------------------------------------ */
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_QUEUE_SETS                     0
#define configUSE_STREAM_BUFFERS                 1
#define configUSE_EVENT_GROUPS                   1
#define configUSE_TIMERS                         0
#define configUSE_CO_ROUTINES                    0
#define configQUEUE_REGISTRY_SIZE                8   /* names queues in the debugger */
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS  0

/* ---- Hooks and debugging ------------------------------------------------------------ */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0
#define configCHECK_FOR_STACK_OVERFLOW           2   /* software check, on top of PSPLIM */
#define configUSE_TRACE_FACILITY                 1   /* needed by uxTaskGetSystemState() */
#define configUSE_STATS_FORMATTING_FUNCTIONS     0   /* we format ourselves, no sprintf */
#define configGENERATE_RUN_TIME_STATS            0   /* enabled in a later phase */

/* Asserts stay enabled in release builds: a firmware that detects a broken invariant
 * should stop in a known way, not keep running in an unknown state. */
#define configASSERT(x)                                    \
  do {                                                     \
    if ((x) == 0) {                                        \
      app_assert_failed(__FILE__, __LINE__);               \
    }                                                      \
  } while (0)

/* ---- ARMv8-M port (GCC/ARM_CM33_NTZ) ------------------------------------------------
 * TrustZone is disabled on the device (TZEN = 0), so the firmware runs non-secure only.
 * The port loads each task's stack bottom into PSPLIM on every context switch, so a task
 * that overflows its stack faults immediately in hardware instead of corrupting memory. */
#define configENABLE_FPU                         1
#define configENABLE_MPU                         0
#define configENABLE_TRUSTZONE                   0
#define configRUN_FREERTOS_SECURE_ONLY           0
#define configENABLE_MVE                         0

/* ---- Cortex-M interrupt priorities --------------------------------------------------
 * STM32U5 implements 4 priority bits (0 = most urgent, 15 = least urgent).
 * Interrupts with a priority value numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
 * may call FreeRTOS "FromISR" APIs. More urgent interrupts (0..4) are never masked by the
 * kernel and must not call any FreeRTOS function; they are reserved for hard real-time
 * work such as the motor-control safety path in a later phase. */
#define configPRIO_BITS                                  4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY          15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY     5
#define configKERNEL_INTERRUPT_PRIORITY \
  (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
  (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---- Optional API functions ---------------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      0
#define INCLUDE_vTaskSuspend                     1   /* lets portMAX_DELAY block forever */
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_eTaskGetState                    1

/* ---- Map the kernel's handlers straight into the vector table ------------------------ */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
