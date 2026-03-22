/******************************************************************************
 * File: sched.h
 * Project: RPi5-Industrial-Gateway Bare Metal Development
 *
 * Cooperative round-robin task scheduler for AArch64 bare metal.
 * Each core maintains its own independent task list.
 * Tasks call task_yield() to give up the CPU voluntarily.
 * task_sleep_ms() suspends a task for N milliseconds using the
 * ARM generic timer tick driven by sched_tick().
 *
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/

#ifndef SCHED_H
#define SCHED_H

/**************************************************
 * INCLUDE FILES
 ***************************************************/
#include <stdint.h>
#include "dispatcher.h"

/**************************************************
 * MACRO DEFINTIONS
 ***************************************************/
#define MAX_TASKS        8      /* max tasks per core                        */
#define TASK_STACK_SIZE  2048   /* 2 KB private stack per task               */
#define CORE_COUNT       4      /* BCM2712 quad-core                         */

/*************************************************
 *  TYPEDEF STRUCTS UNIONS ENUMS
 ************************************************/
typedef enum {
    TASK_READY    = 0,   /* runnable, waiting for its turn                  */
    TASK_RUNNING  = 1,   /* currently executing on this core                */
    TASK_SLEEPING = 2,   /* blocked until wake_tick is reached              */
    TASK_DEAD     = 3    /* finished (future use)                           */
} task_state_t;

typedef struct {
    uint64_t      sp;                     /* saved SP */
    uint16_t      id;                       /* the task ID for dispatcher */
    uint8_t       stack[TASK_STACK_SIZE]; /* private stack, grows downward  */
    task_state_t  state;
    uint64_t      wake_tick;              /* wake when tick_count >= this   */
    void        (*entry)(void);           /* task entry function            */
    const char   *name;                   /* debug label                    */
} tcb_t;

void sched_add_task(jobContext_t *job);
void sched_run(void);
void task_yield(void);
void task_sleep_ms(uint32_t ms);
void sched_tick(void);
extern void sched_context_switch(tcb_t *old_tcb, tcb_t *new_tcb);

#endif /* SCHED_H */
