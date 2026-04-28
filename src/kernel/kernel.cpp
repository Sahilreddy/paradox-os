// ParadoxOS Kernel Entry Point.
//
// Boot order:
//   1. Bring up serial + early VGA so we have somewhere to log if we panic.
//   2. Set up CPU plumbing (IDT, PIC, PIT, syscalls, keyboard).
//   3. Initialize physical memory + paging + heap (depends on multiboot2 mmap).
//   4. PCI scan, process system, scheduler.
//   5. STI — interrupts go live.
//   6. If GRUB gave us a framebuffer tag, switch to graphical mode:
//      framebuffer + font + mouse + GUI compositor, redirect shell text.
//      Otherwise, fall back to the existing VGA text-mode shell.
//   7. Main loop: gui_tick() then HLT.

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
#include "../include/tss.h"
#include "../include/pci.h"
#include "../include/multiboot2.h"
#include "../include/framebuffer.h"
#include "../include/mouse.h"
#include "../include/gui.h"
#include "../include/ata.h"

// Walk the multiboot2 tag list looking for a framebuffer tag (type 8).
// Returns nullptr if missing — caller falls back to text mode.
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

        // Tags are 8-byte aligned.
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
        vga_print("ERROR: Invalid Multiboot2 magic — halting.\n");
        while (1) asm volatile("hlt");
    }
    serial_print("ParadoxOS: Multiboot2 verified\n");

    // CPU plumbing.
    idt_init();
    pic_init();
    keyboard_init();
    timer_init(100);
    syscall_init();

    // Memory.
    pmm_init(mb_info);
    paging_init();
    heap_init();

    serial_print("TSS: disabled (kernel-mode only for now)\n");

    pci_init();
    ata_init();

    // Process system.
    process_init();
    scheduler_init();

    asm volatile("sti");
    serial_print("ParadoxOS: interrupts enabled\n");

    // Try the graphical path. If anything in the chain fails we keep the
    // VGA text-mode banner and the existing shell will run there.
    bool graphical = false;
    const mb2_tag_framebuffer* fb_tag = find_framebuffer_tag(mb_info);
    if (fb_tag && fb_init(fb_tag->addr, fb_tag->pitch,
                          fb_tag->width, fb_tag->height,
                          fb_tag->bpp, fb_tag->fb_type)) {
        mouse_init((int32_t)fb_tag->width, (int32_t)fb_tag->height);

        // Splash first, *then* compose the desktop. The splash uses the
        // framebuffer directly without any GUI state, so it's safe to run
        // before gui_init.
        gui_run_splash();
        gui_init();

        // From here on, vga_print/vga_putchar text goes into the GUI
        // terminal grid. The grid is drawn whenever a Terminal window
        // is open (which happens when the user clicks the icon).
        vga_set_gui_redirect(true);
        graphical = true;
    } else {
        serial_print("ParadoxOS: no framebuffer — staying in text mode\n");
    }

    if (!graphical) {
        vga_clear();
        vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_print("ParadoxOS (text mode)\n");
        vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        // Pre-populate the terminal grid so opening Terminal looks like
        // a real shell session that's been running since boot.
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
