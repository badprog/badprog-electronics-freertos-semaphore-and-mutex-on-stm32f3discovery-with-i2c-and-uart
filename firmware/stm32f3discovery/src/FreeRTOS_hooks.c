// badprog.com
/**
 * @file    FreeRTOS_hooks.c
 * @brief   Mandatory FreeRTOS application hooks for STM32F3Discovery
 *
 * FreeRTOS requires the application to define certain callback functions
 * (called "hooks") when the corresponding feature is enabled in
 * FreeRTOSConfig.h. If a hook is enabled but not defined, the linker
 * will produce an undefined reference error.
 *
 * Hooks enabled in FreeRTOSConfig.h for this project:
 *   configUSE_MALLOC_FAILED_HOOK   = 1 : vApplicationMallocFailedHook()
 *   configCHECK_FOR_STACK_OVERFLOW = 2 : vApplicationStackOverflowHook()
 *
 * Both hooks turn on an error LED and halt the system in an infinite loop.
 * This makes failures immediately visible during development without
 * requiring a debugger.
 *
 * -------------------------------------------------------------------------
 * HOOKS GLOSSARY
 * -------------------------------------------------------------------------
 *
 * vApplicationMallocFailedHook():
 *   Called by pvPortMalloc() when it cannot satisfy an allocation request
 *   because the FreeRTOS heap (configTOTAL_HEAP_SIZE) is exhausted.
 *   Common causes:
 *     - Too many tasks created (each allocates its stack from the heap)
 *     - Stack sizes too large
 *     - configTOTAL_HEAP_SIZE too small for the application
 *
 * vApplicationStackOverflowHook():
 *   Called by the kernel when it detects that a task has overflowed its
 *   stack. With configCHECK_FOR_STACK_OVERFLOW = 2, the last 20 bytes of
 *   each stack are filled with a known pattern at creation time and checked
 *   at every context switch.
 *   Common causes:
 *     - Stack size too small for the task's local variables
 *     - Recursive functions consuming more stack than expected
 *     - Large local arrays allocated on the stack
 *   The pcTaskName parameter contains the name of the offending task.
 * -------------------------------------------------------------------------
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f303xc.h"

// PE9 is used as the error LED (same convention as the SPI gyroscope exo)
#define ERROR_LED_PIN   9U

// ---------------------------------------------------------------------------
// Static helper : turn on PE9 (error LED) and halt
// ---------------------------------------------------------------------------

static void halt_with_error_led(void)
{
    // Make sure GPIOE clock is enabled (defensive : may fire before main() inits)
    RCC->AHBENR |= RCC_AHBENR_IOPEEN;

    // Set PE9 to output mode
    GPIOE->MODER &= ~(0x3UL << (ERROR_LED_PIN * 2));
    GPIOE->MODER |=  (0x1UL << (ERROR_LED_PIN * 2));

    // Turn PE9 on via BSRR set half
    GPIOE->BSRR = (1UL << ERROR_LED_PIN);

    // Disable all maskable interrupts and spin forever.
    // "cpsid i" sets the PRIMASK register on Cortex-M4, blocking all
    // interrupts with configurable priority. The system stays frozen
    // and the LED stays on until a hardware reset.
    __asm volatile ("cpsid i" ::: "memory");
    while (1);
}

// ---------------------------------------------------------------------------
// vApplicationMallocFailedHook
// Called when pvPortMalloc() returns NULL (heap exhausted)
// Enabled by: configUSE_MALLOC_FAILED_HOOK = 1
// ---------------------------------------------------------------------------

void vApplicationMallocFailedHook(void)
{
    halt_with_error_led();
}

// ---------------------------------------------------------------------------
// vApplicationStackOverflowHook
// Called when the kernel detects a task stack overflow
// Enabled by: configCHECK_FOR_STACK_OVERFLOW = 2
//
// xTask     : handle of the task that overflowed its stack
// pcTaskName: name of the offending task (useful for debugging)
// ---------------------------------------------------------------------------

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // Suppress unused parameter warnings.
    // We cannot safely print pcTaskName here because UART may not be
    // accessible from inside a hook (the UART task may hold a mutex).
    // In a real project, write pcTaskName to a known RAM address and
    // read it with a JTAG debugger after the halt.
    (void)xTask;
    (void)pcTaskName;

    halt_with_error_led();
}