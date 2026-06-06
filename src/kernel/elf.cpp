// Minimal ELF64 loader: walks PT_LOAD segments, copies file bytes to vaddr,
// zero-fills BSS, returns e_entry. Destination vaddrs land in the
// identity-mapped user-accessible region.

#include "../include/elf.h"
#include "../include/serial.h"

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed));

#define ET_EXEC      2
#define EM_X86_64    62
#define PT_LOAD      1
#define ELFCLASS64   2
#define ELFDATA2LSB  1

uint64_t elf_load(const uint8_t* image, uint32_t len) {
    if (!image || len < sizeof(Elf64_Ehdr)) {
        serial_print("elf: image too small\n");
        return 0;
    }
    auto* eh = (const Elf64_Ehdr*)image;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        serial_print("elf: bad magic\n");
        return 0;
    }
    if (eh->e_ident[4] != ELFCLASS64) {
        serial_print("elf: not ELF64\n");
        return 0;
    }
    if (eh->e_ident[5] != ELFDATA2LSB) {
        serial_print("elf: not little-endian\n");
        return 0;
    }
    if (eh->e_machine != EM_X86_64) {
        serial_print("elf: not x86_64\n");
        return 0;
    }
    if (eh->e_type != ET_EXEC) {
        serial_print("elf: not ET_EXEC (need a static-linked executable)\n");
        return 0;
    }
    if (eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(Elf64_Phdr) > len) {
        serial_print("elf: phdr table out of bounds\n");
        return 0;
    }

    auto* ph = (const Elf64_Phdr*)(image + eh->e_phoff);
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr& p = ph[i];
        if (p.p_type != PT_LOAD) continue;

        if (p.p_offset + p.p_filesz > len) {
            serial_print("elf: PT_LOAD out of bounds\n");
            return 0;
        }

        // volatile is required — GCC -O2 elides the copy otherwise (no
        // observable side effect downstream of returning e_entry).
        volatile uint8_t* dst = (volatile uint8_t*)p.p_vaddr;
        const uint8_t* src = image + p.p_offset;

        for (uint64_t j = 0; j < p.p_filesz; j++) dst[j] = src[j];
        for (uint64_t j = p.p_filesz; j < p.p_memsz; j++) dst[j] = 0;
    }
    asm volatile("" ::: "memory");

    serial_print("elf: loaded, entry=0x");
    serial_print_hex((uint32_t)(eh->e_entry >> 32));
    serial_print_hex((uint32_t)eh->e_entry);
    serial_print("\n");
    return eh->e_entry;
}
