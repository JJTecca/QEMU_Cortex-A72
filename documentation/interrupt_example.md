# Interrupt Lifecycle — Full Timer IRQ Walkthrough
> **Target hardware:** QEMU `virt` (Cortex-A72) / Raspberry Pi 5 (Cortex-A76)  
> **Architecture:** AArch64, EL1 with SPx (SP_EL1 active on all cores)  
> **Practical example:** ARM Generic Timer PPI — INTID 30 fires while Core 0 sleeps in `wfe`  
> **Files involved:** `src/boot.S` · `src/vector.S` · `include/interrupts/irq.c` · `include/interrupts/irq.h` · `src/main.c`

---

## Table of Contents

1. [Key Registers Reference](#1-key-registers-reference)
2. [IVT Structure Explained](#2-ivt-structure-explained)
3. [Stage 0 — Boot: One-Time Hardware Configuration](#3-stage-0--boot-one-time-hardware-configuration)
4. [Stage 1 — Setup: GIC Distributor Programming](#4-stage-1--setup-gic-distributor-programming-irq_init)
5. [Stage 2 — Setup: Handler Registration + CPU Unmask](#5-stage-2--setup-handler-registration--cpu-unmask)
6. [Stage 3 — Hardware: Timer Fires](#6-stage-3--hardware-timer-fires-zero-code-executed-by-you)
7. [Stage 4 — IVT Slot Execution](#7-stage-4--ivt-slot-execution)
8. [Stage 5 — save_regs: Context Save](#8-stage-5--save_regs-context-save)
9. [Stage 6 — common_trap_handler: GIC Dispatch](#9-stage-6--common_trap_handler-gic-dispatch)
10. [Stage 7 — restore_regs + eret: Return](#10-stage-7--restore_regs--eret-return)
11. [Final State](#11-final-state--fully-transparent-to-main)
12. [Complete Flow Diagram](#12-complete-flow-diagram)

---

## 1. Key Registers Reference

| Register | Type | Who Writes It | Purpose |
|---|---|---|---|
| `MPIDR_EL1` | CPU system reg (RO) | Hardware | Identifies which core is running |
| `VBAR_EL1` | CPU system reg | `boot.S` via `msr` | Base address of the IVT (2KB aligned) |
| `DAIF` | CPU system reg | `irq_enable/disable` via `msr daifclr/set` | CPU-level IRQ mask gate |
| `ELR_EL1` | CPU system reg | Hardware on exception entry | Return PC — where `eret` jumps back to |
| `SPSR_EL1` | CPU system reg | Hardware on exception entry | Saved PSTATE snapshot, restored by `eret` |
| `SP_EL1` | CPU system reg | `boot.S` + `save_regs` macro | Exception stack pointer |
| `GICD_CTLR` | GIC memory-mapped `0x08000000` | `irq_init()` | Enable/disable GIC distributor |
| `GICD_ISENABLER(n)` | GIC memory-mapped `0x08000100+n*4` | `irq_register_handler()` | Unmask specific INTID in distributor |
| `GICD_ICENABLER(n)` | GIC memory-mapped `0x08000180+n*4` | `irq_init()` | Mask specific INTID in distributor |
| `GICD_IPRIORITYR(n)` | GIC memory-mapped `0x08000400+n*4` | `irq_init()` | Set interrupt priority (lower = higher prio) |
| `GICD_ITARGETSR(n)` | GIC memory-mapped `0x08000800+n*4` | `irq_init()` | Route INTID to specific CPU core |
| `GICD_ICFGR(n)` | GIC memory-mapped `0x08000C00+n*4` | `irq_init()` | Level-sensitive vs edge-triggered |
| `GICC_PMR` | GIC memory-mapped `0x08010004` | `irq_init()` | Priority filter threshold |
| `GICC_BPR` | GIC memory-mapped `0x08010008` | `irq_init()` | Binary point / priority grouping |
| `GICC_CTLR` | GIC memory-mapped `0x08010000` | `irq_init()` | Enable GIC CPU interface |
| `GICC_IAR` | GIC memory-mapped `0x0801000C` | `common_trap_handler` (read) | ACK interrupt + get INTID (destructive read) |
| `GICC_EOIR` | GIC memory-mapped `0x08010010` | `common_trap_handler` (write) | Signal End-of-Interrupt to GIC |

> **CPU system registers** are accessed with `mrs` (read) and `msr` (write) instructions.  
> **GIC registers** are memory-mapped — accessed with regular load/store to their physical address.

---

## 2. IVT Structure Explained

The AArch64 IVT is a **2048-byte block of executable code** placed at a 2KB-aligned address.  
The CPU does not follow a pointer — it **computes the jump target arithmetically**:

```
Jump target = VBAR_EL1 + group_offset + type_offset
```

### Two Axes: Group × Type

**Group** (which EL/SP was active when the exception fired):

| Group | Offset Base | Condition |
|---|---|---|
| 0 — SP0 | `+0x000` | Current EL, SP_EL0 active |
| 1 — SPx | `+0x200` | Current EL, SP_EL1 active ← **YOUR ACTIVE GROUP** |
| 2 — A64 | `+0x400` | Lower EL (EL0), AArch64 |
| 3 — A32 | `+0x600` | Lower EL, AArch32 — not supported (hang) |

**Type** (what kind of exception):

| Type | Sub-offset | Meaning |
|---|---|---|
| Sync | `+0x000` | Instruction-caused: SVC, data abort, BRK |
| IRQ | `+0x080` | Normal interrupt from GIC |
| FIQ | `+0x100` | Fast interrupt (GIC Group 0, unused here) |
| SError | `+0x180` | Async bus error |

### IVT Memory Map (VBAR_EL1 = `0x40100800`)

```
Address         Slot                    Handler Target
──────────────────────────────────────────────────────────────────────
0x40100800   b _exc_sp0_sync       → save_regs(0x01) + C dispatch
0x40100880   b _exc_sp0_irq        → save_regs(0x02) + C dispatch
0x40100900   b _exc_sp0_fiq        → save_regs(0x03) + C dispatch
0x40100980   b _exc_sp0_serr       → save_regs(0x04) + C dispatch

0x40100A00   b _exc_spx_sync       → save_regs(0x11) + C dispatch
0x40100A80   b _exc_spx_irq   ◄─── UART0 RX / Timer land HERE
0x40100B00   b _exc_spx_fiq        → save_regs(0x13) + C dispatch
0x40100B80   b _exc_spx_serr       → save_regs(0x14) + C dispatch

0x40100C00   b _exc_a64_sync       → save_regs(0x21) + C dispatch
0x40100C80   b _exc_a64_irq        → save_regs(0x22) + C dispatch
0x40100D00   b _exc_a64_fiq        → save_regs(0x23) + C dispatch
0x40100D80   b _exc_a64_serr       → save_regs(0x24) + C dispatch

0x40100E00   b .  (hang)           ← AArch32 not supported
0x40100E80   b .  (hang)
0x40100F00   b .  (hang)
0x40100F80   b .  (hang)
──────────────────────────────────────────────────────────────────────
0x40101000   ← end of IVT (exactly 0x800 = 2048 bytes)
```

Each slot is **128 bytes** (`.align 7` = 2^7). Only the first 4 bytes contain a `b` instruction; the remaining 124 bytes are NOP padding. This is why each entry in `vector.S` is a single branch — the real handler code lives in `.text` with no size limit.

> **`.align 11` at the table top** forces `VBAR_EL1[10:0] = 0` — a hardware requirement. Violating this silently corrupts every exception dispatch.

---

## 3. Stage 0 — Boot: One-Time Hardware Configuration

**File:** `src/boot.S`

| Step | Code | Action | Register | Before | After |
|---|---|---|---|---|---|
| Read core ID | `mrs x0, mpidr_el1` | Identify which core is running | `MPIDR_EL1` | HW value | `0x80000000` (Core 0) |
| Set stack | `mov sp, x1` | Each core gets 16KB (`0x4000`) | `SP_EL1` | undefined | `0x40204000` (Core 0) |
| Install IVT | `msr vbar_el1, x0` | Point CPU at vector table | `VBAR_EL1` | undefined | `0x40100800` |
| Flush pipeline | `isb` | Guarantee `VBAR_EL1` is live before any exception | CPU pipeline | stale | flushed |
| IRQs still masked | reset default | Not yet unmasked | `DAIF.I` | `1` (masked) | `1` (masked) |

**Stack layout for all 4 cores:**
```
Core 0:  SP_EL1 = 0x40200000 + (0+1)*0x4000 = 0x40204000
Core 1:  SP_EL1 = 0x40200000 + (1+1)*0x4000 = 0x40208000
Core 2:  SP_EL1 = 0x40200000 + (2+1)*0x4000 = 0x4020C000
Core 3:  SP_EL1 = 0x40200000 + (3+1)*0x4000 = 0x40210000
```

---

## 4. Stage 1 — Setup: GIC Distributor Programming (`irq_init`)

**File:** `include/interrupts/irq.c`  
**GIC-400 base addresses (QEMU virt):** GICD = `0x08000000`, GICC = `0x08010000`

| Step | Code | Action | Register / Address | Value Written |
|---|---|---|---|---|
| Disable distributor | `GICD_CTLR = 0` | Safe to configure GIC | `GICD_CTLR` `0x08000000` | `0` |
| Mask all SPIs | `GICD_ICENABLER(1..7)` | Block all INTIDs 32–255 | `0x08000184..019C` | `0xFFFFFFFF` |
| Set medium priority | `GICD_IPRIORITYR(0..63)` | Priority `0xA0` on all INTIDs | `0x08000400..04FC` | `0xA0A0A0A0` |
| Route SPIs → CPU0 | `GICD_ITARGETSR(8..31)` | Deliver all SPIs to Core 0 | `0x08000820..087C` | `0x01010101` |
| Level-sensitive | `GICD_ICFGR(2..7)` | Not edge-triggered | `0x08000C08..0C1C` | `0x00000000` |
| Re-enable distributor | `GICD_CTLR = 1` | GIC starts forwarding | `GICD_CTLR` `0x08000000` | `1` |
| Priority mask open | `GICC_PMR = 0xFF` | All priorities pass (`0xA0 < 0xFF` ✅) | `GICC_PMR` `0x08010004` | `0xFF` |
| No priority grouping | `GICC_BPR = 0x00` | Single priority group | `GICC_BPR` `0x08010008` | `0x00` |
| Enable CPU interface | `GICC_CTLR = 1` | GIC can now assert IRQ to CPU | `GICC_CTLR` `0x08010000` | `1` |

---

## 5. Stage 2 — Setup: Handler Registration + CPU Unmask

**File:** `src/main.c` + `include/interrupts/irq.c`

| Step | Code | Action | Register / Variable | Before | After |
|---|---|---|---|---|---|
| Store handler pointer | `irq_register_handler(30, timer_irq_handler)` | Map INTID 30 → C function | `irq_table[30]` | `NULL` | `timer_irq_handler` address |
| Unmask INTID 30 | `GICD_ISENABLER(0) = (1 << 30)` | Allow timer through distributor | `GICD_ISENABLER(0)` `0x08000100` bit 30 | `0` | `1` → `0x40000000` |
| Unmask IRQs at CPU | `msr daifclr, #2` | Clear `DAIF.I` — open CPU gate | `DAIF.I` | `1` (masked) | `0` (unmasked) |
| Core 0 sleeps | `wfe` | Waiting for events | `PC` | previous | `0x40102A40` (wfe addr) |

**INTID reference (from `irq.h`):**
```c
#define IRQ_ID_TIMER   30u   // ARM Generic Timer PPI → INTID 30
#define IRQ_ID_UART0   33u   // PL011 UART0 SPI #1   → INTID 33
```

---

## 6. Stage 3 — Hardware: Timer Fires (Zero Code Executed By You)

The ARM Generic Timer fires. The GIC distributor checks:
- INTID 30 priority = `0xA0` < `GICC_PMR` `0xFF` ✅
- INTID 30 enabled in `GICD_ISENABLER0` bit 30 ✅
- Routed to CPU0 via `GICD_ITARGETSR` ✅

GIC asserts the physical IRQ line to the Cortex-A76 core. **Hardware takes over automatically:**

| Step | Who | Action | Register | Before | After |
|---|---|---|---|---|---|
| Timer asserts IRQ | ARM Generic Timer HW | INTID 30 goes pending | GIC internal | idle | pending |
| GIC checks priority | GIC HW | `0xA0 < 0xFF` → pass | `GICC_PMR` | `0xFF` | `0xFF` (unchanged) |
| GIC asserts IRQ line | GIC HW | Physical IRQ signal to Cortex-A76 | IRQ line | low | high |
| Save current PC | CPU HW automatic | Capture interrupted instruction | `ELR_EL1` | stale | `0x40102A40` (the `wfe`) |
| Save current PSTATE | CPU HW automatic | Full CPU state snapshot | `SPSR_EL1` | stale | `0x60000005` |
| Mask further IRQs | CPU HW automatic | Prevent nested IRQ | `DAIF.I` | `0` | `1` |
| Compute IVT target | CPU HW automatic | VBAR + group + type offset | `PC` | `0x40102A40` | `0x40100A80` |

**IVT offset formula:**
```
PC ← VBAR_EL1  +  0x200        +  0x080
   = 0x40100800 + (Group 1: SPx) + (Type: IRQ)
   = 0x40100A80
```

**Register state after hardware takeover:**
```
PC        = 0x40100A80   ← inside IVT slot
ELR_EL1   = 0x40102A40   ← saved return address (the wfe)
SPSR_EL1  = 0x60000005   ← saved PSTATE (encodes EL1, SP_EL1, DAIF.I=0)
PSTATE.I  = 1            ← IRQs now masked at CPU level
SP_EL1    = 0x40204000   ← unchanged
```

---

## 7. Stage 4 — IVT Slot Execution

**File:** `src/vector.S` — section `.text.vectors`

| Step | Code | Action | Register | Value |
|---|---|---|---|---|
| Land at IVT slot | HW jumped here | CPU executing at `0x40100A80` | `PC` | `0x40100A80` |
| Execute branch | `b _exc_spx_irq` (4 bytes) | Redirect to real stub in `.text` | `PC` | → `_exc_spx_irq` |
| 124 bytes padding | `.align 7` NOP fill | Enforces 128-byte slot boundary | — | unused |

The IVT slot does exactly one thing — redirect. All real work happens in `.text`.

---

## 8. Stage 5 — `save_regs`: Context Save

**File:** `src/vector.S` — macro `save_regs EXC_SPX_IRQ` expands at `_exc_spx_irq`

| Step | Instruction | Action | Register / Memory | Value |
|---|---|---|---|---|
| Allocate frame | `sub sp, sp, #0x110` | 272 bytes on stack | `SP_EL1` | `0x40204000` → `0x40203EF0` |
| Save x0–x29 | `stp xN, xN+1, [sp, #off]` | All GPRs to frame | `[0x40203EF0..EE0]` | x0–x29 values |
| Save x30 (LR) | `str x30, [sp, #0x0F0]` | Link register saved | `[0x40203FE0]` | x30 value |
| Save original SP | `add x10,sp,#0x110` + `str` | Compute & store pre-frame SP | `[0x40203FF8]` | `0x40204000` |
| Read ELR_EL1 | `mrs x10, elr_el1` | Hardware-saved return PC | `x10` | `0x40102A40` |
| Read SPSR_EL1 | `mrs x11, spsr_el1` | Hardware-saved PSTATE | `x11` | `0x60000005` |
| Write to frame | `stp x10, x11, [sp, #0x100]` | Store ELR + SPSR to frame top | `[0x40204000]` | ELR \| SPSR |
| Set C arg0 | `mov x0, #0x12` | `exc_id = EXC_SPX_IRQ` | `x0` | `0x12` |
| Set C arg1 | `mov x1, sp` | Frame pointer for C | `x1` | `0x40203EF0` |

**Exception frame layout on stack after `save_regs` (base = `SP_EL1` = `0x40203EF0`):**

```
Offset   Contents
0x000    x0  / x1
0x010    x2  / x3
0x020    x4  / x5
0x030    x6  / x7
0x040    x8  / x9
0x050    x10 / x11
0x060    x12 / x13
0x070    x14 / x15
0x080    x16 / x17
0x090    x18 / x19
0x0A0    x20 / x21
0x0B0    x22 / x23
0x0C0    x24 / x25
0x0D0    x26 / x27
0x0E0    x28 / x29
0x0F0    x30 / SP_EL1 (original = 0x40204000)
0x100    ELR_EL1 (0x40102A40) / SPSR_EL1 (0x60000005)
```
Total: `0x110` = 272 bytes — matches `EXC_FRAME_SIZE` and `exc_frame_t` in `irq.h`.

---

## 9. Stage 6 — `common_trap_handler`: GIC Dispatch

**File:** `include/interrupts/irq.c`  
**Called with:** `x0 = 0x12` (`EXC_SPX_IRQ`), `x1 = 0x40203EF0` (frame pointer)

| Step | Code | Action | Register / Address | Before | After |
|---|---|---|---|---|---|
| Switch on exc_id | `switch(0x12)` | Match `EXC_SPX_IRQ` case | — | — | matched |
| ACK interrupt | `GICC_IAR` read at `0x0801000C` | Get INTID + ACK in one destructive read | `GICC_IAR` | `0x0000001E` | consumed |
| Extract INTID | `iar & 0x3FFu` | Isolate bits[9:0] | `irq_id` | — | `30` |
| GIC state change | HW side-effect of IAR read | INTID 30 transitions state | GIC internal | pending | **active** |
| Spurious check | `irq_id == 1023?` | `30 ≠ 1023`, not spurious | — | — | continue |
| Dispatch handler | `irq_table[30](30)` | Call registered C function | `irq_table[30]` | fn pointer | **called** |
| Run timer handler | `timer_irq_handler(30)` | Your application code executes | — | — | returns |
| End-of-Interrupt | `GICC_EOIR = iar` write at `0x08010010` | Release GIC, allow next IRQ | GIC internal | active | **idle** |

> ⚠️ **Critical:** `GICC_IAR` is a **destructive read** — it simultaneously acknowledges the interrupt AND returns the INTID. You must save the full `iar` value (not just `irq_id`) and write it back to `GICC_EOIR`. Forgetting `GICC_EOIR` leaves INTID 30 stuck in `active` state — no further interrupts of that priority will ever arrive.

---

## 10. Stage 7 — `restore_regs` + `eret`: Return

**File:** `src/vector.S` — macro `restore_regs`

| Step | Instruction | Action | Register | Before | After |
|---|---|---|---|---|---|
| Reload ELR_EL1 | `ldp x10,x11,[sp,#0x100]` + `msr elr_el1, x10` | Restore return PC | `ELR_EL1` | handler value | `0x40102A40` |
| Reload SPSR_EL1 | `msr spsr_el1, x11` | Restore PSTATE snapshot | `SPSR_EL1` | handler value | `0x60000005` |
| Restore x30–x0 | `ldp/ldr` in reverse order | All GPRs back to original values | `x0–x30` | handler values | **original values** |
| Deallocate frame | `add sp, sp, #0x110` | Release 272-byte stack frame | `SP_EL1` | `0x40203EF0` | `0x40204000` |
| Atomic return | `eret` | Restore PSTATE + jump atomically | `PSTATE` ← `SPSR_EL1` | handler PSTATE | `0x60000005` |
| Re-enable IRQs | `eret` side-effect | `DAIF.I` restored from `SPSR_EL1` | `DAIF.I` | `1` (masked) | `0` (unmasked) |
| Resume execution | `eret` side-effect | Jump back to interrupted code | `PC` ← `ELR_EL1` | stub area | `0x40102A40` (wfe) |

> **`eret` is atomic** — it restores `PSTATE` from `SPSR_EL1` AND jumps to `ELR_EL1` in a single operation. There is no window between re-enabling IRQs and resuming the interrupted code.

---

## 11. Final State — Fully Transparent to `main()`

| Register | Value | Meaning |
|---|---|---|
| `PC` | `0x40102A40` | Back at `wfe`, Core 0 sleeps again |
| `SP_EL1` | `0x40204000` | Stack fully restored, clean |
| `DAIF.I` | `0` | IRQs unmasked, ready for next interrupt |
| `ELR_EL1` | `0x40102A40` | Stale but irrelevant until next exception |
| `SPSR_EL1` | `0x60000005` | Stale but irrelevant until next exception |
| `x0–x30` | original values | Fully restored — C code sees nothing changed |
| GIC INTID 30 | idle | Ready to fire again on next timer tick |

---

## 12. Complete Flow Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  STAGE 0 — boot.S                                                │
│  MPIDR_EL1 → core ID                                             │
│  SP_EL1    = 0x40204000  (Core 0)                                │
│  VBAR_EL1  = 0x40100800  + isb                                   │
│  DAIF.I    = 1  (still masked)                                   │
└───────────────────────────┬──────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────────────┐
│  STAGE 1+2 — irq_init() + irq_register_handler() + irq_enable() │
│  GICD programmed, INTID 30 unmasked                              │
│  irq_table = timer_irq_handler                               │
│  DAIF.I = 0  (unmasked)                                          │
│  Core 0 → wfe at PC=0x40102A40                                   │
└───────────────────────────┬──────────────────────────────────────┘
                            │
                            │  ← ARM Generic Timer fires
┌───────────────────────────▼──────────────────────────────────────┐
│  STAGE 3 — HARDWARE (automatic, no code from you)                │
│  ELR_EL1  ← 0x40102A40  (the wfe)                                │
│  SPSR_EL1 ← 0x60000005  (PSTATE snapshot)                        │
│  DAIF.I   ← 1           (mask nested IRQs)                       │
│  PC       ← 0x40100800 + 0x200 + 0x080 = 0x40100A80             │
└───────────────────────────┬──────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────────────┐
│  STAGE 4 — IVT slot at 0x40100A80  (vector.S .text.vectors)      │
│  b _exc_spx_irq  ← 1 instruction, 4 bytes                        │
└───────────────────────────┬──────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────────────┐
│  STAGE 5 — save_regs(0x12)  (vector.S .text)                     │
│  SP_EL1  ← 0x40203EF0  (272-byte frame allocated)                │
│  x0–x30 + orig_SP + ELR_EL1 + SPSR_EL1 → stack frame            │
│  x0 = 0x12 (EXC_SPX_IRQ),  x1 = 0x40203EF0 (frame ptr)          │
└───────────────────────────┬──────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────────────┐
│  STAGE 6 — common_trap_handler()  (irq.c)                        │
│  GICC_IAR  read  → iar=0x1E, irq_id=30  (ACK + get INTID)        │
│  GIC: INTID 30  pending → active                                  │
│  irq_table(30)  → timer_irq_handler() called                 │
│  GICC_EOIR write ← 0x1E  (End-of-Interrupt)                      │
│  GIC: INTID 30  active → idle                                     │
└───────────────────────────┬──────────────────────────────────────┘
                            │
┌───────────────────────────▼──────────────────────────────────────┐
│  STAGE 7 — restore_regs + eret  (vector.S .text)                  │
│  ELR_EL1  ← 0x40102A40  (from frame)                             │
│  SPSR_EL1 ← 0x60000005  (from frame)                             │
│  x0–x30  restored from frame                                      │
│  SP_EL1  ← 0x40204000  (frame deallocated)                       │
│  eret → PSTATE ← SPSR_EL1 (DAIF.I=0 re-enabled)                 │
│       → PC     ← ELR_EL1  (back to wfe)                          │
└───────────────────────────┬──────────────────────────────────────┘
                            │
                            ▼
              Core 0 resumes wfe — completely transparent
```

---

*Copyright (c) 2026 Maior Cristian — QEMU_Cortex-A72 / RPi5-Industrial-Gateway*
