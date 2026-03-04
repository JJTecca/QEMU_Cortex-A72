/******************************************************************************
 * File: timer_tests.c
 * Description: Timer and UART0 IRQ handler registration and init
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "interrupt/timer_tests.h"
#include "interrupts/irq.h"
#include "uart/uart0.h"

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/
volatile unsigned char u_timerLeft = '9';

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
 * Function: uart0_irq_handler
 * Description: IRQ handler for PL011 UART0 (INTID 33).
 *              Must read the UART DR register to clear the interrupt.
 ******************************************************************************/
static void uart0_irq_handler(uint32_t irq_id)
{
    (void)irq_id; /* read UART DR to clear */
}

/******************************************************************************
 * Function: timer_irq_handler
 * Description: IRQ handler for ARM Generic Timer (INTID 30).
 *              Prints confirmation and re-arms the countdown for next second.
 ******************************************************************************/
static void timer_irq_handler(uint32_t irq_id)
{
    (void)irq_id;
    uart_puts("[IRQ] Timer fired! Remaining...");
    uart_putc((unsigned char)(u_timerLeft--));
    uart_puts("\n");

    if(u_timerLeft == 0) {
        __asm__ volatile("msr cntp_ctl_el0,  %0" :: "r"(1UL));
    }

    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
}

/******************************************************************************
 * Function: interrupt_tests_init
 * Description: Initializes GIC, registers UART0 and Timer handlers,
 *              arms the physical timer for a 1-second periodic tick,
 *              then unmasks IRQs at EL1.
 ******************************************************************************/
void interrupt_tests_init(void)
{
    irq_init();
    irq_register_handler(IRQ_ID_UART0, uart0_irq_handler);
    irq_register_handler(IRQ_ID_TIMER, timer_irq_handler);

    /* Read board counter frequency (QEMU virt = 62.5 MHz) */
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    /* Load countdown: freq ticks = 1 second */
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
    /* Enable timer, unmask (bit0 = enable, bit1 = imask) */
    __asm__ volatile("msr cntp_ctl_el0,  %0" :: "r"(1UL));

    irq_enable();

    while (u_timerLeft != 0) {
        __asm__ volatile("wfe");   /* sleep, IRQ wakes us, handler runs, wfe again */
    }

    irq_disable();
    uart_puts("[IRQ] Timer test complete. Continuing...\n");
}
