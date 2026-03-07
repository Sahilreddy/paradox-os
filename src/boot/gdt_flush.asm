; GDT Flush - Load new GDT and update segment registers
; C signature: extern "C" void gdt_flush(uint64_t gdt_ptr);

section .text
bits 64

global gdt_flush

gdt_flush:
    ; RDI contains pointer to GDT pointer structure
    lgdt [rdi]              ; Load the new GDT
    
    ; Reload segment registers
    mov ax, 0x10            ; Kernel data segment (2nd entry * 8)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Reload code segment using far return
    ; Push the new CS and return address to do a far return
    push qword 0x08         ; Kernel code segment
    lea rax, [rel .flush]   ; Load address of .flush label
    push rax                ; Push return address
    retfq                   ; Far return (updates CS)
    
.flush:
    ret                     ; Return to caller
