/******************************************************************************
 * File: irq.h
 * GIC-400 / GICv2 base addresses (QEMU virt):
 *   GICD (Distributor)   : 0x08000000
 *   GICC (CPU Interface) : 0x08010000
 * Copyright (c) 2026 Maior Cristian
 ******************************************************************************/
#pragma once
#include <stdint.h>

/* Exception frame — mirrors save_regs in vector.S (EXC_FRAME_SIZE = 0x110) */
typedef struct {
    uint64_t x[31];      /* x0–x30  offsets 0x000–0x0F0  */
    uint64_t sp_el1;     /* orig SP  offset 0x0F8         */
    uint64_t elr_el1;    /* ELR_EL1  offset 0x100         */
    uint64_t spsr_el1;   /* SPSR_EL1 offset 0x108         */
} exc_frame_t;

/* Exception type IDs (mirror .set values in vector.S) */
#define EXC_SP0_SYNC   0x01u
#define EXC_SP0_IRQ    0x02u
#define EXC_SP0_FIQ    0x03u
#define EXC_SP0_SERR   0x04u
#define EXC_SPX_SYNC   0x11u
#define EXC_SPX_IRQ    0x12u
#define EXC_SPX_FIQ    0x13u
#define EXC_SPX_SERR   0x14u
#define EXC_A64_SYNC   0x21u
#define EXC_A64_IRQ    0x22u
#define EXC_A64_FIQ    0x23u
#define EXC_A64_SERR   0x24u

/* GIC INTID constants for this project */
#define IRQ_ID_TIMER   30u   /* ARM generic timer PPI → INTID 30 */
#define IRQ_ID_UART0   33u   /* PL011 UART0 SPI #1   → INTID 33 */

#define IRQ_MAX_HANDLERS 64u
typedef void (*irq_handler_t)(uint32_t irq_id);

void irq_init(void);
void irq_register_handler(uint32_t irq_id, irq_handler_t handler);
void irq_enable(void);
void irq_disable(void);
void common_trap_handler(uint64_t exc_id, exc_frame_t *frame);
