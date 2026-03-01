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

    .text : {
        KEEP(*(.text.boot))
        . = ALIGN(0x800);          /* VBAR_EL1[10:0] must be zero → 2 KB boundary */
        KEEP(*(.text.vectors))     /* IVT pinned at 0x40100800                     */
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
