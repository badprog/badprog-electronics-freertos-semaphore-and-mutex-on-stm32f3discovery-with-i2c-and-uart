// badprog.com
/**
 * @file    main.c
 * @brief   FreeRTOS semaphores and mutexes on STM32F3Discovery
 *
 * Bare metal C with FreeRTOS. No HAL, no CubeMX.
 *
 * This exercise builds on the previous one (tasks and queues) and adds
 * two synchronisation mechanisms:
 *
 *   1. Mutex for UART protection
 *      display_task and button_task both write to UART. Without a mutex,
 *      their messages would overlap and corrupt each other. The mutex
 *      ensures only one task writes at a time.
 *
 *   2. Binary semaphore for button synchronisation
 *      The USER button (PA0) triggers an EXTI0 interrupt. The ISR gives
 *      a binary semaphore. button_task blocks on that semaphore and wakes
 *      up exactly when the button is pressed. No polling, no wasted CPU.
 *
 * Tasks:
 *   sensor_task  (priority 3) : reads LSM303DLHC via I2C, sends to queue
 *   display_task (priority 2) : receives from queue, prints via UART (mutex)
 *   button_task  (priority 2) : waits on semaphore, prints via UART (mutex)
 *   led_task     (priority 1) : rotates compass LEDs independently
 *
 * Hardware:
 *   PA0  : USER button (EXTI0, rising edge)
 *   PB6  : I2C1 SCL
 *   PB7  : I2C1 SDA
 *   PA9  : USART1 TX (115200 baud)
 *   PE8..PE15 : compass LEDs
 *
 * CPU clock: 8 MHz (default HSI)
 *
 * -------------------------------------------------------------------------
 * NEW FREERTOS CONCEPTS
 * -------------------------------------------------------------------------
 *
 * Mutex (xSemaphoreCreateMutex):
 *   A mutual exclusion lock. Only one task can hold it at a time. Any other
 *   task that tries to take it blocks until the holder releases it.
 *   Key property: the task that takes the mutex MUST be the one to give it
 *   back. This ownership model enables priority inheritance.
 *   Priority inheritance: if a high-priority task blocks waiting for a mutex
 *   held by a low-priority task, FreeRTOS temporarily raises the low-priority
 *   task to the same priority as the high-priority task. This prevents a
 *   medium-priority task from starving the low-priority task and causing the
 *   high-priority task to wait indefinitely (priority inversion).
 *   Use for: protecting shared resources (UART, SPI bus, I2C bus, global
 *   variables accessed by multiple tasks).
 *
 * Binary semaphore (xSemaphoreCreateBinary):
 *   A signalling mechanism with two states: available (1) or taken (0).
 *   Unlike a mutex, it has no owner. Anyone can give it, anyone can take it.
 *   There is NO priority inheritance on a binary semaphore.
 *   Use for: signalling events from an ISR to a task, or between tasks where
 *   ownership does not matter.
 *   Typical pattern:
 *     ISR       : xSemaphoreGiveFromISR()  -- signals the event
 *     Task      : xSemaphoreTake()         -- blocks until event occurs
 *
 * xSemaphoreGiveFromISR / xSemaphoreTakeFromISR:
 *   Special variants of Give/Take that are safe to call from an ISR context.
 *   They do not block (ISRs must never block).
 *   The pxHigherPriorityTaskWoken parameter tells FreeRTOS whether to request
 *   a context switch at the end of the ISR: if giving the semaphore woke a
 *   higher-priority task, we want to switch to it immediately rather than
 *   returning to the interrupted task.
 *   portYIELD_FROM_ISR() triggers the context switch if needed.
 *
 * -------------------------------------------------------------------------
 * REGISTER GLOSSARY (new registers for this exo)
 * -------------------------------------------------------------------------
 *
 * EXTI->IMR  - Interrupt Mask Register
 *              Each bit unmasks the corresponding EXTI line.
 *              EXTI line 0 = PA0 (USER button). Set bit 0 to enable.
 *
 * EXTI->RTSR - Rising Trigger Selection Register
 *              Set bit N to trigger EXTI line N on a rising edge (LOW to HIGH).
 *              The USER button pulls PA0 HIGH when pressed.
 *
 * EXTI->PR   - Pending Register
 *              A bit is set when the corresponding EXTI line fires.
 *              Must be cleared in the ISR by writing 1 to the pending bit
 *              (writing 1 clears it, writing 0 has no effect).
 *
 * SYSCFG->EXTICR - External Interrupt Configuration Register
 *              Maps GPIO ports to EXTI lines.
 *              EXTICR[0] bits [3:0] select which port drives EXTI0:
 *                0x0 = PA, 0x1 = PB, 0x2 = PC, 0x3 = PD, 0x4 = PE...
 *              We set it to 0x0 to connect PA0 to EXTI0.
 *
 * NVIC registers (accessed via CMSIS-style macros in stm32f303xc.h):
 *   NVIC_SetPriority(IRQn, priority) : set interrupt priority
 *   NVIC_EnableIRQ(IRQn)             : enable interrupt in the NVIC
 *   EXTI0_IRQn                       : IRQ number for EXTI line 0
 * -------------------------------------------------------------------------
 */

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f303xc.h"

// ---------------------------------------------------------------------------
// Shared FreeRTOS objects
// Declared globally so all tasks and the ISR can access them.
// ---------------------------------------------------------------------------

// Queue: carries AccelData from sensor_task to display_task
#define QUEUE_LENGTH    5

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} AccelData;

static QueueHandle_t     xAccelQueue    = NULL;

// Mutex: protects UART access (shared by display_task and button_task)
// xSemaphoreCreateMutex() includes priority inheritance.
static SemaphoreHandle_t xUartMutex     = NULL;

// Binary semaphore: signals button press from EXTI0 ISR to button_task
// xSemaphoreCreateBinary() does NOT include priority inheritance.
static SemaphoreHandle_t xButtonSem     = NULL;

// ---------------------------------------------------------------------------
// Task stack sizes (words, 1 word = 4 bytes on Cortex-M4)
// ---------------------------------------------------------------------------

#define SENSOR_TASK_STACK_SIZE  256
#define DISPLAY_TASK_STACK_SIZE 256
#define BUTTON_TASK_STACK_SIZE  128
#define LED_TASK_STACK_SIZE     128

// ---------------------------------------------------------------------------
// UART : USART1 on PA9, 115200 baud
// ---------------------------------------------------------------------------

static void uart_init(void)
{
    RCC->AHBENR  |= RCC_AHBENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // PA9 : AF7 (USART1 TX)
    GPIOA->MODER  &= ~(0x3UL << 18);
    GPIOA->MODER  |=  (0x2UL << 18);
    GPIOA->AFR[1] &= ~(0xFUL << 4);
    GPIOA->AFR[1] |=  (0x7UL << 4);

    // BRR = fPCLK / baudrate = 8000000 / 115200 = 69
    USART1->BRR = 69;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}

// uart_print_safe: takes the UART mutex, prints, releases the mutex.
// Any task that needs to write to UART must call this function.
// xSemaphoreTake with portMAX_DELAY blocks until the mutex is available.
static void uart_print_safe(const char *str)
{
    // Take the mutex before accessing UART.
    // If another task holds it, we block here until it is released.
    xSemaphoreTake(xUartMutex, portMAX_DELAY);

    // Critical section: we are the only task writing to UART
    const char *p = str;
    while (*p) {
        while (!(USART1->ISR & USART_ISR_TXE));
        USART1->TDR = (uint32_t)(*p++);
    }

    // Release the mutex so other tasks can write
    xSemaphoreGive(xUartMutex);
}

// ---------------------------------------------------------------------------
// LEDs : PE8..PE15 as push-pull outputs
// ---------------------------------------------------------------------------

static void leds_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_IOPEEN;
    GPIOE->MODER &= ~(0xFFFFUL << 16);
    GPIOE->MODER |=  (0x5555UL << 16);
    GPIOE->BSRR   = LED_ALL_PINS_OFF;
}

// ---------------------------------------------------------------------------
// Button : PA0 as input with EXTI0 on rising edge
// ---------------------------------------------------------------------------

static void button_init(void)
{
    // RCC : enable GPIOA and SYSCFG clocks
    // SYSCFG is needed to configure the EXTI line routing (EXTICR registers)
    RCC->AHBENR  |= RCC_AHBENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // PA0 : input mode (MODER bits [1:0] = 00, which is the reset value)
    GPIOA->MODER &= ~(0x3UL << 0);

    // SYSCFG->EXTICR[0] bits [3:0] : select port A for EXTI line 0
    // 0x0 = port A. This connects PA0 to EXTI0.
    SYSCFG->EXTICR[0] &= ~(0xFUL << 0);
    SYSCFG->EXTICR[0] |=  (0x0UL << 0);

    // EXTI->RTSR : enable rising edge trigger on EXTI line 0
    // The USER button pulls PA0 HIGH when pressed (rising edge)
    EXTI->RTSR |= (1UL << 0);

    // EXTI->IMR : unmask EXTI line 0 (enable the interrupt)
    EXTI->IMR |= (1UL << 0);

    // NVIC : set priority and enable EXTI0 interrupt
    // Priority must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)
    // because the ISR calls xSemaphoreGiveFromISR() which is a FreeRTOS API.
    // We use priority 6 (numerically higher = lower hardware priority than 5).
    NVIC_SetPriority(EXTI0_IRQn, 6);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

// ---------------------------------------------------------------------------
// EXTI0 ISR: fires when the USER button (PA0) is pressed
//
// This runs in interrupt context, not in a task context.
// Rules for ISRs that call FreeRTOS API:
//   - Use the FromISR variants: xSemaphoreGiveFromISR, xQueueSendFromISR...
//   - Never block (no vTaskDelay, no xSemaphoreTake with non-zero timeout)
//   - Call portYIELD_FROM_ISR() at the end if a context switch is needed
// ---------------------------------------------------------------------------

void EXTI0_IRQHandler(void)
{
    // EXTI->PR : clear the pending bit for line 0 by writing 1
    // If not cleared, the ISR fires again immediately after returning
    EXTI->PR = (1UL << 0);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Give the semaphore from ISR context.
    // If button_task was blocked waiting on this semaphore, FreeRTOS marks
    // it as ready and sets xHigherPriorityTaskWoken = pdTRUE.
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);

    // If a higher-priority task was woken, request an immediate context switch.
    // This ensures button_task runs as soon as the ISR returns, rather than
    // waiting for the next tick interrupt.
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ---------------------------------------------------------------------------
// I2C1: PB6 (SCL), PB7 (SDA), 100 kHz
// ---------------------------------------------------------------------------

static void i2c1_init(void)
{
    RCC->AHBENR  |= RCC_AHBENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB->MODER   &= ~((0x3UL << 12) | (0x3UL << 14));
    GPIOB->MODER   |=  (0x2UL << 12) | (0x2UL << 14);
    GPIOB->OTYPER  |=  (1UL << 6) | (1UL << 7);
    GPIOB->OSPEEDR |=  (0x3UL << 12) | (0x3UL << 14);
    GPIOB->PUPDR   &= ~((0x3UL << 12) | (0x3UL << 14));
    GPIOB->PUPDR   |=  (0x1UL << 12) | (0x1UL << 14);
    GPIOB->AFR[0]  &= ~((0xFUL << 24) | (0xFUL << 28));
    GPIOB->AFR[0]  |=  (0x4UL << 24) | (0x4UL << 28);

    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->TIMINGR = 0x10420F13U;
    I2C1->CR1 |= I2C_CR1_PE;
}

static void i2c_wait_busy(void)  { while (I2C1->ISR & I2C_ISR_BUSY); }
static void i2c_wait_tc(void)    { while (!(I2C1->ISR & I2C_ISR_TC)); }
static void i2c_write_byte(uint8_t b) { while (!(I2C1->ISR & I2C_ISR_TXIS)); I2C1->TXDR = b; }
static uint8_t i2c_read_byte(void)   { while (!(I2C1->ISR & I2C_ISR_RXNE)); return (uint8_t)I2C1->RXDR; }

static void i2c_start_write(uint8_t addr, uint8_t n)
{
    I2C1->CR2 = ((uint32_t)(addr << 1)) | ((uint32_t)n << I2C_CR2_NBYTES_Pos) | I2C_CR2_START;
}

static void i2c_start_read(uint8_t addr, uint8_t n)
{
    I2C1->CR2 = ((uint32_t)(addr << 1)) | I2C_CR2_RD_WRN
              | ((uint32_t)n << I2C_CR2_NBYTES_Pos) | I2C_CR2_START | I2C_CR2_AUTOEND;
}

static void i2c_stop(void) { I2C1->CR2 |= I2C_CR2_STOP; while (I2C1->ISR & I2C_ISR_BUSY); }

// ---------------------------------------------------------------------------
// LSM303DLHC
// ---------------------------------------------------------------------------

#define LSM303_ADDR         0x19U
#define LSM303_CTRL_REG1_A  0x20U
#define LSM303_CTRL_REG4_A  0x23U
#define LSM303_OUT_X_L_A    0x28U

static void lsm303_write_reg(uint8_t reg, uint8_t val)
{
    i2c_wait_busy();
    i2c_start_write(LSM303_ADDR, 2);
    i2c_write_byte(reg);
    i2c_write_byte(val);
    i2c_wait_tc();
    i2c_stop();
}

static void lsm303_read_xyz(AccelData *data)
{
    uint8_t buf[6];
    i2c_wait_busy();
    i2c_start_write(LSM303_ADDR, 1);
    i2c_write_byte(LSM303_OUT_X_L_A | 0x80U);
    i2c_wait_tc();
    i2c_start_read(LSM303_ADDR, 6);
    for (uint8_t i = 0; i < 6; i++) { buf[i] = i2c_read_byte(); }
    data->x = (int16_t)((uint16_t)(buf[1] << 8) | buf[0]) >> 4;
    data->y = (int16_t)((uint16_t)(buf[3] << 8) | buf[2]) >> 4;
    data->z = (int16_t)((uint16_t)(buf[5] << 8) | buf[4]) >> 4;
}

static void lsm303_init(void)
{
    lsm303_write_reg(LSM303_CTRL_REG1_A, 0x57U);
    lsm303_write_reg(LSM303_CTRL_REG4_A, 0x08U);
}

// ---------------------------------------------------------------------------
// Task 1: sensor_task (priority 3)
// Reads LSM303 every 100ms and sends data to the queue.
// Unchanged from exo07.
// ---------------------------------------------------------------------------

static void sensor_task(void *pvParameters)
{
    (void)pvParameters;
    AccelData data;

    while (1) {
        lsm303_read_xyz(&data);
        xQueueSend(xAccelQueue, &data, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ---------------------------------------------------------------------------
// Task 2: display_task (priority 2)
// Receives AccelData from the queue and prints via UART.
// Uses uart_print_safe() which takes the UART mutex before writing.
// ---------------------------------------------------------------------------

static void display_task(void *pvParameters)
{
    (void)pvParameters;
    AccelData data;
    char buf[48];

    uart_print_safe("FreeRTOS semaphores and mutexes\r\n");
    uart_print_safe("---\r\n");
    uart_print_safe("     X        Y        Z\r\n");

    while (1) {
        if (xQueueReceive(xAccelQueue, &data, portMAX_DELAY) == pdTRUE) {
            int n = 0;
            int16_t vals[3] = {data.x, data.y, data.z};
            for (int axis = 0; axis < 3; axis++) {
                int16_t v = vals[axis];
                buf[n++] = (v < 0) ? '-' : ' ';
                if (v < 0) v = -v;
                uint8_t started = 0;
                for (int16_t div = 1000; div >= 1; div /= 10) {
                    int16_t digit = v / div;
                    v %= div;
                    if (digit || started || div == 1) {
                        buf[n++] = '0' + (char)digit;
                        started = 1;
                    } else {
                        buf[n++] = ' ';
                    }
                }
                buf[n++] = ' '; buf[n++] = ' '; buf[n++] = ' ';
            }
            buf[n++] = '\r'; buf[n++] = '\n'; buf[n] = '\0';
            uart_print_safe(buf);
        }
    }
}

// ---------------------------------------------------------------------------
// Task 3: button_task (priority 2)
//
// Blocks indefinitely on the binary semaphore xButtonSem.
// When EXTI0_IRQHandler gives the semaphore, this task wakes up and
// prints a message via UART using the shared mutex.
//
// This demonstrates the ISR-to-task synchronisation pattern:
//   ISR   : xSemaphoreGiveFromISR() -- fires on button press
//   Task  : xSemaphoreTake()        -- wakes up exactly when needed
// ---------------------------------------------------------------------------

static void button_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        // Block until the button ISR gives the semaphore.
        // portMAX_DELAY = wait forever. No CPU is consumed while waiting.
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) {
            uart_print_safe("Button pressed!\r\n");
        }
    }
}

// ---------------------------------------------------------------------------
// Task 4 : led_task (priority 1)
// Rotates one LED around PE8..PE15 every 250ms.
// Unchanged from exo07.
// ---------------------------------------------------------------------------

static void led_task(void *pvParameters)
{
    (void)pvParameters;
    uint8_t led = 8;

    while (1) {
        GPIOE->BSRR = LED_ALL_PINS_OFF;
        GPIOE->BSRR = (1UL << led);
        led++;
        if (led > 15) led = 8;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void)
{
    uart_init();
    leds_init();
    button_init();
    i2c1_init();
    lsm303_init();

    // Create queue
    xAccelQueue = xQueueCreate(QUEUE_LENGTH, sizeof(AccelData));
    if (xAccelQueue == NULL) {
        while (1);
    }

    // Create UART mutex
    // xSemaphoreCreateMutex() includes priority inheritance.
    // The mutex starts in the "given" state (available).
    xUartMutex = xSemaphoreCreateMutex();
    if (xUartMutex == NULL) {
        while (1);
    }

    // Create button binary semaphore
    // xSemaphoreCreateBinary() starts in the "taken" state (unavailable).
    // button_task will block immediately until the ISR gives it.
    xButtonSem = xSemaphoreCreateBinary();
    if (xButtonSem == NULL) {
        while (1);
    }

    // Create tasks
    xTaskCreate(sensor_task,  "sensor",  SENSOR_TASK_STACK_SIZE,  NULL, 3, NULL);
    xTaskCreate(display_task, "display", DISPLAY_TASK_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(button_task,  "button",  BUTTON_TASK_STACK_SIZE,  NULL, 2, NULL);
    xTaskCreate(led_task,     "leds",    LED_TASK_STACK_SIZE,     NULL, 1, NULL);

    // Start the scheduler
    vTaskStartScheduler();

    while (1);
    return 0;
}