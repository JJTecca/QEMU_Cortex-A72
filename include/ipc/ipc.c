/******************************************************************************
* File: ipc.c
* Description: Inter-Core Communication Implementation
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "ipc/ipc.h"

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
* Function: spinlock_init
* Description: Initialize the global spinlock
*****************************************************************************/
void spinlock_init(void) {
    *SPINLOCK_ADDR = 0;
}

/******************************************************************************
* Function: spinlock_acquire
* Description: Acquire spinlock using ARM exclusive instructions (LDXR/STXR)
* This provides proper atomic lock acquisition
*****************************************************************************/
void spinlock_acquire(volatile unsigned int *lock) {
    unsigned int tmp, val;
    
    __asm__ volatile(
        "   mov   %w1, #1           \n"  // Prepare lock value
        "1: ldxr  %w0, [%2]         \n"  // Load exclusive
        "   cbnz  %w0, 1b           \n"  // If locked, retry
        "   stxr  %w0, %w1, [%2]    \n"  // Store exclusive
        "   cbnz  %w0, 1b           \n"  // If store failed, retry
        "   dmb   sy                \n"  // Data memory barrier
        : "=&r" (tmp), "=&r" (val)
        : "r" (lock)
        : "memory"
    );
}

/******************************************************************************
* Function: spinlock_release
* Description: Release spinlock with memory barrier
*****************************************************************************/
void spinlock_release(volatile unsigned int *lock) {
    __asm__ volatile("dmb sy" ::: "memory");  // Memory barrier
    *lock = 0;
    __asm__ volatile("dsb sy" ::: "memory");  // Data synchronization barrier
    __asm__ volatile("sev"    ::: "memory");  // Signal event (wake WFE cores)
}

/******************************************************************************
* Function: mailbox_init
* Description: Initialize a core's mailbox
*****************************************************************************/
void mailbox_init(int core_id) {
    volatile mailbox_t *mb = GET_MAILBOX(core_id);
    mb->lock = 0;
    mb->msg_type = MSG_NONE;
    mb->msg_data = 0;
    mb->sender_id = 0xFF;
    mb->status = 0;
    mb->counter = 0;
}

/******************************************************************************
* Function: mailbox_send
* Description: Send message to another core's mailbox
* Returns: 0 on success, -1 if mailbox full
*****************************************************************************/
int mailbox_send(int dest_core, int msg_type, unsigned int data) {
    volatile mailbox_t *mb = GET_MAILBOX(dest_core);
    
    spinlock_acquire((volatile unsigned int*)&mb->lock);
    
    if (mb->status == 1) {
        spinlock_release((volatile unsigned int*)&mb->lock);
        return -1; 
    }
    
    // Write message
    unsigned int sender;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(sender));
    sender = sender & 0xFF;
    
    mb->sender_id = sender;
    mb->msg_type = msg_type;
    mb->msg_data = data;
    mb->status = 1;  // Mark as ready
    mb->counter++;
    
    spinlock_release((volatile unsigned int*)&mb->lock);
    
    // Wake up destination core
    __asm__ volatile("sev" ::: "memory");
    
    return 0;
}

/******************************************************************************
* Function: mailbox_receive
* Description: Receive message from own mailbox (non-blocking)
* Returns: 1 if message received, 0 if no message, -1 on error
*****************************************************************************/
int mailbox_receive(int core_id, unsigned int *sender, unsigned int *msg_type, unsigned int *data) {
    volatile mailbox_t *mb = GET_MAILBOX(core_id);
    int result = 0;
    
    spinlock_acquire((volatile unsigned int*)&mb->lock);
    
    if (mb->status == 1) {
        // Message available
        *sender = mb->sender_id;
        *msg_type = mb->msg_type;
        *data = mb->msg_data;
        mb->status = 2;  // Mark as being processed
        result = 1;
    }
    
    spinlock_release((volatile unsigned int*)&mb->lock);
    return result;
}

/******************************************************************************
* Function: mailbox_clear
* Description: Clear mailbox after processing message
*****************************************************************************/
void mailbox_clear(int core_id) {
    volatile mailbox_t *mb = GET_MAILBOX(core_id);
    
    spinlock_acquire((volatile unsigned int*)&mb->lock);
    mb->status = 0;
    mb->msg_type = MSG_NONE;
    spinlock_release((volatile unsigned int*)&mb->lock);
}
