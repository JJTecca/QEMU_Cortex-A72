/******************************************************************************
* File: uart0.c
* Project: RPi5-Industrial-Gateway Bare Metal Development
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

#include "uart/uart0.h"

// UART register addresses
#define UART0_DR (*((volatile unsigned int*)0x09000000))
#define UART0_FR (*((volatile unsigned int*)0x09000018))

static const char hex_chars[] = "0123456789ABCDEF";

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
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex_chars[(val >> i) & 0xF]);
    }
}

