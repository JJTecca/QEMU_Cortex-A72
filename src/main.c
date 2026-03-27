/******************************************************************************
 * File: main.c
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

/******************************************************************************
 * Include headers
 *****************************************************************************/
#include "uart/uart0.h"
#include "ipc/ipc.h"
#include "ringbuffer/ringbuf.h"
#include "interrupts/irq.h"
#include "trivial/tests.h"
#include "interrupt/timer_tests.h"
#include "scheduler/scheduler.h"
#include "dispatcher.h"
#include "crypto/hmac_sha256.h"

/******************************************************************************
 * Macro Definition
 *****************************************************************************/
#define PSCI_CPU_ON 0xC4000003

/*****************************************************************************
 * Global Variables
 *****************************************************************************/
static const uint8_t secret_key[HMAC_KEY_SIZE] = {
        0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
        0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
        0x76, 0x2E, 0x45, 0x18, 0x5A, 0x31, 0xC7, 0xD9,
        0x11, 0xF2, 0x83, 0x6A, 0xBC, 0x44, 0x07, 0x9E
};
 
/******************************************************************************
 * Function: delay
 * Description: Simple busy-wait delay loop
 * Parameters: n - Number of loop iterations
 * Returns: None
 *****************************************************************************/
void delay(int n) {
    for (volatile int i = 0; i < n; i++);
}

/******************************************************************************
 * Function: read_sp
 * Description: Reads the current stack pointer value
 * Parameters: None
 * Returns: Current stack pointer address
 *****************************************************************************/
unsigned long read_sp(void) {
    unsigned long sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

/******************************************************************************
 * Function: read_current_el
 * Description: Reads the current exception level from CurrentEL register
 * Parameters: None
 * Returns: Exception level (1, 2, or 3)
 *****************************************************************************/
unsigned long read_current_el(void) {
    unsigned long el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    return (el >> 2) & 0x3;
}

/******************************************************************************
 * Function: read_sctlr
 * Description: Reads the System Control Register (SCTLR_EL1)
 * Parameters: None
 * Returns: SCTLR_EL1 register value
 *****************************************************************************/
unsigned long read_sctlr(void) {
    unsigned long sctlr;
    __asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr));
    return sctlr;
}

/******************************************************************************
 * Function: psci_cpu_on
 * Description: Invokes PSCI CPU_ON function to start a secondary CPU core
 * Parameters: 
 *   cpu - Target CPU ID (1, 2, or 3)
 *   entry - Entry point address for the secondary CPU
 * Returns: PSCI return code (0 = success, negative = error)
 * Note: Uses HVC instruction to call into EL2 firmware
 *****************************************************************************/
long psci_cpu_on(unsigned long cpu, unsigned long entry) {
    register unsigned long x0 __asm__("x0") = PSCI_CPU_ON;
    register unsigned long x1 __asm__("x1") = cpu;
    register unsigned long x2 __asm__("x2") = entry;
    register unsigned long x3 __asm__("x3") = 0;
    __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

/******************************************************************************
 * Function: secondary_main
 * Description: Entry point for secondary CPU cores (Cores 1-3)
 * Parameters: None
 * Returns: None (infinite loop)
 * Note: Displays core ID and system state, then enters WFE idle loop
 *****************************************************************************/
extern void _start(void);

void secondary_main(void) {
    unsigned long cpu = get_cpu_id();
    delay(cpu * 8000000);

    // All cores announce themselves (keep this)
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("[Core "); uart_putc('0' + cpu);
    uart_puts("] Online! SP: "); uart_puthex(read_sp());
    uart_puts(" EL: "); uart_putc('0' + read_current_el());
    uart_puts("\n");
    spinlock_release(SPINLOCK_ADDR);

    // Testing with circular ring buffer 0 delay
    if (cpu == 1) {
        // Core 1: Producer — push test bytes 'A' to 'J' into ring buffer
        spinlock_acquire(SPINLOCK_ADDR);
        uart_puts("[Core 1] Ring buffer test: pushing A-J...\n");
        spinlock_release(SPINLOCK_ADDR);

        for (unsigned char c = 'A'; c <= 'J'; c++) {
            ring_buffer_put(UART_RX_BUFFER, c);
        }

        while (1) { __asm__ volatile("wfe"); }
    }

    if (cpu == 2) {
        // Core 2: Consumer — read from ring buffer and print
        spinlock_acquire(SPINLOCK_ADDR);
        uart_puts("[Core 2] Ring buffer test: waiting for data...\n");
        spinlock_release(SPINLOCK_ADDR);

        unsigned char byte;
        int received = 0;
        while (received < 10) {
            if (ring_buffer_get(UART_RX_BUFFER, &byte) == 0) {
                spinlock_acquire(SPINLOCK_ADDR);
                uart_puts("[Core 2] Got: ");
                uart_putc(byte);
                uart_puts("\n");
                spinlock_release(SPINLOCK_ADDR);
                received++;
            } else {
                __asm__ volatile("wfe");  // sleep until Core 1 fires SEV
            }
        }

        spinlock_acquire(SPINLOCK_ADDR);
        uart_puts("[Core 2] Ring buffer test COMPLETE\n");
        spinlock_release(SPINLOCK_ADDR);

        while (1) { __asm__ volatile("wfe"); }
    }

    if (cpu == 3) {
        jobContext_t mailbox_job = { MAILBOX_DISP_TASK, 3, "mailbox_dis", mailbox_dispatcher_task };
        sched_add_task(&mailbox_job);
        sched_run();
    }
}
/******************************************************************************
 * Function: main
 * Description: Main entry point - executed by Core 0 after boot.S
 * Parameters: None
 * Returns: None (infinite loop)
 * Responsibilities:
 *   - Initialize spinlock, uart, ringbuffer
 *   - Display boot diagnostics (EL, SP, registers)
 *   - Start secondary cores (1, 2, 3) via PSCI
 *   - Run test suites
 *   - Initialize the irq module
 *****************************************************************************/
void main(void) {
    /*********************************
     *          MAIN FLOW 
     * Spinlock Init -> move between cores
     * Uart Init -> RX TX transm no conf needed QEMU 
     * Ring Buffer Init -> Inter Core Messaging 
     * Mail Box Init -> all 4
     * I. Start the secondary cores: 1 2 3
     * II. Start Interrupt Tests -> timer_tests
     * III. Start Communication Tests -> trivial/tests.c
     * IV. Scheduler Register Tasks -> sched_add_tasks
     *******************************/
    spinlock_init();
    uart_init();
    ring_buffer_init(UART_RX_BUFFER);
    hmac_key_init(secret_key);

    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n=== Multi-Core Boot Test ===\n");
    uart_puts("[Core 0] Initializing mailboxes...\n");
    spinlock_release(SPINLOCK_ADDR);
    
    // Initialize all mailboxes
    for (int i = 0; i < 4; i++) {
        mailbox_init(i);
    }
    
    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("[Core 0] Starting secondary cores...\n\n");
    spinlock_release(SPINLOCK_ADDR);
    
    // Start cores 1, 2, 3
    for (int cpu = 1; cpu <= 3; cpu++) {
        long ret = psci_cpu_on(cpu, (unsigned long)_start);

        spinlock_acquire(SPINLOCK_ADDR);
        uart_puts("[Core 0] Core ");
        uart_putc('0' + cpu);
        uart_puts(" PSCI: ");
        uart_puthex(ret);
        uart_puts("\n");
        spinlock_release(SPINLOCK_ADDR);
        delay(3000000);
    }
    
    delay(10000000);  // Wait for all cores to boot

    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n[Core 0] === Starting Interrupt Test ===\n\n");
    spinlock_release(SPINLOCK_ADDR);

    interrupt_tests_init();

    spinlock_acquire(SPINLOCK_ADDR);
    uart_puts("\n[Core 0] === Starting Communication Test ===\n\n");
    spinlock_release(SPINLOCK_ADDR);

    run_all_tests();

    jobContext_t uart_job = { UART_RX_TASK, 0, "uart_rx", uart_rx_task };
    sched_add_task(&uart_job);

    jobContext_t consumer_job = { RING_COSUMER_TASK, 0, "ring_consumer", ring_consumer_task };
    sched_add_task(&consumer_job);

    sched_run();
}
