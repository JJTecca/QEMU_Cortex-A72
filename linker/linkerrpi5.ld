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
    /* Reserve 64KB for stacks (16KB per core x 4 cores) */
    _stack_top = .;
    . = . + 0x10000;
}
