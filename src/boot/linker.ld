/* ParadoxOS Linker Script */

ENTRY(start)

SECTIONS
{
    /* Kernel starts at 1MB (standard for x86) */
    . = 1M;

    /* Multiboot header MUST be in first 8KB and at the beginning */
    .boot : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    /* Read-execute: code */
    .text ALIGN(4K) : AT(ADDR(.text))
    {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data ALIGN(4K) : AT(ADDR(.data))
    {
        *(.data)
        *(.data.*)
    }

    /* Uninitialized data */
    .bss ALIGN(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Stack */
    .stack ALIGN(4K) : AT(ADDR(.stack))
    {
        *(.stack)
    }

    /* Kernel end marker */
    _kernel_end = .;
    kernel_end = .;
}
