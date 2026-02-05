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
#define UART0_BASE 0x09000000
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
