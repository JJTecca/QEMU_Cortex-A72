/******************************************************************************
* File: ringbuf.c
* Project: RPi5-Industrial-Gateway Bare Metal Development
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "ringbuf.h"

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
* Function: ring_buffer_init
* Description: Initialise the ring buffer by resetting head and tail to zero
* Parameters: rb - pointer to the ring buffer in shared memory
* Returns: None
*****************************************************************************/
void ring_buffer_init(volatile ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

/******************************************************************************
* Function: ring_buffer_put
* Description: Write one byte into the ring buffer (called by Core 1)
* Parameters: rb - pointer to ring buffer
*             c  - byte to store
* Returns: 0 on success, -1 if buffer is full
*****************************************************************************/
int ring_buffer_put(volatile ring_buffer_t *rb, unsigned char c) {
    // Do not exceed the 255 buffer size : do the 255 & head 
    // Example : (0x00000000 + 1) & 255 = 1 
    unsigned int next_head = (rb->head + 1) & (RING_BUFFER_SIZE - 1);

    if (next_head == rb->tail) {
        return -1;  // Buffer full — Core 2 is not keeping up
    }

    rb->data[rb->head] = c;
    __asm__ volatile("dmb sy" ::: "memory");  // Ensure byte written before head moves
    rb->head = next_head;
    return 0;
}

/******************************************************************************
* Function: ring_buffer_get
* Description: Read one byte from the ring buffer (called by Core 2)
* Parameters: rb - pointer to ring buffer
*             c  - pointer to store the retrieved byte
* Returns: 0 on success, -1 if buffer is empty
*****************************************************************************/
int ring_buffer_get(volatile ring_buffer_t *rb, unsigned char *c) {
    if (rb->head == rb->tail) {
        return -1;  // Buffer empty
    }

    __asm__ volatile("dmb sy" ::: "memory");  // Ensure we read current memory state
    *c = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) & (RING_BUFFER_SIZE - 1);
    return 0;
}
