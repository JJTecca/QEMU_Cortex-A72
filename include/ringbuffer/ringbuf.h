/******************************************************************************
* File: ringbuf.h
* Project: RPi5-Industrial-Gateway Bare Metal Development
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#ifndef RINGBUF_H
#define RINGBUF_H

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
#define RING_BUFFER_BASE 0x40220200   // Right after your mailboxes (end at 0x4022015F)
#define RING_BUFFER_SIZE 256

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/
typedef struct {
    volatile unsigned int  head;             // Core 1 writes here
    volatile unsigned int  tail;             // Core 2 reads here
    volatile unsigned char data[RING_BUFFER_SIZE];
} ring_buffer_t;

#define UART_RX_BUFFER ((volatile ring_buffer_t*)RING_BUFFER_BASE)

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/
void ring_buffer_init(volatile ring_buffer_t *rb);
int  ring_buffer_put (volatile ring_buffer_t *rb, unsigned char c);
int  ring_buffer_get (volatile ring_buffer_t *rb, unsigned char *c);

#endif
