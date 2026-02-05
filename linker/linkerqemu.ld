ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40100000, LENGTH = 512M
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
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM
    
    /* For RAM-only systems, load address = runtime address */
    __data_load = LOADADDR(.data);
    
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM
}
