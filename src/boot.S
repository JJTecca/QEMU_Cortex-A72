.section ".text.boot"
.global _start

_start:
    // Read CPU ID
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    
    // Set stack - simple calculation
    // Base stack at 0x40200000 (way above code)
    // Each core gets 16KB (0x4000)
    mov     x1, #0x40200000
    add     x0, x0, #1              // core_id + 1
    mov     x2, #0x4000
    mul     x2, x0, x2
    add     x1, x1, x2
    mov     sp, x1
    
    // Get CPU ID again for branching
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    
    // Core 0 goes to main, others go to secondary_main
    cbnz    x0, secondary_cpu
    
primary_cpu:
    // Clear BSS (Core 0 only)
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    
clear_bss:
    cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       clear_bss
    
bss_done:
    bl      main
    b       hang

secondary_cpu:
    bl      secondary_main
    
hang:
    wfe
    b       hang
