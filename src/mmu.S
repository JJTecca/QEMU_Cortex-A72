/******************************************************************************
 * File: src/mmu.S
 * Project: RPi5-Industrial-Gateway Bare Metal Development
 *
 * What this file does:
 *   Turns on the MMU so the CPU knows which addresses are hardware
 *   registers (UART, GIC) and which are normal RAM (our kernel).
 *
 * The table we build:
 *   0x00000000 - 0x3FFFFFFF  →  DEVICE  (GIC at 0x08000000, UART at 0x09000000)
 *   0x40000000 - 0x7FFFFFFF  →  NORMAL  (kernel code, stacks, ring buffers)
 *
 * Virtual address == Physical address (identity map). Nothing else changes.
 ******************************************************************************/


/* Where we put our page table in RAM (must be before kernel at 0x40100000) */
#define TTB_BASE     0x40080000

/* MAIR: tells the CPU our two memory types
 *   Attr0 = 0xFF = Normal RAM  → cache it, fast
 *   Attr1 = 0x00 = Device HW   → no cache, strict order  */
#define MAIR_VAL     0x00FF

/* TCR: tells the CPU "use 4KB pages, 32-bit address space" */
#define TCR_VAL      0x803520

/* SCTLR: the ON switch — bit0=MMU, bit2=data cache, bit12=instruction cache */
#define SCTLR_VAL    0x30C51835

/* ── Page table entry templates ──────────────────────────────────────────── */

/* DEVICE entry: use Attr1 (Device), no execute, strict ordering
 * Used for: GIC + UART (0x00000000 - 0x3FFFFFFF)              */
#define DEVICE_BLOCK  0x00000000000705

/* NORMAL entry: use Attr0 (Normal RAM), cacheable, executable
 * Used for: kernel code + stacks + data (0x40000000 - 0x7FFFFFFF) */
#define NORMAL_BLOCK  0x00000000000701

/* ── Shared variables (in RAM, all cores see these) ──────────────────────── */

    .section ".data.mmu"
    .balign 4

mmu_lock:
    .4byte 0        /* 0 = nobody is inside, 1 = one core is working */

mmu_init:
    .4byte 0        /* 0 = table not built yet,  1 = table is ready   */

/* ── mmu_on ───────────────────────────────────────────────────────────────── */

    .section ".text.mmu_on", "ax"
    .balign 4
    .global mmu_on
    .type   mmu_on, %function

/*
 * mmu_on — called by all 4 cores from boot.S before "bl main".
 *
 * The table only needs to be built ONCE, so we use a spinlock:
 *   - Core that wins the lock  → builds the table, then programs its own registers
 *   - Cores that lose the lock → wait, then just program their own registers
 *   (TTBR0, MAIR, TCR, SCTLR are separate inside each core — every core must set them)
 *
 * Clobbers: x0–x4
 */
mmu_on:

    /* ── 1. Lock: make sure only one core enters the setup section ────── */
    ADRP    x0, mmu_lock
    ADD     x0, x0, :lo12:mmu_lock   /* x0 = address of the lock variable  */
    MOV     w1, #1                    /* the value meaning "locked"          */
    SEVL                              /* so the first WFE doesn't sleep      */
1:
    WFE                               /* sleep until another core does SEV   */
    LDAXR   w2, [x0]                 /* read the lock atomically            */
    CBNZ    w2, 1b                    /* if already locked → sleep and retry */
    STXR    w3, w1, [x0]             /* try to grab the lock                */
    CBNZ    w3, 1b                    /* if someone else grabbed it → retry  */

    /* ── 2. Check: did a previous core already build the table? ─────────  */
check_init:
    ADRP    x1, mmu_init
    ADD     x1, x1, :lo12:mmu_init
    LDR     w2, [x1]
    CBNZ    w2, load_regs            /* table already built → skip to step 6 */

    /* ── 3. Zero the page table (wipe any old garbage from RAM) ─────────  */
zero_table:
    LDR     x2, =TTB_BASE            /* start of our table                  */
    MOV     x3, #512                 /* 512 entries (one 4KB table)         */
    MOV     x4, #0
2:
    STR     x4, [x2], #8            /* write 0, move to next entry         */
    SUBS    x3, x3, #1
    B.NE    2b

    /* ── 4. Fill entry [0]: DEVICE — covers GIC + UART ──────────────────  */
    LDR     x2, =TTB_BASE
    LDR     x3, =DEVICE_BLOCK        /* template with Device attributes     */
                                     /* PA 0x00000000 is already in template */
    STR     x3, [x2, #0]            /* table[0] = Device 1GB block         */

    /* ── 5. Fill entry [1]: NORMAL — covers all kernel RAM ──────────────  */
    LDR     x3, =NORMAL_BLOCK        /* template with Normal attributes     */
    LDR     x4, =0x40000000          /* physical address of our RAM         */
    ORR     x3, x3, x4               /* combine address + attributes        */
    STR     x3, [x2, #8]            /* table[1] = Normal 1GB block         */

    /* Mark the table as built so other cores skip steps 3-5 */
    MOV     w2, #1
    STR     w2, [x1]

    /* ── 6. Every core writes its own CPU registers ──────────────────────  */
load_regs:
    LDR     x1, =TTB_BASE
    MSR     ttbr0_el1, x1            /* "CPU, find the table here"          */

    LDR     x1, =MAIR_VAL
    MSR     mair_el1, x1             /* "CPU, here are the 2 memory types"  */

    LDR     x1, =TCR_VAL
    MSR     tcr_el1, x1              /* "CPU, use 4KB pages, 32-bit space"  */
    ISB

    /* Safety: read TCR back and confirm CPU accepted it */
    MRS     x2, tcr_el1
    LDR     x3, =TCR_VAL
    CMP     x2, x3
    B.NE    mmu_hang                 /* mismatch = something is very wrong  */

    /* Flush any old TLB entries before switching MMU on */
    TLBI    VMALLE1IS
    DSB     ISH
    ISB

    LDR     x1, =SCTLR_VAL
    MSR     sctlr_el1, x1            /* "CPU, MMU ON. Use the table."       */
    ISB

    /* ── 7. Unlock so the next core can proceed ──────────────────────────  */
    ADRP    x0, mmu_lock
    ADD     x0, x0, :lo12:mmu_lock
    STLR    wzr, [x0]                /* release the lock                    */
    RET                              /* back to boot.S → bl main            */

/* If TCR check failed, hang here forever */
mmu_hang:
    WFE
    B       mmu_hang
