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
bool uart_has_data(void);
const unsigned int uart_special_chars(unsigned char*);
unsigned uart_getc(void);
key_event_t uart_key_event(unsigned char byte);

#endif

