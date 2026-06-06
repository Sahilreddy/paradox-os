; Multiboot2 entry: set up 4 GiB identity map with 2 MiB pages, switch to
; long mode, jump to kernel_main(magic, mb_info).

MAGIC          equ 0xE85250D6
ARCH           equ 0
HEADER_LENGTH  equ header_end - header_start
CHECKSUM       equ -(MAGIC + ARCH + HEADER_LENGTH)

section .multiboot
header_start:
    align 8
    dd MAGIC
    dd ARCH
    dd HEADER_LENGTH
    dd CHECKSUM

    ; framebuffer request (optional — fall back to text mode if denied)
    align 8
fb_tag_start:
    dw 5
    dw 1
    dd fb_tag_end - fb_tag_start
    dd 1280
    dd 800
    dd 32
fb_tag_end:

    ; module alignment
    align 8
    dw 6
    dw 0
    dd 8

    align 8
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; 4 GiB identity map: P4[0] -> P3, P3[0..3] -> P2_*, P2 entries are 2 MiB pages.
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table_0:
    resb 4096
p2_table_1:
    resb 4096
p2_table_2:
    resb 4096
p2_table_3:
    resb 4096

section .text
bits 32
global start
extern kernel_main

start:
    mov esp, stack_top

    ; eax = multiboot magic, ebx = pointer to multiboot info
    mov edi, eax
    mov esi, ebx

    mov dword [0xb8000], 0x2f502f50

    call check_long_mode
    mov dword [0xb8004], 0x2f412f41

    call setup_page_tables
    mov dword [0xb8008], 0x2f472f47

    call enable_paging
    mov dword [0xb800c], 0x2f452f45

    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

check_long_mode:
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

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret

.no_long_mode:
    mov dword [0xb8000], 0x4f4e4f4e
    mov dword [0xb8004], 0x4f304f4f
    mov dword [0xb8008], 0x4f344f36
.halt:
    cli
    hlt
    jmp .halt

setup_page_tables:
    ; Link entries P4/P3 use 0x07 (P|W|U) — the U bit at every level is
    ; required, not just the leaf, or ring-3 access faults.
    mov eax, p3_table
    or eax, 0b111
    mov [p4_table], eax

    mov eax, p2_table_0
    or eax, 0b111
    mov [p3_table + 0 * 8], eax

    mov eax, p2_table_1
    or eax, 0b111
    mov [p3_table + 1 * 8], eax

    mov eax, p2_table_2
    or eax, 0b111
    mov [p3_table + 2 * 8], eax

    mov eax, p2_table_3
    or eax, 0b111
    mov [p3_table + 3 * 8], eax

    ; 4 * 512 = 2048 leaf entries, each a 2 MiB huge page. Flags 0x87 =
    ; P|W|U|huge. The global U bit here is not a real isolation boundary;
    ; per-process page tables would replace it.
    xor ecx, ecx
.map_loop:
    mov eax, 0x200000           ; 2 MiB
    mul ecx                     ; edx:eax = ecx * 2 MiB
    or eax, 0b10000111          ; present | writable | user | huge
    mov [p2_table_0 + ecx * 8], eax
    mov dword [p2_table_0 + ecx * 8 + 4], edx  ; high 32 bits of phys addr

    inc ecx
    cmp ecx, 2048
    jne .map_loop
    ret

enable_paging:
    mov eax, p4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5              ; CR4.PAE
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8              ; EFER.LME
    wrmsr

    mov eax, cr0
    or eax, 1 << 31             ; CR0.PG
    mov cr0, eax
    ret

bits 64
long_mode_start:
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top

    mov rax, 0x2f342f36
    mov [0xb8010], rax

    ; Zero-extend the magic/mb_info into rdi/rsi for kernel_main(magic, mb_info).
    mov eax, edi
    mov rdi, rax
    mov eax, esi
    mov rsi, rax

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .rodata
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)
.data: equ $ - gdt64
    dq (1<<44) | (1<<47)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
