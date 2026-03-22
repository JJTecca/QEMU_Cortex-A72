/******************************************************************************
* File: dispatcher.c
* Description: Task runner — calls the correct task function by ID
* Copyright (c) 2026 Maior Cristian
*****************************************************************************/

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "dispatcher.h"
#include "uart/uart0.h"
#include "ipc/ipc.h"
#include "scheduler/scheduler.h"
#include "ringbuffer/ringbuf.h"

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/

/**************************************************
 * GLOBAL VARIABLES
 ***************************************************/

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
 * Function: get_cpu_id
 * Description: Reads the current CPU core ID from MPIDR_EL1 register
 * Parameters: None
 * Returns: CPU affinity level 0 (core ID: 0-3)
 *****************************************************************************/
unsigned long get_cpu_id(void) {
    unsigned long id;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(id));
    return id & 0xFF;
}

void uart_rx_task(void) {
    /* Simulating real time keyboard */
    while (1) {
        if(uart_has_data()) {
            unsigned char c = (unsigned char)uart_getc();
            ring_buffer_put(UART_RX_BUFFER, c);
            __asm__ volatile("sev" ::: "memory");
        }
        task_yield();
    }
}

void ring_consumer_task(void) {
    unsigned char byte = '\0';
    while (1) {
        if (ring_buffer_get(UART_RX_BUFFER, &byte) == 0) {

            /* Halt the system when ctr+c arrives */
            spinlock_acquire(SPINLOCK_ADDR);
            switch (uart_key_event(byte)) {
                case KEY_CTRL_C:
                    uart_puts("\r\n[ERROR] Keyboard locked. System halted.\r\n");
                    while (1) { __asm__ volatile("wfe"); }
                    break;
                case KEY_ENTER:
                    uart_puts("\r\n");
                    break;
                case KEY_NONE:
                default:
                    uart_putc(byte);
                    break;
            }
            spinlock_release(SPINLOCK_ADDR);
        }
        task_yield();
    }
}

void mailbox_dispatcher_task(void) {
    unsigned int sender, msg_type, msg_data;
    unsigned long cpu = get_cpu_id();   // will always be 3 when this runs

    while (1) {
        if (mailbox_receive(cpu, &sender, &msg_type, &msg_data) == 1) {

            spinlock_acquire(SPINLOCK_ADDR);
            uart_puts("[Core 3] RX from Core "); uart_putc('0' + sender);
            uart_puts(" | Type: ");              uart_putc('0' + msg_type);
            uart_puts(" | Data: ");              uart_puthex(msg_data);
            uart_puts("\n");
            spinlock_release(SPINLOCK_ADDR);

            unsigned int ack_data = msg_data + (cpu << 16);
            mailbox_send(sender, MSG_ACK, ack_data);
            mailbox_clear(cpu);
        }
        task_yield();   // nothing in mailbox → give turn to next task
    }
}

/**************************************************
* Function: dispatcher_run
* Description: Receives a task ID and calls the matching task function.
*              Called by the scheduler or any module that needs to
*              trigger a specific task by its numeric ID.
* Parameters: task_id — one of the TASK_ID macros from dispatcher.h
* Returns: None
***************************************************/
void dispatcher_run(uint16_t task_id)
{
    switch (task_id) {

        case UART_RX_TASK:
            uart_rx_task();
            break;

        case RING_COSUMER_TASK:
            ring_consumer_task();
            break;

        case MAILBOX_DISP_TASK:
            mailbox_dispatcher_task();
            break;

        default:
            uart_puts("[DISPATCHER] Unknown task ID: ");
            uart_puthex(task_id);
            uart_puts("\n");
            break;
    }
}
 