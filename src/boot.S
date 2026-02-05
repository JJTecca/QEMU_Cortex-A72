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
    // Initialize .data section (Core 0 only)
    // Copy initialized data from load address to RAM
    ldr     x0, =__data_start       // Destination in RAM
    ldr     x1, =__data_load        // Source (load address)
    ldr     x2, =__data_end         // End of data section
    
    // Skip if .data section is empty or already in place
    cmp     x0, x1
    b.eq    skip_data_init
    cmp     x0, x2
    b.ge    skip_data_init
    
copy_data:
    cmp     x0, x2
    b.ge    data_done
    ldr     x3, [x1], #8            // Load from source, increment
    str     x3, [x0], #8            // Store to dest, increment
    b       copy_data
    
data_done:
skip_data_init:
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
