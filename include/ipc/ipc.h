/******************************************************************************
* File: ipc.h
* Description: Inter-Core Communication using Shared Memory
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/


/**************************************************
 * INCLUDE FILES
 ***************************************************/

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
#ifndef IPC_H
#define IPC_H
// Shared memory layout - placed after stacks
// Stacks: 0x40200000 - 0x40210000 (4 cores × 16KB)
#define SHARED_MEM_BASE     0x40220000
#define SPINLOCK_ADDR       ((volatile unsigned int*)0x40220000)
#define MAILBOX_BASE        0x40220100
#define MSG_NONE            0
#define MSG_PING            1
#define MSG_DATA            2
#define MSG_ACK             3
#define MSG_SHUTDOWN        4

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/
typedef struct {
    volatile unsigned int lock;         // Spinlock for this mailbox
    volatile unsigned int msg_type;     // Message type
    volatile unsigned int msg_data;     // Message payload
    volatile unsigned int sender_id;    // Source core ID
    volatile unsigned int status;       // 0=empty, 1=ready, 2=processed
    volatile unsigned int counter;      // Message counter
} mailbox_t;

#define GET_MAILBOX(core_id) ((volatile mailbox_t*)(MAILBOX_BASE + (core_id) * sizeof(mailbox_t)))

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/
void spinlock_init(void);
void spinlock_acquire(volatile unsigned int *lock);
void spinlock_release(volatile unsigned int *lock);
void mailbox_init(int core_id);
int  mailbox_send(int dest_core, int msg_type, unsigned int data);
int  mailbox_receive(int core_id, unsigned int *sender, unsigned int *msg_type, unsigned int *data);
void mailbox_clear(int core_id);

#endif
