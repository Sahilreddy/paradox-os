; ParadoxOS Bootloader
; Multiboot2-compliant boot code for x86_64

MAGIC equ 0xE85250D6                ; Multiboot2 magic number
ARCH equ 0                          ; i386 protected mode
HEADER_LENGTH equ header_end - header_start
CHECKSUM equ -(MAGIC + ARCH + HEADER_LENGTH)

section .multiboot
header_start:
    align 8
    dd MAGIC
    dd ARCH
    dd HEADER_LENGTH
    dd CHECKSUM
    
    ; End tag
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

section .bss
align 16
stack_bottom:
    resb 16384  ; 16 KB stack
stack_top:

; Page tables for long mode
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096

section .text
bits 32
global start
extern kernel_main

start:
    ; Set up stack (32-bit)
    mov esp, stack_top
    
    ; Save multiboot info (32-bit registers)
    ; EAX contains magic value
    ; EBX contains address of multiboot info structure
    mov edi, eax  ; Save magic in EDI
    mov esi, ebx  ; Save multiboot info in ESI
    
    ; Print 'P' to screen as early boot indicator
    mov dword [0xb8000], 0x2f502f50  ; 'PP' in white on green
    
    ; Check for long mode support
    call check_long_mode
    
    ; Print 'A' - passed long mode check
    mov dword [0xb8004], 0x2f412f41  ; 'AA'
    
    ; Set up page tables for long mode
    call setup_page_tables
    
    ; Print 'G' - page tables set up
    mov dword [0xb8008], 0x2f472f47  ; 'GG'
    
    ; Enable paging and long mode
    call enable_paging
    
    ; Print 'E' - paging enabled
    mov dword [0xb800c], 0x2f452f45  ; 'EE'
    
    ; Load 64-bit GDT
    lgdt [gdt64.pointer]
    
    ; Jump to 64-bit code
    jmp gdt64.code:long_mode_start

; Check if CPU supports long mode
check_long_mode:
    ; Check for CPUID
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_long_mode
    
    ; Check for extended CPUID
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    ; Check for long mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret

.no_long_mode:
    ; Print "NO64" error
    mov dword [0xb8000], 0x4f4e4f4e  ; 'NN' - white on red
    mov dword [0xb8004], 0x4f304f4f  ; 'OO'
    mov dword [0xb8008], 0x4f344f36  ; '64'
.halt:
    cli
    hlt
    jmp .halt

; Set up identity mapping for first 2MB
setup_page_tables:
    ; Map P4[0] -> P3
    mov eax, p3_table
    or eax, 0b11  ; present + writable
    mov [p4_table], eax
    
    ; Map P3[0] -> P2
    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax
    
    ; Map P2 entries (512 * 2MB = 1GB)
    mov ecx, 0
.map_p2_table:
    mov eax, 0x200000  ; 2MB
    mul ecx
    or eax, 0b10000011 ; present + writable + huge
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_table
    ret

; Enable paging and switch to long mode
enable_paging:
    ; Load P4 to CR3
    mov eax, p4_table
    mov cr3, eax
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Set long mode bit in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

bits 64
long_mode_start:
    ; Clear segment registers
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Set up 64-bit stack
    mov rsp, stack_top
    
    ; Print '64' to show we're in 64-bit mode
    mov rax, 0x2f342f36  ; '64' in white on green
    mov [0xb8010], rax
    
    ; Zero-extend EDI/ESI to RDI/RSI (should be automatic but let's be explicit)
    mov eax, edi
    mov rdi, rax
    mov eax, esi
    mov rsi, rax
    
    ; Call kernel main
    call kernel_main
    
    ; Hang if kernel returns
.hang:
    cli
    hlt
    jmp .hang

; 64-bit GDT
section .rodata
gdt64:
    dq 0                                                ; null descriptor
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)          ; code segment
.data: equ $ - gdt64
    dq (1<<44) | (1<<47)                               ; data segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
