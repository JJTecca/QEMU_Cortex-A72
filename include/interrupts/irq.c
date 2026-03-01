/******************************************************************************
 * File: irq.c
 * Description: Interrupt Request Controller
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/
#include "interrupts/irq.h"

#define GICD_BASE  0x08000000UL
#define GICC_BASE  0x08010000UL

#define GICD_CTLR          (*(volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_ISENABLER(n)  (*(volatile uint32_t *)(GICD_BASE + 0x100 + (n)*4))
#define GICD_ICENABLER(n)  (*(volatile uint32_t *)(GICD_BASE + 0x180 + (n)*4))
#define GICD_IPRIORITYR(n) (*(volatile uint32_t *)(GICD_BASE + 0x400 + (n)*4))
#define GICD_ITARGETSR(n)  (*(volatile uint32_t *)(GICD_BASE + 0x800 + (n)*4))
#define GICD_ICFGR(n)      (*(volatile uint32_t *)(GICD_BASE + 0xC00 + (n)*4))

#define GICC_CTLR  (*(volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR   (*(volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_BPR   (*(volatile uint32_t *)(GICC_BASE + 0x008))
#define GICC_IAR   (*(volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR  (*(volatile uint32_t *)(GICC_BASE + 0x010))

static irq_handler_t irq_table[IRQ_MAX_HANDLERS];

void irq_init(void)
{
    uint32_t i;
    GICD_CTLR = 0;                              /* disable distributor    */
    for (i = 1; i < 8; i++)
        GICD_ICENABLER(i) = 0xFFFFFFFFu;        /* mask all SPIs          */
    for (i = 0; i < 64; i++)
        GICD_IPRIORITYR(i) = 0xA0A0A0A0u;      /* medium priority        */
    for (i = 8; i < 32; i++)
        GICD_ITARGETSR(i) = 0x01010101u;        /* route all SPIs → CPU0  */
    for (i = 2; i < 8; i++)
        GICD_ICFGR(i) = 0x00000000u;            /* level-sensitive        */
    GICD_CTLR = 1u;                             /* enable distributor     */
    GICC_PMR  = 0xFFu;                          /* pass all priorities    */
    GICC_BPR  = 0x00u;
    GICC_CTLR = 1u;                             /* enable CPU interface   */
}

void irq_register_handler(uint32_t irq_id, irq_handler_t handler)
{
    if (irq_id >= IRQ_MAX_HANDLERS) return;
    irq_table[irq_id] = handler;
    GICD_ISENABLER(irq_id / 32u) = (1u << (irq_id % 32u));
}

void irq_enable(void)  { __asm__ volatile("msr daifclr, #2" ::: "memory"); }
void irq_disable(void) { __asm__ volatile("msr daifset, #2" ::: "memory"); }

void common_trap_handler(uint64_t exc_id, exc_frame_t *frame)
{
    (void)frame;
    switch (exc_id) {
    case EXC_SPX_IRQ:
    case EXC_A64_IRQ: {
        uint32_t iar    = GICC_IAR;
        uint32_t irq_id = iar & 0x3FFu;          /* INTID in bits [9:0]    */
        if (irq_id == 1023u) break;               /* spurious — ignore      */
        if (irq_id < IRQ_MAX_HANDLERS && irq_table[irq_id])
            irq_table[irq_id](irq_id);
        GICC_EOIR = iar;                          /* end-of-interrupt       */
        break;
    }
    default:
        for (;;) __asm__ volatile("wfe");         /* fatal — halt core      */
    }
}
