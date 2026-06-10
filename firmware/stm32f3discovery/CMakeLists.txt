# badprog.com
cmake_minimum_required(VERSION 3.20)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project("stm32f3discovery" C ASM)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ---------------------------------------------------------------------------
# FreeRTOS kernel location
# Set by the Dockerfile via ENV FREERTOS_DIR=/opt/FreeRTOS-Kernel-11.3.0
# ---------------------------------------------------------------------------
set(FREERTOS_DIR $ENV{FREERTOS_DIR})

if(NOT FREERTOS_DIR)
    message(FATAL_ERROR "FREERTOS_DIR is not set. Rebuild the devcontainer.")
endif()

message(STATUS "FreeRTOS kernel: ${FREERTOS_DIR}")

# ---------------------------------------------------------------------------
# STM32F303VCT6 : Cortex-M4 with hardware FPU
# ---------------------------------------------------------------------------
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

# ---------------------------------------------------------------------------
# Application sources
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.c
    src/startup_stm32f303xc.s
    src/sysmem.c
    src/FreeRTOS_hooks.c
)

# ---------------------------------------------------------------------------
# FreeRTOS kernel sources
#
# We include only the files strictly required for this project:
#   tasks.c       : task creation, scheduler, context switch
#   queue.c       : queues (also used internally by semaphores and mutexes)
#   list.c        : doubly linked list used internally by the kernel
#   timers.c      : software timers (not used in exo07 but harmless to include)
#   event_groups.c: event groups (not used in exo07, included for completeness)
#   heap_4.c      : dynamic memory allocator with coalescence
#                   heap_4 is the best general-purpose choice:
#                     - supports malloc + free (unlike heap_1 and heap_2)
#                     - coalesces adjacent free blocks (unlike heap_2)
#                     - simpler than heap_5 (no multiple memory regions)
#
# The port file (port.c) provides the Cortex-M4 context switch implementation.
# GCC/ARM_CM4F is the correct port for Cortex-M4 with hardware FPU.
# ---------------------------------------------------------------------------
set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/event_groups.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${FREERTOS_SOURCES}
)

# ---------------------------------------------------------------------------
# Include directories
#
# inc/               : application headers (stm32f303xc.h, FreeRTOSConfig.h)
# FreeRTOS include/  : kernel public headers (FreeRTOS.h, task.h, queue.h...)
# GCC/ARM_CM4F/      : port-specific header (portmacro.h)
# ---------------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    inc/
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
)

# ---------------------------------------------------------------------------
# Compiler options
# ---------------------------------------------------------------------------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F303xC
    $<$<CONFIG:Debug>:DEBUG>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${CPU_FLAGS}
    -fdata-sections
    -ffunction-sections
    -fstack-usage
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
    $<$<CONFIG:Debug>:-O0 -g3 -ggdb>
    $<$<CONFIG:Release>:-Os>
)

# ---------------------------------------------------------------------------
# Linker options
#
# nosys.specs  : minimal syscall stubs (no semihosting, no file I/O)
#                Use this instead of rdimon.specs now that we have UART output
# nano.specs   : smaller printf/scanf from newlib-nano (no float by default)
# ---------------------------------------------------------------------------
target_link_options(${PROJECT_NAME} PRIVATE
    ${CPU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/stm32f303xc_flash.ld
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nosys.specs
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,--start-group
    -lc
    -lm
    -Wl,--end-group
)

# ---------------------------------------------------------------------------
# Post-build: generate HEX and BIN files
# ---------------------------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND arm-none-eabi-size   $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND arm-none-eabi-objcopy -O ihex   $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN..."
)