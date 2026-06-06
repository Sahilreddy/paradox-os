; Ring-3 entry / exit harness. usermode_jump iretq's into ring 3;
; usermode_return longjmps back to the saved kernel state.

[BITS 64]

global usermode_jump
global usermode_return
global g_usermode_save
global g_usermode_active

section .bss
align 8
g_usermode_save:
    .rsp:    resq 1
    .rbp:    resq 1
    .rbx:    resq 1
    .r12:    resq 1
    .r13:    resq 1
    .r14:    resq 1
    .r15:    resq 1
    .rflags: resq 1
g_usermode_active:
    resb 1

section .text

; void usermode_jump(uint64_t entry_rip /rdi, uint64_t user_rsp /rsi);
usermode_jump:
    mov [rel g_usermode_save.rsp], rsp
    mov [rel g_usermode_save.rbp], rbp
    mov [rel g_usermode_save.rbx], rbx
    mov [rel g_usermode_save.r12], r12
    mov [rel g_usermode_save.r13], r13
    mov [rel g_usermode_save.r14], r14
    mov [rel g_usermode_save.r15], r15

    pushfq
    pop rax
    mov [rel g_usermode_save.rflags], rax

    mov byte [rel g_usermode_active], 1

    ; iretq frame: SS, RSP, RFLAGS, CS, RIP.
    ; User CS = 0x1B, user SS = 0x23 (DPL 3 selectors).
    push qword 0x23
    push rsi
    push qword 0x202
    push qword 0x1B
    push rdi

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iretq

; void usermode_return();
; Called from the SYS_EXIT handler.
usermode_return:
    mov byte [rel g_usermode_active], 0

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, [rel g_usermode_save.rsp]
    mov rbp, [rel g_usermode_save.rbp]
    mov rbx, [rel g_usermode_save.rbx]
    mov r12, [rel g_usermode_save.r12]
    mov r13, [rel g_usermode_save.r13]
    mov r14, [rel g_usermode_save.r14]
    mov r15, [rel g_usermode_save.r15]

    ; popfq restores IF (cleared by the int 0x80 interrupt gate). Skipping
    ; this leaves the kernel hlt deadlocked after every ring-3 return.
    push qword [rel g_usermode_save.rflags]
    popfq

    ret
