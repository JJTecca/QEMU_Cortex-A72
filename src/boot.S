.section ".text.boot"
.global _start

_start:
    // Read CPU ID (MPIDR_EL1)
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF      // Extract CPU ID
    cbz     x0, primary_cpu    // CPU 0 continues, others spin
    
secondary_spin:
    wfe                        // Wait for event
    b       secondary_spin

primary_cpu:
    // Set stack pointer (core 0)
    ldr     x0, =_stack_top
    mov     sp, x0
    
    // Clear BSS section
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    sub     x1, x1, x0         // BSS size
    cbz     x1, bss_done
    
clear_bss:
    str     xzr, [x0], #8
    subs    x1, x1, #8
    b.gt    clear_bss
    
bss_done:
    // Jump to C main()
    bl      main
    
    // Hang if main returns
hang:
    wfe
    b hang
