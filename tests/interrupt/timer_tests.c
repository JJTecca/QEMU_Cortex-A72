/******************************************************************************
 * File: timer_tests.c
 * Description: Timer safety tests — frequency sanity, IMASK masking,
 *              countdown with IRQ, and overflow/late-reload delta check.
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "interrupt/timer_tests.h"
#include "interrupts/irq.h"
#include "uart/uart0.h"

/**************************************************
 * MACRO DEFINITIONS
 ***************************************************/
#define QEMU_VIRT_FREQ     62500000UL   /* cntfrq_el0 on QEMU virt machine   */
#define COUNTDOWN_START    '5'          /* ASCII countdown 5 → 0             */
#define DELTA_TEST_TICKS   3u           /* number of IRQs to measure spacing */
#define TOLERANCE_PERCENT  10u          /* allow 10% jitter on delta check   */

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/
volatile unsigned char u_timerLeft;
static volatile uint32_t g_imask_fired;
static volatile uint32_t g_delta_count;
static volatile uint64_t g_delta_timestamps[DELTA_TEST_TICKS + 1];

/**************************************************
 * HELPER — read physical counter
 ***************************************************/
static inline uint64_t read_cntpct(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
    return v;
}

static inline uint64_t read_cntfrq(void)
{
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}

/**************************************************
 * IRQ HANDLERS
 ***************************************************/

static void uart0_irq_handler(uint32_t irq_id) { (void)irq_id; }

static void countdown_irq_handler(uint32_t irq_id)
{
    (void)irq_id;
    uart_puts("[IRQ] Timer fired! Remaining...");
    uart_putc((unsigned char)u_timerLeft);
    uart_puts("\n");

    if (u_timerLeft > '0') {
        u_timerLeft--;
    } else {
        /* Disable timer completely */
        __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(0UL));
        u_timerLeft = 0;   /* signal while-loop to exit */
        return;
    }

    /* Re-arm for next 1-second tick */
    uint64_t freq = read_cntfrq();
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
}

/* IMASK test handler — should NEVER be called */
static void imask_irq_handler(uint32_t irq_id)
{
    (void)irq_id;
    g_imask_fired++;
}

/* Delta test handler — records timestamp of each fire */
static void delta_irq_handler(uint32_t irq_id)
{
    (void)irq_id;

    if (g_delta_count <= DELTA_TEST_TICKS) {
        g_delta_timestamps[g_delta_count] = read_cntpct();
        g_delta_count++;
    }

    if (g_delta_count > DELTA_TEST_TICKS) {
        /* Collected enough samples — disable timer */
        __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(0UL));
        return;
    }

    /* Re-arm for next tick */
    uint64_t freq = read_cntfrq();
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
}

/******************************************************************************
 * Test 1: Frequency Sanity Check
 * Reads cntfrq_el0 and asserts it matches the QEMU virt expected value.
 * If wrong, all timer math is invalid — halt immediately.
 ******************************************************************************/
static int test_freq_sanity(void)
{
    uart_puts("[TEST] Frequency sanity check... ");
    uint64_t freq = read_cntfrq();

    if (freq != QEMU_VIRT_FREQ) {
        uart_puts("FAIL (got 0x");
        uart_puthex(freq);
        uart_puts(", expected 0x");
        uart_puthex(QEMU_VIRT_FREQ);
        uart_puts(")\n");
        return -1;
    }

    uart_puts("PASS (62.5 MHz)\n");
    return 0;
}

/******************************************************************************
 * Test 2: IMASK Bit Test
 * Arms the timer with ENABLE=1, IMASK=1 (cntp_ctl_el0 = 0b11).
 * The timer counts internally but the IRQ line stays deasserted.
 * Waits 2x freq ticks, asserts handler was never called.
 ******************************************************************************/
static int test_imask(void)
{
    uart_puts("[TEST] IMASK bit test... ");

    g_imask_fired = 0;
    irq_register_handler(IRQ_ID_TIMER, imask_irq_handler);

    uint64_t freq = read_cntfrq();

    /* cntp_ctl_el0 has 2 imp bits 
    * 00 -> ENABLE 1
    * 01 -> IMASK  count internally, but don't signal the interrupt controller
    * 11 -> The timer is running and will assert its condition flag */
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
    __asm__ volatile("msr cntp_ctl_el0,  %0" :: "r"(3UL));

    irq_enable();

    /* Busy-wait for ~2 seconds worth of ticks */
    uint64_t start = read_cntpct();
    while ((read_cntpct() - start) < (2 * freq)) {
        __asm__ volatile("nop");
    }

    irq_disable();

    /* Disable timer */
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(0UL));

    if (g_imask_fired != 0) {
        uart_puts("FAIL (handler fired ");
        uart_putc('0' + (g_imask_fired & 0xF));
        uart_puts(" times despite IMASK)\n");
        return -1;
    }

    uart_puts("PASS (0 fires with IMASK set)\n");
    return 0;
}

/******************************************************************************
 * Test 3: Countdown Test (original, with bug fix)
 * Counts from COUNTDOWN_START down to '0', printing each second.
 ******************************************************************************/
static int test_countdown(void)
{
    uart_puts("[TEST] Countdown timer ");
    uart_putc(COUNTDOWN_START);
    uart_puts(" -> 0...\n");

    u_timerLeft = COUNTDOWN_START;
    irq_register_handler(IRQ_ID_TIMER, countdown_irq_handler);

    uint64_t freq = read_cntfrq();
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
    __asm__ volatile("msr cntp_ctl_el0,  %0" :: "r"(1UL));

    irq_enable();

    while (u_timerLeft != 0) {
        __asm__ volatile("wfe");
    }

    irq_disable();
    uart_puts("[TEST] Countdown... PASS\n");
    return 0;
}

/******************************************************************************
 * Test 4: Overflow / Late-Reload Delta Check
 * Fires DELTA_TEST_TICKS consecutive 1-second IRQs and records cntpct_el0
 * at each. Verifies every consecutive pair is spaced within +-TOLERANCE%.
 ******************************************************************************/
static int test_delta(void)
{
    uart_puts("[TEST] Overflow/late-reload delta check... ");

    g_delta_count = 0;
    for (uint32_t i = 0; i <= DELTA_TEST_TICKS; i++)
        g_delta_timestamps[i] = 0;

    irq_register_handler(IRQ_ID_TIMER, delta_irq_handler);

    uint64_t freq = read_cntfrq();
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(freq));
    __asm__ volatile("msr cntp_ctl_el0,  %0" :: "r"(1UL));

    irq_enable();

    while (g_delta_count <= DELTA_TEST_TICKS) {
        __asm__ volatile("wfe");
    }

    irq_disable();

    /* Validate deltas between consecutive timestamps */
    uint64_t min_ok = freq - (freq * TOLERANCE_PERCENT / 100);
    uint64_t max_ok = freq + (freq * TOLERANCE_PERCENT / 100);
    int pass = 1;

    for (uint32_t i = 1; i <= DELTA_TEST_TICKS; i++) {
        uint64_t delta = g_delta_timestamps[i] - g_delta_timestamps[i - 1];

        uart_puts("  delta[");
        uart_putc('0' + i);
        uart_puts("]: 0x");
        uart_puthex(delta);

        if (delta < min_ok || delta > max_ok) {
            uart_puts(" FAIL\n");
            pass = 0;
        } else {
            uart_puts(" OK\n");
        }
    }

    if (!pass) {
        uart_puts("[TEST] Delta check... FAIL\n");
        return -1;
    }

    uart_puts("[TEST] Delta check... PASS\n");
    return 0;
}

/******************************************************************************
 * Function: interrupt_tests_init
 * Description: Runs all timer safety tests in order, halts on fatal failure.
 ******************************************************************************/
void interrupt_tests_init(void)
{
    irq_init();
    irq_register_handler(IRQ_ID_UART0, uart0_irq_handler);

    /* Test 1 — freq must be correct for ALL other tests */
    if (test_freq_sanity() != 0) {
        uart_puts("[FATAL] Cannot proceed - timer frequency mismatch!\n");
        while (1) { __asm__ volatile("wfe"); }
    }

    test_imask();
    test_countdown();
    test_delta();

    uart_puts("[IRQ] All timer tests complete. Continuing...\n");
}