// QEMU UART base address (mem map)
#define UART0_BASE_QEMU 0x09000000

// Pi5 RP1 UART base address (mem map)
#define UART0_BASE_RPI5 0x40030000

// Select base based on platform 
// Select hardarwe vs QEMU version
#ifdef PLATFORM_QEMUVIRT
    #define UART0_BASE UART0_BASE_QEMU
#else
    #define UART0_BASE UART0_BASE_RPI5
#endif

// UART registers (base + offset)
#define UART0_DR   (*(volatile unsigned int*)(UART0_BASE + 0x00))
#define UART0_FR   (*(volatile unsigned int*)(UART0_BASE + 0x18))

void uart_putc(char c) {
    // Wait until TX FIFO not full
    while (UART0_FR & (1 << 5));
    UART0_DR = c;
}

void uart_puts(const char* str) {
    while (*str) {
        if (*str == '\n')
            uart_putc('\r');
        uart_putc(*str++);
    }
}

void main(void) {
    uart_puts("Hello from bare metal!\n");
    uart_puts("CPU booted successfully\n");
    
    // Infinite loop
    while (1) {
        __asm__ volatile("wfe");
    }
}
