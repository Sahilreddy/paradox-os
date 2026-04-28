// Multiboot2 boot information tag definitions (subset we actually use).
// Spec: https://www.gnu.org/software/grub/manual/multiboot2/

#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include "types.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

// Tag types (only the ones we read)
#define MB2_TAG_END               0
#define MB2_TAG_CMDLINE           1
#define MB2_TAG_BOOTLOADER_NAME   2
#define MB2_TAG_MODULE            3
#define MB2_TAG_BASIC_MEMINFO     4
#define MB2_TAG_BIOS_BOOTDEV      5
#define MB2_TAG_MMAP              6
#define MB2_TAG_FRAMEBUFFER       8

#define MB2_FRAMEBUFFER_TYPE_INDEXED   0
#define MB2_FRAMEBUFFER_TYPE_RGB       1
#define MB2_FRAMEBUFFER_TYPE_EGA_TEXT  2

struct mb2_info_header {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed));

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_tag_framebuffer {
    uint32_t type;          // 8
    uint32_t size;
    uint64_t addr;          // physical address of the linear framebuffer
    uint32_t pitch;         // bytes per scanline
    uint32_t width;         // pixels
    uint32_t height;        // pixels
    uint8_t  bpp;           // bits per pixel
    uint8_t  fb_type;       // 0=indexed, 1=RGB, 2=EGA text
    uint16_t reserved;
    // Followed by colour-info union we don't need.
} __attribute__((packed));

#endif
