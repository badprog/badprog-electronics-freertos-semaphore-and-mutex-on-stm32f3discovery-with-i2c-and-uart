// badprog.com
#ifndef STM32F303XC_H
#define STM32F303XC_H

#include <stdint.h>

typedef volatile uint32_t reg32_t;

// ---------------------------------------------------------------------------
// Bus base addresses
// ---------------------------------------------------------------------------
#define PERIPH_BASE         (0x40000000UL)
#define APB1PERIPH_BASE     (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE     (PERIPH_BASE + 0x00020000UL)
#define AHB2PERIPH_BASE     (PERIPH_BASE + 0x08000000UL)

// ---------------------------------------------------------------------------
// RCC - Reset and Clock Control
// ---------------------------------------------------------------------------
#define RCC_BASE            (AHB1PERIPH_BASE + 0x00001000UL)

typedef struct {
    reg32_t CR;
    reg32_t CFGR;
    reg32_t CIR;
    reg32_t APB2RSTR;
    reg32_t APB1RSTR;
    reg32_t AHBENR;
    reg32_t APB2ENR;
    reg32_t APB1ENR;
    reg32_t BDCR;
    reg32_t CSR;
    reg32_t AHBRSTR;
    reg32_t CFGR2;
    reg32_t CFGR3;
} RCC_TypeDef;

#define RCC                     ((RCC_TypeDef *) RCC_BASE)

// AHB peripheral clock enable bits
#define RCC_AHBENR_IOPAEN       (1UL << 17)   // GPIOA clock
#define RCC_AHBENR_IOPBEN       (1UL << 18)   // GPIOB clock (I2C1 pins PB6/PB7)
#define RCC_AHBENR_IOPEEN       (1UL << 21)   // GPIOE clock (LEDs PE8..PE15)

// APB2 peripheral clock enable bits
#define RCC_APB2ENR_USART1EN    (1UL << 14)   // USART1 clock (TX on PA9)
#define RCC_APB2ENR_SYSCFGEN    (1UL << 0)    // SYSCFG clock (required for EXTI routing)

// APB1 peripheral clock enable bits
#define RCC_APB1ENR_I2C1EN      (1UL << 21)   // I2C1 clock (PB6=SCL, PB7=SDA)

// ---------------------------------------------------------------------------
// GPIO
// ---------------------------------------------------------------------------
#define GPIOA_BASE          (AHB2PERIPH_BASE + 0x00000000UL)
#define GPIOB_BASE          (AHB2PERIPH_BASE + 0x00000400UL)
#define GPIOE_BASE          (AHB2PERIPH_BASE + 0x00001000UL)

typedef struct {
    reg32_t MODER;      // Mode register          : 2 bits per pin: 00=in 01=out 10=AF 11=analog
    reg32_t OTYPER;     // Output type register   : 1 bit per pin: 0=push-pull 1=open-drain
    reg32_t OSPEEDR;    // Output speed register  : 2 bits per pin: 00=low 01=med 10=high 11=vhigh
    reg32_t PUPDR;      // Pull-up/down register  : 2 bits per pin: 00=none 01=pull-up 10=pull-down
    reg32_t IDR;        // Input data register    : read pin state
    reg32_t ODR;        // Output data register   : set pin state
    reg32_t BSRR;       // Bit set/reset register : bits[15:0]=set, bits[31:16]=reset (atomic)
    reg32_t LCKR;       // Lock register
    reg32_t AFR[2];     // Alternate function registers : AFR[0]=pins 0..7, AFR[1]=pins 8..15
    reg32_t BRR;        // Bit reset register
} GPIO_TypeDef;

#define GPIOA               ((GPIO_TypeDef *) GPIOA_BASE)
#define GPIOB               ((GPIO_TypeDef *) GPIOB_BASE)
#define GPIOE               ((GPIO_TypeDef *) GPIOE_BASE)

// MODER field values (2 bits per pin)
#define GPIO_MODER_INPUT    (0x0UL)
#define GPIO_MODER_OUTPUT   (0x1UL)
#define GPIO_MODER_AF       (0x2UL)
#define GPIO_MODER_ANALOG   (0x3UL)

// OTYPER field values (1 bit per pin)
#define GPIO_OTYPER_PP      (0x0UL)   // Push-pull
#define GPIO_OTYPER_OD      (0x1UL)   // Open-drain (required for I2C)

// OSPEEDR field values (2 bits per pin)
#define GPIO_OSPEEDR_LOW    (0x0UL)
#define GPIO_OSPEEDR_MED    (0x1UL)
#define GPIO_OSPEEDR_HIGH   (0x2UL)
#define GPIO_OSPEEDR_VHIGH  (0x3UL)

// PUPDR field values (2 bits per pin)
#define GPIO_PUPDR_NONE     (0x0UL)
#define GPIO_PUPDR_UP       (0x1UL)   // Pull-up
#define GPIO_PUPDR_DOWN     (0x2UL)   // Pull-down

// LEDs on GPIOE PE8..PE15
// LED_ALL_PINS_OFF: BSRR reset half (bits[31:16]) for PE8..PE15
#define LED_ALL_PINS_OFF    (0xFFUL << 24)

// ---------------------------------------------------------------------------
// SYSCFG - System Configuration Controller
// Required to route GPIO pins to EXTI lines via EXTICR registers.
// ---------------------------------------------------------------------------
#define SYSCFG_BASE         (APB2PERIPH_BASE + 0x00000000UL)

typedef struct {
    reg32_t CFGR1;      // Configuration register 1
    reg32_t RCR;        // CCM RAM control register
    reg32_t EXTICR[4];  // External interrupt configuration registers
                        // EXTICR[0] : EXTI0..3
                        // EXTICR[1] : EXTI4..7
                        // EXTICR[2] : EXTI8..11
                        // EXTICR[3] : EXTI12..15
                        // 4 bits per EXTI line: 0=PA 1=PB 2=PC 3=PD 4=PE...
    reg32_t CFGR2;      // Configuration register 2
    reg32_t RESERVED[12];
    reg32_t CFGR3;      // Configuration register 3
} SYSCFG_TypeDef;

#define SYSCFG              ((SYSCFG_TypeDef *) SYSCFG_BASE)

// ---------------------------------------------------------------------------
// EXTI - External Interrupt/Event Controller
// Manages external interrupt lines connected to GPIO pins.
// ---------------------------------------------------------------------------
#define EXTI_BASE           (APB2PERIPH_BASE + 0x00000400UL)

typedef struct {
    reg32_t IMR;    // Interrupt mask register    : bit N=1 enables EXTI line N interrupt
    reg32_t EMR;    // Event mask register        : bit N=1 enables EXTI line N event
    reg32_t RTSR;   // Rising trigger selection   : bit N=1 triggers on rising edge
    reg32_t FTSR;   // Falling trigger selection  : bit N=1 triggers on falling edge
    reg32_t SWIER;  // Software interrupt event   : write 1 to trigger EXTI line N in software
    reg32_t PR;     // Pending register           : bit N=1 means EXTI line N fired
                    //                              write 1 to clear (writing 0 has no effect)
} EXTI_TypeDef;

#define EXTI                ((EXTI_TypeDef *) EXTI_BASE)

// ---------------------------------------------------------------------------
// NVIC - Nested Vectored Interrupt Controller
// Controls interrupt priorities and enable/disable for all peripherals.
//
// On Cortex-M4, interrupt priorities use LOWER numbers for HIGHER priority.
// This is the OPPOSITE of FreeRTOS task priorities.
//
// NVIC_SetPriority(IRQn, priority) : set the priority of an interrupt
// NVIC_EnableIRQ(IRQn)             : enable an interrupt
// NVIC_DisableIRQ(IRQn)            : disable an interrupt
//
// IRQ numbers for STM32F303xC peripherals used in this project:
// ---------------------------------------------------------------------------

// IRQ numbers (from STM32F303xC vector table, RM0316 Table 64)
typedef enum {
    EXTI0_IRQn  = 6,    // EXTI line 0 interrupt (PA0 = USER button)
    EXTI1_IRQn  = 7,    // EXTI line 1 interrupt
    EXTI2_IRQn  = 8,    // EXTI line 2 interrupt
    EXTI3_IRQn  = 9,    // EXTI line 3 interrupt
    EXTI4_IRQn  = 10,   // EXTI line 4 interrupt
} IRQn_Type;

// NVIC base address
#define NVIC_BASE           (0xE000E100UL)

typedef struct {
    reg32_t ISER[8];    // Interrupt Set Enable Registers    : write 1 to enable IRQ N
    reg32_t RESERVED0[24];
    reg32_t ICER[8];    // Interrupt Clear Enable Registers  : write 1 to disable IRQ N
    reg32_t RESERVED1[24];
    reg32_t ISPR[8];    // Interrupt Set Pending Registers
    reg32_t RESERVED2[24];
    reg32_t ICPR[8];    // Interrupt Clear Pending Registers
    reg32_t RESERVED3[24];
    reg32_t IABR[8];    // Interrupt Active Bit Registers
    reg32_t RESERVED4[56];
    reg32_t IP[240];    // Interrupt Priority Registers      : 1 byte per IRQ
} NVIC_TypeDef;

#define NVIC                ((NVIC_TypeDef *) NVIC_BASE)

// NVIC helper functions
// priority : 0..15 on STM32F3 (4-bit priority field in the top 4 bits of IP register)
static inline void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority)
{
    // IP registers are byte-wide. Priority is stored in the top 4 bits.
    // The bottom 4 bits are not implemented and read as zero.
    NVIC->IP[(uint32_t)IRQn] = (uint32_t)(priority << 4);
}

static inline void NVIC_EnableIRQ(IRQn_Type IRQn)
{
    // ISER: each register covers 32 IRQs. Write 1 to bit (IRQn % 32).
    NVIC->ISER[(uint32_t)IRQn >> 5] = (1UL << ((uint32_t)IRQn & 0x1FUL));
}

static inline void NVIC_DisableIRQ(IRQn_Type IRQn)
{
    NVIC->ICER[(uint32_t)IRQn >> 5] = (1UL << ((uint32_t)IRQn & 0x1FUL));
}

// ---------------------------------------------------------------------------
// USART1 - Universal Synchronous/Asynchronous Receiver Transmitter
// PA9 = TX (AF7)
// ---------------------------------------------------------------------------
#define USART1_BASE         (APB2PERIPH_BASE + 0x00003800UL)

typedef struct {
    reg32_t CR1;    // Control register 1 : UE (enable), TE (tx enable), RE (rx enable)
    reg32_t CR2;    // Control register 2 : stop bits
    reg32_t CR3;    // Control register 3 : DMA, flow control
    reg32_t BRR;    // Baud rate register : BRR = fPCLK / baudrate
    reg32_t GTPR;   // Guard time and prescaler
    reg32_t RTOR;   // Receiver timeout
    reg32_t RQR;    // Request register
    reg32_t ISR;    // Interrupt and status register
    reg32_t ICR;    // Interrupt flag clear register
    reg32_t RDR;    // Receive data register
    reg32_t TDR;    // Transmit data register
} USART_TypeDef;

#define USART1              ((USART_TypeDef *) USART1_BASE)

// CR1 bits
#define USART_CR1_UE        (1UL << 0)    // USART enable
#define USART_CR1_TE        (1UL << 3)    // Transmitter enable
#define USART_CR1_RE        (1UL << 2)    // Receiver enable

// ISR bits
#define USART_ISR_TXE       (1UL << 7)    // Transmit data register empty

// ---------------------------------------------------------------------------
// I2C1 - Inter-Integrated Circuit
// PB6 = SCL (AF4), PB7 = SDA (AF4)
// ---------------------------------------------------------------------------
#define I2C1_BASE           (APB1PERIPH_BASE + 0x00005400UL)

typedef struct {
    reg32_t CR1;        // Control register 1    : PE (enable), filters, interrupts
    reg32_t CR2;        // Control register 2    : SADD, NBYTES, START, STOP, RD_WRN
    reg32_t OAR1;       // Own address register 1
    reg32_t OAR2;       // Own address register 2
    reg32_t TIMINGR;    // Timing register       : PRESC, SCLDEL, SDADEL, SCLH, SCLL
    reg32_t TIMEOUTR;   // Timeout register
    reg32_t ISR;        // Interrupt and status  : TXE, TXIS, RXNE, TC, BUSY...
    reg32_t ICR;        // Interrupt clear register
    reg32_t PECR;       // PEC register
    reg32_t RXDR;       // Receive data register
    reg32_t TXDR;       // Transmit data register
} I2C_TypeDef;

#define I2C1                ((I2C_TypeDef *) I2C1_BASE)

// CR1 bits
#define I2C_CR1_PE          (1UL << 0)

// CR2 bits
#define I2C_CR2_RD_WRN      (1UL << 10)
#define I2C_CR2_START       (1UL << 13)
#define I2C_CR2_STOP        (1UL << 14)
#define I2C_CR2_AUTOEND     (1UL << 25)
#define I2C_CR2_NBYTES_Pos  16

// ISR bits
#define I2C_ISR_TXE         (1UL << 0)
#define I2C_ISR_TXIS        (1UL << 1)
#define I2C_ISR_RXNE        (1UL << 2)
#define I2C_ISR_TC          (1UL << 6)
#define I2C_ISR_BUSY        (1UL << 15)

// ---------------------------------------------------------------------------
// LSM303DLHC - Accelerometer
// ---------------------------------------------------------------------------
#define LSM303_ACCEL_ADDR       0x19U
#define LSM303_CTRL_REG1_A      0x20U
#define LSM303_CTRL_REG1_A_VAL  0x57U
#define LSM303_CTRL_REG4_A      0x23U
#define LSM303_CTRL_REG4_A_VAL  0x08U
#define LSM303_OUT_X_L_A        0x28U

// ---------------------------------------------------------------------------
// Cortex-M4 intrinsics
// ---------------------------------------------------------------------------
static inline void __NOP(void) { __asm volatile ("nop"); }

#endif // STM32F303XC_H