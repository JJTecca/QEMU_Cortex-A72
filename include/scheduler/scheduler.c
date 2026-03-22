/******************************************************************************
 * File: scheduler.c
 * Description: None
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/

 /**************************************************
 * INCLUDE FILES
 ***************************************************/
#include "scheduler/scheduler.h"
#include "uart/uart0.h"

 /**************************************************
 * MACRO DEFINITIONS
 ***************************************************/

 /**************************************************
 * GLOBAL VARIABLES
 ***************************************************/
static tcb_t task_pool[CORE_COUNT][MAX_TASKS]; // Maximum amount of tasks per core is undefined for now
static uint32_t task_count[CORE_COUNT];
static uint32_t current_task[CORE_COUNT];
static volatile uint64_t tick_count[CORE_COUNT]; // ms counter / core

 /**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

static inline uint32_t get_core_id(void)
{
    uint64_t id;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(id));
    return (uint32_t)(id & 0xFF);
}

/******************************************************************************
 * Function: sched_add_task
 * Description: Registers one task into the scheduler (basically save the state with SP)
 * Parameters: *entry (function) , task description
 * Returns: None
 *****************************************************************************/
void sched_add_task(jobContext_t *job)
{
    uint32_t core = get_core_id();
    uint32_t idx  = task_count[core];
    tcb_t *t      = &task_pool[core][idx];
    t->id    = job->id;
    t->entry = job->entry;
    t->name  = job->task_name;
    t->state = TASK_READY;
    t->wake_tick = 0;

    /*
     * Build the initial stack frame that sched_context_switch expects.
     * It saves/restores 6 register pairs (x19-x30) = 12 x uint64_t = 96 bytes.
     * We pre-fill them as zeroes except x30 = entry address, so the very
     * first 'ret' in sched_context_switch jumps into entry().
     */
    uint64_t *stack_top = (uint64_t *)(t->stack + TASK_STACK_SIZE);
    stack_top -= 12;
    for (int i = 0; i < 12; i++) stack_top[i] = 0;
    stack_top[1] = (uint64_t)job->entry;   // ← slot 11 = x30
    t->sp = (uint64_t)stack_top;

    task_count[core]++;

    uart_puts("[SCHEDULE] Core ");
    uart_putc('0' + core);
    uart_puts(": registered '");
    uart_puts(job->task_name);
    uart_puts("'\n");
}


void sched_tick(void)
{
    uint32_t core = get_core_id();
    tick_count[core]++;

    for (uint32_t i = 0; i < task_count[core]; i++) {
        tcb_t *t = &task_pool[core][i];
        if (t->state == TASK_SLEEPING && tick_count[core] >= t->wake_tick)
            t->state = TASK_READY;
    }
}

/******************************************************************************
 * Function: pick_next
 * Description: Return the first task which occurs to be ready to be executed
 * Parameters: core
 * Returns: task
 *****************************************************************************/
static uint32_t pick_next(uint32_t core)
{
    uint32_t count = task_count[core];
    uint32_t cur   = current_task[core];

    for (uint32_t i = 1; i <= count; i++) {
        uint32_t candidate = (cur + i) % count;
        if (task_pool[core][candidate].state == TASK_READY)
            return candidate;
    }
    return cur;   /* nobody else ready — stay on current */
}

/******************************************************************************
 * Function: task_yield
 * Description: Pick the next task when and switch contexts (SP switch)
 * Parameters: 
 * Returns: None
 *****************************************************************************/
void task_yield(void)
{
    uint32_t core    = get_core_id();
    uint32_t old_idx = current_task[core];
    uint32_t new_idx = pick_next(core);

    if (old_idx == new_idx) return;   /* only one ready task — nothing to do */

    tcb_t *old_tcb = &task_pool[core][old_idx];
    tcb_t *new_tcb = &task_pool[core][new_idx];

    old_tcb->state     = TASK_READY;
    new_tcb->state     = TASK_RUNNING;
    current_task[core] = new_idx;

    sched_context_switch(old_tcb, new_tcb);
}


void task_sleep_ms(uint32_t ms)
{
    uint32_t core = get_core_id();
    uint32_t current_task_core = current_task[core];
    tcb_t   *t    = &task_pool[core][current_task_core];

    t->state     = TASK_SLEEPING;
    t->wake_tick = tick_count[core] + (uint64_t)ms;
    task_yield();
    /* returns here ~ms milliseconds later */
}


/******************************************************************************
 * Function: sched_run
 * Description: Enter first function from the scheduler on X core
 * Parameters: None
 * Returns: None
 *****************************************************************************/
void sched_run(void)
{
    uint32_t core = get_core_id();

    if (task_count[core] == 0) {
        uart_puts("[SCHED] Core ");
        uart_putc('0' + core);
        uart_puts(": no tasks - wait for event\n");
        while (1) { __asm__ volatile("wfe"); }
    }

    uart_puts("[SCHEDULE] Core ");
    uart_putc('0' + core);
    uart_puts(": starting ");
    uart_putc('0' + task_count[core]);
    uart_puts(" task(s)\n");

    current_task[core] = 0;
    task_pool[core][0].state = TASK_RUNNING;

    //Using a boot temp as entry to make the switch 
    static tcb_t boot_temp[CORE_COUNT];
    sched_context_switch(&boot_temp[core], &task_pool[core][0]);

    while (1) { __asm__ volatile("wfe"); }
}
