ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000, LENGTH = 1024M
}

SECTIONS
{
    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    } > RAM
    
    .rodata : {
        *(.rodata*)
    } > RAM
    
    .data : {
        *(.data*)
    } > RAM
    
    __bss_start = .;
    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM
    __bss_end = .;
    
    . = ALIGN(16);
    _stack_top = . + 0x4000;  /* 16KB stack */
}
