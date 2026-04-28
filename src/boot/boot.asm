; ParadoxOS Bootloader
; Multiboot2-compliant boot code for x86_64
;
; Responsibilities:
;   - Multiboot2 header (with framebuffer + module-align request tags)
;   - Identity map the first 4 GiB with 2 MiB huge pages
;   - Switch CPU into long mode and jump to kernel_main(magic, mb_info)

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

    ; --- Framebuffer request tag (type 5) --------------------------------
    ; Ask GRUB for a 1024x768x32 linear framebuffer. Flags bit 0 = optional,
    ; so if GRUB cannot satisfy the request it still loads us (we then fall
    ; back to text mode).
    align 8
fb_tag_start:
    dw 5            ; type
    dw 1            ; flags = optional
    dd fb_tag_end - fb_tag_start
    dd 1024         ; width
    dd 768          ; height
    dd 32           ; bpp
fb_tag_end:

    ; --- Module alignment request tag (type 6) ---------------------------
    align 8
    dw 6
    dw 0
    dd 8

    ; --- End tag ---------------------------------------------------------
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

; Long-mode page tables. We identity-map the first 4 GiB with 2 MiB huge
; pages: P4[0] -> P3, P3[0..3] -> P2_0..P2_3, each P2 entry is a 2 MiB page.
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

    ; Early text-mode boot indicator (harmless if the screen is in graphics)
    mov dword [0xb8000], 0x2f502f50  ; "PP"

    call check_long_mode
    mov dword [0xb8004], 0x2f412f41  ; "AA"

    call setup_page_tables
    mov dword [0xb8008], 0x2f472f47  ; "GG"

    call enable_paging
    mov dword [0xb800c], 0x2f452f45  ; "EE"

    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

; -----------------------------------------------------------------------------
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
    mov dword [0xb8000], 0x4f4e4f4e  ; "NN" white-on-red
    mov dword [0xb8004], 0x4f304f4f  ; "OO"
    mov dword [0xb8008], 0x4f344f36  ; "64"
.halt:
    cli
    hlt
    jmp .halt

; -----------------------------------------------------------------------------
; Build the page tables: P4[0] -> P3, P3[0..3] -> 4 x P2 (each 1 GiB),
; every P2 entry is a 2 MiB huge page covering the next 2 MiB of physical
; memory. Result: 4 GiB identity map.
setup_page_tables:
    ; P4[0] -> P3
    mov eax, p3_table
    or eax, 0b11
    mov [p4_table], eax

    ; P3[0..3] -> p2_table_0..3
    mov eax, p2_table_0
    or eax, 0b11
    mov [p3_table + 0 * 8], eax

    mov eax, p2_table_1
    or eax, 0b11
    mov [p3_table + 1 * 8], eax

    mov eax, p2_table_2
    or eax, 0b11
    mov [p3_table + 2 * 8], eax

    mov eax, p2_table_3
    or eax, 0b11
    mov [p3_table + 3 * 8], eax

    ; Fill 4 * 512 = 2048 P2 entries, each is a 2 MiB huge page.
    ; The mapped physical address is index * 2 MiB.
    xor ecx, ecx                ; ecx = page index 0..2047
.map_loop:
    mov eax, 0x200000           ; 2 MiB
    mul ecx                     ; edx:eax = ecx * 2 MiB
    or eax, 0b10000011          ; present | writable | huge
    mov [p2_table_0 + ecx * 8], eax
    mov dword [p2_table_0 + ecx * 8 + 4], edx  ; high 32 bits of phys addr

    inc ecx
    cmp ecx, 2048
    jne .map_loop
    ret

; -----------------------------------------------------------------------------
enable_paging:
    mov eax, p4_table
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; EFER.LME = 1 (long mode enable)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; CR0.PG = 1
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

; -----------------------------------------------------------------------------
bits 64
long_mode_start:
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top

    ; Visible-on-text-mode indicator (harmless in graphics mode).
    mov rax, 0x2f342f36
    mov [0xb8010], rax

    ; Zero-extend the 32-bit values stashed in edi/esi into rdi/rsi for the
    ; SystemV AMD64 ABI: kernel_main(magic, mb_info_phys).
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
