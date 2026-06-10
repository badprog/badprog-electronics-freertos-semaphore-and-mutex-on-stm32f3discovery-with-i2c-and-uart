// badprog.com
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// ---------------------------------------------------------------------------
// FreeRTOSConfig.h : kernel configuration for STM32F3Discovery (Cortex-M4)
//
// This file is the single place where the FreeRTOS kernel is tuned for the
// target hardware and application requirements.
//
// Every macro here is read by the FreeRTOS kernel source files at compile
// time. Changing a value here changes the behaviour of the entire kernel.
//
// Reference: https://www.freertos.org/a00110.html
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// SCHEDULER BEHAVIOUR
// ---------------------------------------------------------------------------

// configUSE_PREEMPTION
//   1 = preemptive scheduler (default and recommended)
//       The kernel can switch tasks at any tick interrupt, even if the
//       running task has not explicitly yielded. Higher-priority tasks
//       always get the CPU immediately when they become ready.
//   0 = cooperative scheduler
//       Tasks run until they call taskYIELD() or block.
#define configUSE_PREEMPTION                    1

// configUSE_TIME_SLICING
//   1 = tasks of equal priority share CPU time in round-robin fashion.
//       Each task gets one tick period before the scheduler moves on.
//   0 = a task runs until it blocks or yields even if peers are ready.
#define configUSE_TIME_SLICING                  1

// configUSE_PORT_OPTIMISED_TASK_SELECTION
//   1 = use the hardware CLZ instruction to find the highest-priority ready
//       task in O(1). Available on Cortex-M4. Limits configMAX_PRIORITIES to 32.
//   0 = generic C implementation, any number of priorities.
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

// configUSE_TICKLESS_IDLE
//   0 = tick runs continuously (simpler, no power saving).
#define configUSE_TICKLESS_IDLE                 0

// ---------------------------------------------------------------------------
// CLOCK AND TICK
// ---------------------------------------------------------------------------

// configCPU_CLOCK_HZ : frequency of the CPU clock in Hz.
// The STM32F3Discovery runs on the default HSI oscillator at 8 MHz.
#define configCPU_CLOCK_HZ                      ( 8000000UL )

// configTICK_RATE_HZ : how many times per second the tick interrupt fires.
// 1000 Hz = 1 ms tick period.
#define configTICK_RATE_HZ                      ( 1000 )

// configTICK_TYPE_WIDTH_IN_BITS : width of the tick counter.
// TICK_TYPE_WIDTH_32_BITS is correct for Cortex-M4.
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS

// ---------------------------------------------------------------------------
// TASK PRIORITIES AND STACK
// ---------------------------------------------------------------------------

// configMAX_PRIORITIES : number of distinct priority levels (0 = lowest).
// 5 levels for this project:
//   0 = idle (used internally by FreeRTOS)
//   1 = low   (led_task)
//   2 = normal (display_task, button_task)
//   3 = high  (sensor_task)
//   4 = reserved
#define configMAX_PRIORITIES                    ( 5 )

// configMINIMAL_STACK_SIZE : stack size in words for the idle task.
// 128 words = 512 bytes.
#define configMINIMAL_STACK_SIZE                ( 128 )

// configTOTAL_HEAP_SIZE : total FreeRTOS heap size in bytes.
// The STM32F303VCT6 has 40 KB of SRAM. We reserve 10 KB for FreeRTOS.
#define configTOTAL_HEAP_SIZE                   ( 10 * 1024 )

// configMAX_TASK_NAME_LEN : max characters in a task name (with null terminator).
#define configMAX_TASK_NAME_LEN                 ( 16 )

// ---------------------------------------------------------------------------
// HOOKS
// ---------------------------------------------------------------------------

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

// ---------------------------------------------------------------------------
// MEMORY ALLOCATION
// ---------------------------------------------------------------------------

#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

// ---------------------------------------------------------------------------
// STACK OVERFLOW DETECTION
// ---------------------------------------------------------------------------

// configCHECK_FOR_STACK_OVERFLOW = 2 : fill stack with a known pattern and
// verify at every context switch. Requires vApplicationStackOverflowHook().
#define configCHECK_FOR_STACK_OVERFLOW          2

// ---------------------------------------------------------------------------
// MALLOC FAILURE HOOK
// ---------------------------------------------------------------------------

#define configUSE_MALLOC_FAILED_HOOK            1

// ---------------------------------------------------------------------------
// RUNTIME STATS AND DEBUGGING
// ---------------------------------------------------------------------------

#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1
#define configGENERATE_RUN_TIME_STATS           0

// ---------------------------------------------------------------------------
// KERNEL FEATURES
// ---------------------------------------------------------------------------

#define configUSE_QUEUE_SETS                    0

// Mutexes : enabled for UART protection (xSemaphoreCreateMutex)
// Mutex includes priority inheritance to prevent priority inversion.
// Only the task that took the mutex can give it back.
#define configUSE_MUTEXES                       1

#define configUSE_RECURSIVE_MUTEXES             0

// Counting semaphores : not used in this exo
#define configUSE_COUNTING_SEMAPHORES           0

// Software timers : not used in this exo
#define configUSE_TIMERS                        0

// Task notifications : lightweight signaling, enabled for future use
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

// ---------------------------------------------------------------------------
// CORTEX-M4 INTERRUPT PRIORITY CONFIGURATION
//
// The Cortex-M4 NVIC uses LOWER numbers for HIGHER priority (0 = highest).
// FreeRTOS uses HIGHER numbers for HIGHER priority (0 = lowest).
// The two systems are inverted relative to each other.
//
// FreeRTOS manages interrupts with NVIC priority >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY.
// ISRs with NVIC priority < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (numerically lower
// = hardware higher priority) must NEVER call any FreeRTOS API function.
//
// The EXTI0 ISR for the button uses xSemaphoreGiveFromISR() which is a
// FreeRTOS API call. Its NVIC priority must therefore be >= 5.
// We set it to 6 in main.c (see NVIC_SetPriority).
//
// STM32F3 uses 4 bits for priority (16 levels: 0..15).
// ---------------------------------------------------------------------------

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 4) )

#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 4) )

// ---------------------------------------------------------------------------
// OPTIONAL API FUNCTIONS
// ---------------------------------------------------------------------------

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

// ---------------------------------------------------------------------------
// MAP FREERTOS HANDLERS TO CORTEX-M VECTOR TABLE NAMES
// ---------------------------------------------------------------------------

#define xPortPendSVHandler      PendSV_Handler
#define vPortSVCHandler         SVC_Handler
#define xPortSysTickHandler     SysTick_Handler

#endif // FREERTOS_CONFIG_H