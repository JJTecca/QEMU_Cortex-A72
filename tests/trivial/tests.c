/******************************************************************************
 * File: tests.c
 * Project: RPi5-Industrial-Gateway Bare Metal Development
 *
 * Target Hardware: Raspberry Pi 5 (BCM2712, Cortex-A76)
 * UART Controller: ARM PrimeCell UART (PL011) r1p5
 * Base Address:
 *   - QEMU virt: 0x09000000
 *   - RPi5 RP1: 0x40030000
 *
 * Copyright (c) 2026
 * Author: Maior Cristian
 * License: MIT / Proprietary (Bachelor Thesis Project)
 *****************************************************************************/

/******************************************************************************
 * ⚠️  CRITICAL: Global variables with initializers will HANG the system!
 *     boot.S does not initialize .data/.bss sections.
 *     Use fixed memory addresses instead (e.g., LOCK_ADDR).
 *****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "uart/uart0.h"
#include "ipc/ipc.h"
#include "ringbuffer/ringbuf.h"

/***************************************************
 * Macro Definitions
 **************************************************/
#define TEST_PING_DATA_BASE     0x1000
#define TEST_DATA_BASE          0xDEAD0000
#define TEST_DATA_STEP          0x100
#define TEST_ACK_POLL_ROUNDS    10
#define TEST_DELAY_SHORT        2000000
#define TEST_DELAY_MEDIUM       3000000
#define TEST_DELAY_LONG         10000000

/******************************************************************************
 * External declarations
 *****************************************************************************/
extern void delay(int n);

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
 * Function: test_print_pass
 * Description: Prints a PASS banner for a given test label
 * Parameters: label - Short string identifying the test
 * Returns: None
 *****************************************************************************/
static void test_print_pass(const char *label) {
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("[PASS] ");
    uart_puts(label);
    uart_puts("\n");
    spinlock_release(SPINLOCK_ADDR);
}

/******************************************************************************
 * Function: test_print_fail
 * Description: Prints a FAIL banner for a given test label with a reason
 * Parameters:
 *   label  - Short string identifying the test
 *   reason - Description of why it failed
 * Returns: None
 *****************************************************************************/
static void test_print_fail(const char *label, const char *reason) {
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("[FAIL] ");
    uart_puts(label);
    uart_puts(" -> ");
    uart_puts(reason);
    uart_puts("\n");
    spinlock_release(SPINLOCK_ADDR);
}

/******************************************************************************
 * UNIT TEST CASES
 *****************************************************************************/

/******************************************************************************
 * Function: test1_ping_all_cores
 * Description: [Test 1] Sends a PING message from Core 0 to all secondary
 *              cores (1-3) using the mailbox IPC mechanism. Verifies that
 *              mailbox_send() returns success (0) for each destination core.
 * Parameters: None
 * Returns:
 *   0  - All PINGs sent successfully
 *  -1  - One or more mailbox_send() calls failed
 *****************************************************************************/
int test1_ping_all_cores(void) {
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n[Core 0] Test 1: Sending PING to all cores\n");
    spinlock_release(SPINLOCK_ADDR);

    int result = 0;

    for (int dest = 1; dest <= 3; dest++) {
        unsigned int test_data = TEST_PING_DATA_BASE + dest;

        if (mailbox_send(dest, MSG_PING, test_data) == 0) {
            spinlock_acquire(SPINLOCK_ADDR);
            uart_puts("[Core 0] -> Core ");
            uart_putc('0' + dest);
            uart_puts(" PING sent\n");
            spinlock_release(SPINLOCK_ADDR);
        } else {
            test_print_fail("Test1 PING", "mailbox_send returned -1");
            result = -1;
        }

        delay(TEST_DELAY_SHORT);
    }

    // Wait for secondary cores to process PINGs
    delay(TEST_DELAY_LONG);

    if (result == 0) {
        test_print_pass("Test1: PING to all cores");
    }

    return result;
}

/******************************************************************************
 * Function: test2_send_data_messages
 * Description: [Core 0] Test 2: Sending DATA messages to all secondary cores
 *              (1-3) with distinct payloads (0xDEADxx00 per core). Then polls
 *              Core 0's own mailbox for ACK responses from each core and
 *              prints the ACK payload.
 * Parameters: None
 * Returns:
 *   0  - All DATA messages sent and at least one ACK received
 *  -1  - No ACK received after poll rounds
 *****************************************************************************/
int test2_send_data_messages(void) {
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n[Core 0] Test 2: Sending DATA messages\n");
    spinlock_release(SPINLOCK_ADDR);

    // Send DATA to each secondary core
    for (int dest = 1; dest <= 3; dest++) {
        unsigned int test_data = TEST_DATA_BASE + (dest * TEST_DATA_STEP);
        mailbox_send(dest, MSG_DATA, test_data);

        spinlock_acquire(SPINLOCK_ADDR);
        uart_puts("[Core 0] -> Core ");
        uart_putc('0' + dest);
        uart_puts(" DATA: ");
        uart_puthex(test_data);
        uart_puts("\n");
        spinlock_release(SPINLOCK_ADDR);

        delay(TEST_DELAY_SHORT);
    }

    // Wait for secondary cores to reply with ACKs
    delay(TEST_DELAY_LONG);

    // Poll Core 0's mailbox for ACK responses
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n[Core 0] Checking for ACK responses...\n");
    spinlock_release(SPINLOCK_ADDR);

    unsigned int sender, msg_type, msg_data;
    int acks_received = 0;

    for (int i = 0; i < TEST_ACK_POLL_ROUNDS; i++) {
        if (mailbox_receive(0, &sender, &msg_type, &msg_data) == 1) {
            spinlock_acquire(SPINLOCK_ADDR);
            uart_puts("[Core 0] <- ACK from Core ");
            uart_putc('0' + sender);
            uart_puts(" | Data: ");
            uart_puthex(msg_data);
            uart_puts("\n");
            spinlock_release(SPINLOCK_ADDR);

            mailbox_clear(0);
            acks_received++;
        }
        delay(TEST_DELAY_MEDIUM);
    }

    if (acks_received > 0) {
        test_print_pass("Test2: DATA messages + ACK responses");
        return 0;
    }

    test_print_fail("Test2 DATA", "No ACK received from secondary cores");
    return -1;
}

/******************************************************************************
 * Function: test3_uart_rx_keyboard_simulation
 * Description: [Test 3] UART RX live - Keyboard Simulation.
 * Parameters: None
 * Returns: None (Core 0 announces then enters WFE sleep)
 *****************************************************************************/
void test3_uart_rx_keyboard_simulation(void) {
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n[Test 3] UART RX live - Keyboard Simulation:\n");
    spinlock_release(SPINLOCK_ADDR);

    // Core 0 enters low-power idle — test runs on Cores 1 & 2
    while (1) { __asm__ volatile("wfe"); }
}

/******************************************************************************
 * Function: run_all_tests
 * Description: Master test runner — executes all unit test cases in sequence.
 *              Called from main() on Core 0 after all secondary cores are
 *              booted and mailboxes are initialized.
 * Parameters: None
 * Returns: None
 *****************************************************************************/
void run_all_tests(void) {
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n=== [TEST SUITE] Starting All Tests ===\n\n");
    spinlock_release(SPINLOCK_ADDR);

    test1_ping_all_cores();
    test2_send_data_messages();
    test3_uart_rx_keyboard_simulation();  // Does not return (WFE loop)
}
