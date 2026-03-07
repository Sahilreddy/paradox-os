; System Call Handler (INT 0x80)
; System calls use this convention:
; rax = syscall number
; rdi, rsi, rdx, r10, r8, r9 = arguments
; Return value in rax

[BITS 64]

extern syscall_handler_c

global syscall_handler_asm

syscall_handler_asm:
    ; Save all registers that we'll modify
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Arguments for syscall_handler_c:
    ; rdi = syscall_num (from rax)
    ; rsi = arg1 (from rdi)
    ; rdx = arg2 (from rsi) 
    ; rcx = arg3 (from rdx)
    ; r8  = arg4 (from r10)
    ; r9  = arg5 (from r8)
    
    ; Save original rdi, rsi, rdx values before we overwrite them
    mov r11, rdi    ; r11 = original rdi (arg1)
    mov r12, rsi    ; r12 = original rsi (arg2)
    mov r13, rdx    ; r13 = original rdx (arg3)
    mov r14, r10    ; r14 = original r10 (arg4)
    mov r15, r8     ; r15 = original r8 (arg5)
    
    ; Setup arguments for C handler
    mov rdi, rax    ; syscall number
    mov rsi, r11    ; arg1
    mov rdx, r12    ; arg2
    mov rcx, r13    ; arg3
    mov r8, r14     ; arg4
    mov r9, r15     ; arg5
    
    ; Call C handler
    call syscall_handler_c
    
    ; Result is in rax, restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    
    ; Return to caller (rax contains return value)
    iretq
