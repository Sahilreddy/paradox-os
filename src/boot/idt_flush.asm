; IDT Flush - Load new IDT
; C signature: extern "C" void idt_flush(uint64_t idt_ptr);

section .text
bits 64

global idt_flush

idt_flush:
    ; RDI contains pointer to IDT pointer structure
    lidt [rdi]              ; Load the new IDT
    ret
