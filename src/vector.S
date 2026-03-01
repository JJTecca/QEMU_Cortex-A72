/******************************************************************************
* File: src/interrupts/vector.S
* Project: RPi5-Industrial-Gateway Bare Metal Development
*
* Target Hardware: QEMU virt (Cortex-A72) / Raspberry Pi 5 (Cortex-A76)
* Architecture:    AArch64, EL1 with SPx (SP_EL1 active on all cores)
*
* Description:
*   AArch64 Exception Vector Table (IVT) + exception handler stubs.
*
*   Table layout: 16 entries x 128 bytes = 2048 bytes (0x800).
*   Each entry is a single 'b' branch — actual save/restore code lives
*   in .text below (no 128-byte size limit there).
*
*   Active group: "Current EL with SPx" (VBAR+0x200 to VBAR+0x37F).
*   All 4 cores boot at EL1 with SPSel=1, so SPx group fires for all IRQs.
*
*   boot.S writes 'vectors' to VBAR_EL1 (after stack setup, before main).
*
* Exception frame layout on stack (EXC_FRAME_SIZE = 0x110 = 272 bytes):
*   0x000  x0  / x1      0x010  x2  / x3
*   0x020  x4  / x5      0x030  x6  / x7
*   0x040  x8  / x9      0x050  x10 / x11
*   0x060  x12 / x13     0x070  x14 / x15
*   0x080  x16 / x17     0x090  x18 / x19
*   0x0A0  x20 / x21     0x0B0  x22 / x23
*   0x0C0  x24 / x25     0x0D0  x26 / x27
*   0x0E0  x28 / x29     0x0F0  x30 / SP_EL1(orig)
*   0x100  ELR_EL1 / SPSR_EL1
*
* Copyright (c) 2026 Maior Cristian
******************************************************************************/

/* ─── Constants ─────────────────────────────────────────────────────────── */

.set EXC_FRAME_SIZE, 0x110

/* Exception type IDs — passed in x0 to common_trap_handler()             */
/* Encoding: high nibble = group (0-3), low nibble = type (1=sync..4=serr) */
.set EXC_SP0_SYNC,  0x01
.set EXC_SP0_IRQ,   0x02
.set EXC_SP0_FIQ,   0x03
.set EXC_SP0_SERR,  0x04
.set EXC_SPX_SYNC,  0x11    /* data/instruction abort, SVC, BRK           */
.set EXC_SPX_IRQ,   0x12    /* UART0 RX (SPI #1) and Timer (PPI #30)      */
.set EXC_SPX_FIQ,   0x13    /* not used — GIC Group 0 not configured       */
.set EXC_SPX_SERR,  0x14    /* async bus errors                            */
.set EXC_A64_SYNC,  0x21    /* lower EL — no EL0 tasks yet                 */
.set EXC_A64_IRQ,   0x22
.set EXC_A64_FIQ,   0x23
.set EXC_A64_SERR,  0x24

/* ─── Macros ────────────────────────────────────────────────────────────── */

/*
 * save_regs <exc_id>
 * Allocates the exception frame and saves all 31 GPRs + ELR_EL1 + SPSR_EL1.
 * On exit:  x0 = exc_id   (arg 0 for common_trap_handler)
 *           x1 = sp       (arg 1 - pointer to saved frame)
 * Uses x10/x11 as scratch (saved before use, so safe).
 */
.macro save_regs exc_id
    sub  sp,  sp,  #EXC_FRAME_SIZE
    stp  x0,  x1,  [sp, #0x000]
    stp  x2,  x3,  [sp, #0x010]
    stp  x4,  x5,  [sp, #0x020]
    stp  x6,  x7,  [sp, #0x030]
    stp  x8,  x9,  [sp, #0x040]
    stp  x10, x11, [sp, #0x050]
    stp  x12, x13, [sp, #0x060]
    stp  x14, x15, [sp, #0x070]
    stp  x16, x17, [sp, #0x080]
    stp  x18, x19, [sp, #0x090]
    stp  x20, x21, [sp, #0x0A0]
    stp  x22, x23, [sp, #0x0B0]
    stp  x24, x25, [sp, #0x0C0]
    stp  x26, x27, [sp, #0x0D0]
    stp  x28, x29, [sp, #0x0E0]
    str  x30,      [sp, #0x0F0]
    add  x10, sp,  #EXC_FRAME_SIZE     // original SP before frame alloc
    str  x10,      [sp, #0x0F8]
    mrs  x10, elr_el1
    mrs  x11, spsr_el1
    stp  x10, x11, [sp, #0x100]
    mov  x0,  #\exc_id                  // arg0: exception type
    mov  x1,  sp                        // arg1: frame pointer
.endm

/*
 * restore_regs
 * Restores ELR_EL1, SPSR_EL1, all GPRs, deallocates frame, executes ERET.
 */
.macro restore_regs
    ldp  x10, x11, [sp, #0x100]
    msr  elr_el1,  x10
    msr  spsr_el1, x11
    ldr  x30,      [sp, #0x0F0]
    ldp  x28, x29, [sp, #0x0E0]
    ldp  x26, x27, [sp, #0x0D0]
    ldp  x24, x25, [sp, #0x0C0]
    ldp  x22, x23, [sp, #0x0B0]
    ldp  x20, x21, [sp, #0x0A0]
    ldp  x18, x19, [sp, #0x090]
    ldp  x16, x17, [sp, #0x080]
    ldp  x14, x15, [sp, #0x070]
    ldp  x12, x13, [sp, #0x060]
    ldp  x10, x11, [sp, #0x050]
    ldp  x8,  x9,  [sp, #0x040]
    ldp  x6,  x7,  [sp, #0x030]
    ldp  x4,  x5,  [sp, #0x020]
    ldp  x2,  x3,  [sp, #0x010]
    ldp  x0,  x1,  [sp, #0x000]
    add  sp,  sp,  #EXC_FRAME_SIZE
    eret
.endm

/* ═══════════════════════════════════════════════════════════════════════════
 * EXCEPTION VECTOR TABLE  (.text.vectors)
 * Each slot: one 'b' instruction (4 bytes). .align 7 pads to next 128-byte
 * boundary before each entry, satisfying the hardware constraint.
 * linkerqemu.ld places this at ALIGN(0x800) → VBAR_EL1 = 0x40100800
 * ═══════════════════════════════════════════════════════════════════════════ */

.section ".text.vectors", "ax"
.align 11               // 2^11 = 2048 — VBAR_EL1[10:0] must be zero
.global vectors
vectors:

    /* ── Group 0: Current EL with SP0 (0x000–0x17F) ── */
    .align 7
    b   _exc_sp0_sync               // 0x000
    .align 7
    b   _exc_sp0_irq                // 0x080
    .align 7
    b   _exc_sp0_fiq                // 0x100
    .align 7
    b   _exc_sp0_serr               // 0x180

    /* ── Group 1: Current EL with SPx (0x200–0x37F)  ← ACTIVE ── */
    .align 7
    b   _exc_spx_sync               // 0x200 — data abort, SVC, BRK
    .align 7
    b   _exc_spx_irq                // 0x280 — UART0 RX / Timer IRQ
    .align 7
    b   _exc_spx_fiq                // 0x300 — not used
    .align 7
    b   _exc_spx_serr               // 0x380 — async bus errors

    /* ── Group 2: Lower EL, AArch64 (0x400–0x57F) ── */
    .align 7
    b   _exc_a64_sync               // 0x400
    .align 7
    b   _exc_a64_irq                // 0x480
    .align 7
    b   _exc_a64_fiq                // 0x500
    .align 7
    b   _exc_a64_serr               // 0x580

    /* ── Group 3: Lower EL, AArch32 (0x600–0x77F) — NOT SUPPORTED ── */
    .align 7
    b   .                           // 0x600 — hang
    .align 7
    b   .                           // 0x680 — hang
    .align 7
    b   .                           // 0x700 — hang
    .align 7
    b   .                           // 0x780 — hang


/* ═══════════════════════════════════════════════════════════════════════════
 * EXCEPTION HANDLER STUBS  (.text — no 128-byte limit here)
 * Pattern: save_regs → call common_trap_handler(C) → restore_regs → ERET
 *
 * C signature:
 *   void common_trap_handler(uint64_t exc_id, exc_frame_t *frame);
 *   x0 = exc_id (EXC_SP0_SYNC..EXC_A64_SERR)
 *   x1 = pointer to saved exception frame on stack
 * ═══════════════════════════════════════════════════════════════════════════ */

.section ".text", "ax"

/* ── Group 0 stubs (SP0 — unexpected, but handled safely) ── */
.global _exc_sp0_sync
_exc_sp0_sync:
    save_regs EXC_SP0_SYNC
    bl   common_trap_handler
    restore_regs

.global _exc_sp0_irq
_exc_sp0_irq:
    save_regs EXC_SP0_IRQ
    bl   common_trap_handler
    restore_regs

.global _exc_sp0_fiq
_exc_sp0_fiq:
    save_regs EXC_SP0_FIQ
    bl   common_trap_handler
    restore_regs

.global _exc_sp0_serr
_exc_sp0_serr:
    save_regs EXC_SP0_SERR
    bl   common_trap_handler
    restore_regs

/* ── Group 1 stubs (SPx — ACTIVE GROUP, all cores) ── */
.global _exc_spx_sync
_exc_spx_sync:
    save_regs EXC_SPX_SYNC
    bl   common_trap_handler
    restore_regs

.global _exc_spx_irq
_exc_spx_irq:                       // UART0 RX and Timer IRQs arrive here
    save_regs EXC_SPX_IRQ
    bl   common_trap_handler
    restore_regs

.global _exc_spx_fiq
_exc_spx_fiq:
    save_regs EXC_SPX_FIQ
    bl   common_trap_handler
    restore_regs

.global _exc_spx_serr
_exc_spx_serr:
    save_regs EXC_SPX_SERR
    bl   common_trap_handler
    restore_regs

/* ── Group 2 stubs (Lower EL, AArch64 — no EL0 tasks yet) ── */
.global _exc_a64_sync
_exc_a64_sync:
    save_regs EXC_A64_SYNC
    bl   common_trap_handler
    restore_regs

.global _exc_a64_irq
_exc_a64_irq:
    save_regs EXC_A64_IRQ
    bl   common_trap_handler
    restore_regs

.global _exc_a64_fiq
_exc_a64_fiq:
    save_regs EXC_A64_FIQ
    bl   common_trap_handler
    restore_regs

.global _exc_a64_serr
_exc_a64_serr:
    save_regs EXC_A64_SERR
    bl   common_trap_handler
    restore_regs
