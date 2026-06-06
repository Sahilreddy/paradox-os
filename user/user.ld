/* Userland linker script.
 * The kernel identity-maps 4 GiB but QEMU only gives us ~512 MiB of
 * actual RAM, so the user image must land BELOW that (writes past
 * end-of-RAM are silently dropped). 0x0800_0000 = 128 MiB sits well
 * clear of the kernel image (loaded near 1 MiB) and the 16 MiB heap. */

ENTRY(_start)

SECTIONS
{
    . = 0x08000000;          /* 128 MiB */

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
