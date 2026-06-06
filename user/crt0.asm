; C runtime stub. Kernel hands us argc/argv on the user stack per the
; SysV layout; we forward them to main and call SYS_EXIT with the result.

[BITS 64]

global _start
extern main

section .text
_start:
    mov rdi, [rsp]
    lea rsi, [rsp + 8]

    and rsp, -16
    sub rsp, 8

    call main

    mov edi, eax
    mov eax, 4
    int 0x80

.hang:
    jmp .hang
