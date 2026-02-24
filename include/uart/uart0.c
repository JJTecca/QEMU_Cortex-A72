/******************************************************************************
* File: uart0.c
* Project: RPi5-Industrial-Gateway Bare Metal Development
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "uart/uart0.h"

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
#ifdef TARGET_QEMU
    #define UART0_BASE 0x09000000
#elif defined(TARGET_RPI5)
    #define UART0_BASE  0x40030000
    #define UART_CR_OFFSET    0x30
    #define UART_IBRD_OFFSET  0x24
    #define UART_FBRD_OFFSET  0x28
    #define UART_LCRH_OFFSET  0x2C
    #define UART_IMSC_OFFSET  0x38
    #define UART_ICR_OFFSET   0x44
#else 
    #error "Target undefined! Use an official platform"
#endif
#define UART_DR_OFFSET   0x00
#define UART_FR_OFFSET   0x18

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/
volatile unsigned int* const uart0_dr = (volatile unsigned int*)(UART0_BASE + UART_DR_OFFSET);
volatile unsigned int* const uart0_fr = (volatile unsigned int*)(UART0_BASE + UART_FR_OFFSET);

static const char hex_chars[] = "0123456789ABCDEF";

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/
void uart_init() {
    #if defined(TARGET_QEMU)
        (void)0; // No configuration needed

    #elif defined(TARGET_RPI5)
    /**************
     * IBRD = Integer baud rate 
     * CR = Control Register
     * FBRD =Fractional baud rate
     * LCRH  = Line Control Register High => FIFO = 1 Use hardware register
     * IMSC = Interrupt Mask Set/Clear
     * ICR = Interrupt Clear Register
     *************/
        volatile unsigned int* uart_cr   = (volatile unsigned int*)(UART0_BASE + UART_CR_OFFSET);
        volatile unsigned int* uart_ibrd = (volatile unsigned int*)(UART0_BASE + UART_IBRD_OFFSET);
        volatile unsigned int* uart_fbrd = (volatile unsigned int*)(UART0_BASE + UART_FBRD_OFFSET);
        volatile unsigned int* uart_lcrh = (volatile unsigned int*)(UART0_BASE + UART_LCRH_OFFSET);
        volatile unsigned int* uart_imsc = (volatile unsigned int*)(UART0_BASE + UART_IMSC_OFFSET);
        volatile unsigned int* uart_icr  = (volatile unsigned int*)(UART0_BASE + UART_ICR_OFFSET);

        /* 1. Disable UART before configuration */
        *uart_cr = 0;
        /* 2. Clear all pending interrupts */
        *uart_icr = 0x7FF;

        /*************************************** 
        * Target: 115200 baud
        * Clock:  48 MHz (standard RPi UART clock)
        * Divider = 48,000,000 / (16 * 115200) = 26.041666...
        * IBRD = 26
        * FBRD = 0.041666 * 64 + 0.5 = 3
        *********************************************/
        *uart_ibrd = 26;
        *uart_fbrd = 3;

        /******************************************
        * Bit 4 (FEN) = 1 (Enable FIFOs)
        * Bit 5-6 (WLEN) = 11 (8-bit word length)
        ********************************************/
        *uart_lcrh = (1 << 4) | (3 << 5);

        /* 5. Mask all interrupts*/
        *uart_imsc = 0;

        /* 6. Enable UART, TX, and RX */
        *uart_cr = (1 << 0) | (1 << 8) | (1 << 9);
    #endif
}

void uart_putc(char c) {
    // Wait while transmit FIFO is full (bit 5 of FR register)
    while (*uart0_fr & (1 << 5));
    *uart0_dr = c;
}

void uart_puts(const char* str) {
    while (*str) {
        if (*str == '\n') uart_putc('\r');
        uart_putc(*str++);
    }
}

void uart_puthex(unsigned long val) {
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex_chars[(val >> i) & 0xF]);
    }
}

bool uart_has_data(void) {
    // RXFE (Receive FIFO Empty) bit 4: 0 = data available, 1 = empty
    return (*uart0_fr & (1 << 4)) == 0;
}


unsigned uart_getc(void) {
    // FR = 0 -> FIFO has data => while(true) => "WAIT"
    // i.e if bit 4 is up => FR = 1 => empty => while(false)
    while(! uart_has_data()); // Empty means FR = 1
    return (unsigned char)(*uart0_dr & 0xFF);
}
