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

/******************************************************************************
 * Macro Definition
 *****************************************************************************/
#define PSCI_CPU_ON 0xC4000003
#define LOCK_ADDR ((volatile int*)0x40300000)

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
 * Function: get_cpu_id
 * Description: Reads the current CPU core ID from MPIDR_EL1 register
 * Parameters: None
 * Returns: CPU affinity level 0 (core ID: 0-3)
 *****************************************************************************/
unsigned long get_cpu_id(void) {
    unsigned long id;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(id));
    return id & 0xFF;
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
 * Function: simple_lock
 * Description: Acquires a simple spinlock for UART synchronization
 * Parameters: None
 * Returns: None
 * Note: Spins until lock is available, then sets lock
 *****************************************************************************/
void simple_lock(void) {
    while (*LOCK_ADDR != 0);
    *LOCK_ADDR = 1;
}

/******************************************************************************
 * Function: simple_unlock
 * Description: Releases the spinlock
 * Parameters: None
 * Returns: None
 *****************************************************************************/
void simple_unlock(void) {
    *LOCK_ADDR = 0;
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
    
    simple_lock();
    uart_puts("[Core ");
    uart_putc('0' + cpu);
    uart_puts("] Online! SP: ");
    uart_puthex(read_sp());
    uart_puts(" EL: ");
    uart_putc('0' + read_current_el());
    uart_puts("\n");
    simple_unlock();
    
    //No more wait-for-event (wfe)
    //Error caused : buffer overflow in makefile 
    delay(20000000);
    //Declare needed variables as unsigned int
    unsigned int sender, msg_type, msg_data;
    uart_puts("\n[Core 0] Checking for ACK responses...\n");
    while (1) {
        if (mailbox_receive(cpu, &sender, &msg_type, &msg_data) == 1) {
            spinlock_acquire(SPINLOCK_ADDR);
            uart_puts("[Core ");
            uart_putc('0' + cpu);
            uart_puts("] RX from Core ");
            uart_putc('0' + sender);
            uart_puts(" | Type: ");
            uart_putc('0' + msg_type);
            uart_puts(" | Data: ");
            uart_puthex(msg_data);
            uart_puts("\n");
            spinlock_release(SPINLOCK_ADDR);
            
            // Send ACK back to sender
            unsigned int ack_data = msg_data + (cpu << 16);
            mailbox_send(sender, MSG_ACK, ack_data);
            
            mailbox_clear(cpu);
        }
        
        __asm__ volatile("wfe");  // Wait for event
    }
}

/******************************************************************************
 * Function: main
 * Description: Main entry point - executed by Core 0 after boot.S
 * Parameters: None
 * Returns: None (infinite loop)
 * Responsibilities:
 *   - Initialize spinlock
 *   - Display boot diagnostics (EL, SP, registers)
 *   - Start secondary cores (1, 2, 3) via PSCI
 *   - Enter low-power idle loop
 *****************************************************************************/
void main(void) {
    spinlock_init();
    
    uart_puts("\n=== Multi-Core IPC Test ===\n");
    uart_puts("[Core 0] Initializing mailboxes...\n");
    
    // Initialize all mailboxes
    for (int i = 0; i < 4; i++) {
        mailbox_init(i);
    }
    
    uart_puts("[Core 0] Starting secondary cores...\n\n");
    
    // Start cores 1, 2, 3
    for (int cpu = 1; cpu <= 3; cpu++) {
        long ret = psci_cpu_on(cpu, (unsigned long)_start);
        uart_puts("[Core 0] Core ");
        uart_putc('0' + cpu);
        uart_puts(" PSCI: ");
        uart_puthex(ret);
        uart_puts("\n");
        delay(3000000);
    }
    
    delay(10000000);  // Wait for all cores to boot
    
    uart_puts("\n[Core 0] === Starting Communication Test ===\n\n");
    
    // Test 1: Send PING to each core
    uart_puts("[Core 0] Test 1: Sending PING to all cores\n");
    for (int dest = 1; dest <= 3; dest++) {
        unsigned int test_data = 0x1000 + dest;
        if (mailbox_send(dest, MSG_PING, test_data) == 0) {
            uart_puts("[Core 0] -> Core ");
            uart_putc('0' + dest);
            uart_puts(" PING sent\n");
        }
        delay(2000000);
    }
    
    // Wait for ACKs
    delay(10000000);
    
    // Test 2: Send DATA messages
    uart_puts("\n[Core 0] Test 2: Sending DATA messages\n");
    for (int dest = 1; dest <= 3; dest++) {
        unsigned int test_data = 0xDEAD0000 + (dest * 0x100);
        mailbox_send(dest, MSG_DATA, test_data);
        uart_puts("[Core 0] -> Core ");
        uart_putc('0' + dest);
        uart_puts(" DATA: ");
        uart_puthex(test_data);
        uart_puts("\n");
        delay(2000000);
    }
    
    // Process ACKs from secondary cores
    delay(10000000);
    unsigned int sender, msg_type, msg_data;
    uart_puts("\n[Core 0] Checking for ACK responses...\n");
    for (int i = 0; i < 10; i++) {
        if (mailbox_receive(0, &sender, &msg_type, &msg_data) == 1) {
            uart_puts("[Core 0] <- ACK from Core ");
            uart_putc('0' + sender);
            uart_puts(" | Data: ");
            uart_puthex(msg_data);
            uart_puts("\n");
            mailbox_clear(0);
        }
        delay(3000000);
    }
    
    uart_puts("\n[Core 0] === Communication Test Complete ===\n");
    
    while (1) { __asm__ volatile("wfe"); }
}
