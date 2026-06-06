#include "../include/kernel.h"
#include "../include/vga.h"
#include "../include/serial.h"
#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/memory.h"
#include "../include/paging.h"
#include "../include/process.h"
#include "../include/timer.h"
#include "../include/syscall.h"
#include "../include/gdt.h"
#include "../include/tss.h"
#include "../include/pci.h"
#include "../include/multiboot2.h"
#include "../include/framebuffer.h"
#include "../include/mouse.h"
#include "../include/gui.h"
#include "../include/ata.h"
#include "../include/vfs.h"
#include "../include/diskfs.h"
#include "../include/usermode.h"
#include "../include/stdin.h"

static const mb2_tag_framebuffer* find_framebuffer_tag(uint64_t mb_info_phys) {
    if (!mb_info_phys) return nullptr;

    auto* hdr = (const mb2_info_header*)mb_info_phys;
    const uint8_t* p = (const uint8_t*)mb_info_phys + sizeof(mb2_info_header);
    const uint8_t* end = (const uint8_t*)mb_info_phys + hdr->total_size;

    while (p < end) {
        auto* tag = (const mb2_tag*)p;
        if (tag->type == MB2_TAG_END) break;
        if (tag->type == MB2_TAG_FRAMEBUFFER)
            return (const mb2_tag_framebuffer*)tag;

        uint32_t size = (tag->size + 7) & ~7u;
        p += size;
    }
    return nullptr;
}

extern "C" void kernel_main(unsigned long magic, unsigned long mb_info) {
    serial_init();
    serial_print("ParadoxOS: Serial initialized\n");

    vga_init();
    serial_print("ParadoxOS: VGA initialized\n");

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        serial_print("ERROR: Invalid Multiboot2 magic\n");
        vga_print("ERROR: Invalid Multiboot2 magic - halting.\n");
        while (1) asm volatile("hlt");
    }
    serial_print("ParadoxOS: Multiboot2 verified\n");

    idt_init();
    pic_init();
    keyboard_init();
    timer_init(100);
    syscall_init();

    pmm_init(mb_info);
    paging_init();
    heap_init();

    // GDT/TSS for ring-3: gdt_init swaps to a GDT with user descriptors,
    // tss_init points RSP0 at a kernel stack, tss_flush loads it.
    gdt_init();
    tss_init();
    gdt_set_tss((uint64_t)&tss, sizeof(tss_entry) - 1);
    tss_flush(GDT_TSS);
    serial_print("TSS: loaded\n");

    pci_init();
    ata_init();
    vfs_init();
    diskfs_init();
    stdin_init();

    process_init();
    scheduler_init();

    asm volatile("sti");
    serial_print("ParadoxOS: interrupts enabled\n");

    bool graphical = false;
    const mb2_tag_framebuffer* fb_tag = find_framebuffer_tag(mb_info);
    if (fb_tag && fb_init(fb_tag->addr, fb_tag->pitch,
                          fb_tag->width, fb_tag->height,
                          fb_tag->bpp, fb_tag->fb_type)) {
        mouse_init((int32_t)fb_tag->width, (int32_t)fb_tag->height);

        gui_run_splash();
        gui_init();

        vga_set_gui_redirect(true);
        graphical = true;
    } else {
        serial_print("ParadoxOS: no framebuffer - text mode\n");
    }

    if (!graphical) {
        vga_clear();
        vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_print("ParadoxOS (text mode)\n");
        vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        vga_print("ParadoxOS - graphical kernel\n");
        vga_print("Click the Terminal icon on the desktop.\n\n");
    }

    shell_init();

    serial_print("ParadoxOS: kernel ready\n");

    while (1) {
        if (graphical) gui_tick();
        asm volatile("hlt");
    }
}
