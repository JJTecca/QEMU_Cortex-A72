// UART registers
#define UART0_DR (*((volatile unsigned int*)0x09000000))
#define UART0_FR (*((volatile unsigned int*)0x09000018))

// PSCI function ID
#define PSCI_CPU_ON 0xC4000003

// Simple lock
volatile int uart_lock = 0;

void uart_putc(char c) {
    while (UART0_FR & (1 << 5));
    UART0_DR = c;
}

void uart_puts(const char* str) {
    while (*str) {
        if (*str == '\n') uart_putc('\r');
        uart_putc(*str++);
    }
}

void uart_puthex(unsigned long val) {
    const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex[(val >> i) & 0xF]);
    }
}

void delay(int n) {
    for (volatile int i = 0; i < n; i++);
}

unsigned long get_cpu_id(void) {
    unsigned long id;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(id));
    return id & 0xFF;
}

unsigned long read_sp(void) {
    unsigned long sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

unsigned long read_current_el(void) {
    unsigned long el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
    return (el >> 2) & 0x3;
}

unsigned long read_sctlr(void) {
    unsigned long sctlr;
    __asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr));
    return sctlr;
}

// Simplified lock
void simple_lock(void) {
    while (1) {
        int result = 0;
        __asm__ volatile(
            "ldaxr w1, [%2]\n"
            "cmp w1, wzr\n"
            "b.ne 1f\n"
            "mov w1, #1\n"
            "stxr w2, w1, [%2]\n"
            "cmp w2, wzr\n"
            "b.ne 1f\n"
            "mov %w0, #1\n"
            "b 2f\n"
            "1: wfe\n"
            "2:\n"
            : "=r"(result)
            : "r"(0), "r"(&uart_lock)
            : "w1", "w2", "memory"
        );
        if (result) break;
    }
}

void simple_unlock(void) {
    __asm__ volatile("stlr wzr, [%0]" : : "r"(&uart_lock) : "memory");
    __asm__ volatile("sev");
}

// PSCI CPU_ON
long psci_cpu_on(unsigned long cpu, unsigned long entry) {
    register unsigned long x0 __asm__("x0") = PSCI_CPU_ON;
    register unsigned long x1 __asm__("x1") = cpu;
    register unsigned long x2 __asm__("x2") = entry;
    register unsigned long x3 __asm__("x3") = 0;
    __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

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

    while (1) { __asm__ volatile("wfe"); }
}

void main(void) {
    uart_puts("\n=== Multi-Core Boot Diagnostics ===\n");
    uart_puts("[Core 0] Starting...\n");

    // Verify boot state
    uart_puts("[Core 0] Exception Level: ");
    uart_putc('0' + read_current_el());
    uart_puts("\n[Core 0] Stack Pointer: ");
    uart_puthex(read_sp());
    uart_puts("\n[Core 0] MPIDR_EL1: ");
    uart_puthex(get_cpu_id());
    uart_puts("\n[Core 0] SCTLR_EL1: ");
    uart_puthex(read_sctlr());
    uart_puts("\n\n");

    // Start cores 1, 2, 3
    for (int cpu = 1; cpu <= 3; cpu++) {
        uart_puts("[Core 0] Starting core ");
        uart_putc('0' + cpu);
        uart_puts("...");

        long ret = psci_cpu_on(cpu, (unsigned long)_start);
        uart_puts(" PSCI ret: ");
        uart_puthex(ret);
        uart_puts("\n");

        delay(3 * 8000000);
    }

    uart_puts("\n[Core 0] Waiting for all cores...\n");
    delay(3 * 8000000);
    uart_puts("[Core 0] Done!\n");

    while (1) { __asm__ volatile("wfe"); }
}