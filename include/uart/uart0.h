/******************************************************************************
* File: uart0.h
* Project: RPi5-Industrial-Gateway Bare Metal Development
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include <stdbool.h>
/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
#ifndef UART0_H
#define UART0_H

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(unsigned long value);
bool uart_hasData(void);
unsigned uart_getc(void);

#endif

