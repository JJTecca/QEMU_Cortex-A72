/******************************************************************************
 * File: dispatcher.h
 * Description: Handler of the tasks added into the stack (better performance)
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/
 #pragma once

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include <stdint.h>

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
#ifndef DISPATCHER_H
#define DISPATCHER_H

/* TODO : Add here the next */
#define UART_RX_TASK (0UL)
#define RING_COSUMER_TASK (1UL)
#define MAILBOX_DISP_TASK (2UL)

/*************************************************
 *  TYPEDEF STRUCTS UNIONS ENUMS
 ************************************************/
typedef struct {
    uint16_t    id;             /* id which is the macro */
    uint16_t    core_id;        /* which core is the jobContext on*/
    const char *task_name;      /* task__name = "uart_rx" i.e the driver name */
    void        (*entry)(void); /* entry point of the function */
}jobContext_t;


/* TODO : FIX ON ALL*/
/**************************************************
* FUNCTION PROTOTYPE
***************************************************/
unsigned long get_cpu_id(void);
void uart_rx_task(void);
void ring_consumer_task(void);
void mailbox_dispatcher_task(void);
void dispatcher_run(uint16_t task_id);

#endif