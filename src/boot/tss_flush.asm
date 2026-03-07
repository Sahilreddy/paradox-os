; Load TSS (Task State Segment)
[BITS 64]

global tss_flush

tss_flush:
    ; Parameter: rdi = TSS selector
    mov ax, di      ; Move selector to ax
    ltr ax          ; Load Task Register
    ret
