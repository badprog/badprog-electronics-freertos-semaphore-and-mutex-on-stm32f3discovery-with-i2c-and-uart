// badprog.com
/**
 * @file    sysmem.c
 * @brief   Heap management via _sbrk for newlib.
 *          Grows the heap upward and guards against collision with the stack.
 */

#include <errno.h>
#include <stdint.h>

extern uint32_t _end;           /* End of .bss — heap starts here (linker script) */
extern uint32_t _estack;        /* Top of RAM — stack starts here (linker script) */
extern uint32_t _Min_Stack_Size;

void *_sbrk(int incr)
{
    static uint8_t *heap_end = NULL;
    uint8_t        *prev_heap_end;

    if (heap_end == NULL) {
        heap_end = (uint8_t *)&_end;
    }

    prev_heap_end = heap_end;

    uintptr_t stack_limit = (uintptr_t)&_estack - (uintptr_t)&_Min_Stack_Size;
    if ((uintptr_t)(heap_end + incr) > stack_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev_heap_end;
}
