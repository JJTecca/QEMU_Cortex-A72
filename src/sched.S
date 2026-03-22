/******************************************************************************
 * File: sched_asm.S
 * Project: RPi5-Industrial-Gateway Bare Metal Development
 *
 * void sched_context_switch(tcb_t *old_tcb, tcb_t *new_tcb)
 *
 *   x0 = old TCB pointer  — save current SP here  (old_tcb->sp at offset 0)
 *   x1 = new TCB pointer  — load next SP from here (new_tcb->sp at offset 0)
 *
 * Only callee-saved registers are touched (AArch64 ABI: x19-x30).
 * x30 (LR) holds the return address back into task_yield() — or for a
 * brand-new task, sched_add_task() pre-loaded it with entry().
 *
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/

.section ".text", "ax"
.global sched_context_switch
.type   sched_context_switch, %function

sched_context_switch:

    /* ── 1. Push callee-saved regs onto OLD task's stack ── */
    stp  x19, x20, [sp, #-16]!
    stp  x21, x22, [sp, #-16]!
    stp  x23, x24, [sp, #-16]!
    stp  x25, x26, [sp, #-16]!
    stp  x27, x28, [sp, #-16]!
    stp  x29, x30, [sp, #-16]!

    /* ── 2. Save current SP → old_tcb->sp  (offset 0) ── */
    mov  x2, sp
    str  x2, [x0, #0]           /* ← was #2048, must be #0 */

    /* ── 3. Load new SP ← new_tcb->sp  (offset 0) ── */
    ldr  x2, [x1, #0]           /* ← was #2048, must be #0 */
    mov  sp, x2

    /* ── 4. Pop callee-saved regs from NEW task's stack ── */
    ldp  x29, x30, [sp], #16
    ldp  x27, x28, [sp], #16
    ldp  x25, x26, [sp], #16
    ldp  x23, x24, [sp], #16
    ldp  x21, x22, [sp], #16
    ldp  x19, x20, [sp], #16

    ret

.size sched_context_switch, . - sched_context_switch
