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

/*************************************************
 *  TYPEDEF STRUCTS UNIONS ENUMS
 ************************************************/
typedef enum {
    KEY_NONE   = 0,
    KEY_CTRL_C = 1,   /* 0x03 */
    KEY_ENTER  = 2    /* 0x0D / 0x0A */
} key_event_t;

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puthex(unsigned long value);
bool uart_hasData(void);
const unsigned int uart_special_chars(unsigned char*);
unsigned uart_getc(void);
key_event_t uart_key_event(unsigned char byte);

#endif

