// ParadoxOS Kernel Entry Point

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
#include "../include/pci.h"

// Kernel main function (now receiving 64-bit parameters)
extern "C" void kernel_main(unsigned long magic, unsigned long addr) {
    // Initialize serial port for debugging
    serial_init();
    serial_print("ParadoxOS: Serial initialized\n");
    
    // Initialize VGA terminal
    vga_init();
    serial_print("ParadoxOS: VGA initialized\n");
    
    // Note: GDT is already set up in boot.asm, so we skip gdt_init() for now
    // gdt_init();
    
    // Initialize IDT (interrupt handlers)
    idt_init();
    
    // Initialize PIC (Programmable Interrupt Controller)
    pic_init();
    
    // Initialize keyboard
    keyboard_init();
    
    // Initialize timer (100 Hz)
    timer_init(100);
    
    // Initialize system calls
    syscall_init();
    
    // Initialize memory management
    pmm_init(addr);
    paging_init();
    heap_init();
    
    // Initialize TSS (needs memory management first)
    // TODO: Fix TSS descriptor format to avoid triple fault
    // tss_init();
    // gdt_set_tss((uint64_t)&tss, sizeof(tss_entry) - 1);
    // tss_flush(GDT_TSS);
    serial_print("TSS: Disabled temporarily (kernel mode only)\n");
    
    // Initialize PCI bus
    pci_init();
    
    // Initialize PCI bus enumeration
    pci_init();
    
    // Initialize process management
    process_init();
    scheduler_init();
    
    // Enable interrupts!
    asm volatile("sti");
    serial_print("ParadoxOS: Interrupts ENABLED - keyboard ready!\n");
    
    // VERIFY interrupts are actually enabled
    uint64_t rflags;
    asm volatile("pushfq; pop %0" : "=r"(rflags));
    if (rflags & (1 << 9)) {
        serial_print("RFLAGS: IF bit is SET (interrupts enabled)\n");
    } else {
        serial_print("ERROR: IF bit is CLEAR (interrupts disabled!)\n");
    }
    
    // Check if we were booted by a Multiboot2-compliant bootloader
    if (magic != 0x36d76289) {
        serial_print("ERROR: Invalid magic number!\n");
        vga_print("ERROR: Invalid magic number!\n");
        vga_print("Not booted by Multiboot2 bootloader\n");
        return;
    }
    
    serial_print("ParadoxOS: Multiboot2 verified\n");
    
    // Display epic banner
    vga_clear();
    vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print("================================================================================\n");
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("  ____                     _            ___  ____  \n");
    vga_print(" |  _ \\ __ _ _ __ __ _  __| | _____  __/ _ \\/ ___| \n");
    vga_print(" | |_) / _` | '__/ _` |/ _| |/ _ \\ \\/ / | | \\___ \\ \n");
    vga_print(" |  __/ (_| | | | (_| | (_| | (_) >  <| |_| |___) |\n");
    vga_print(" |_|   \\__,_|_|  \\__,_|\\__|_|\\___/_/\\_\\\\___/|____/ \n");
    vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print("================================================================================\n\n");
    
    vga_setcolor(VGA_COLOR_LIGHT_YELLOW, VGA_COLOR_BLACK);
    vga_print("Welcome, Master!\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("Your OS awaits your command.\n\n");
    
    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("[OK] All systems initialized\n\n");
    
    // Initialize shell
    shell_init();
    
    serial_print("ParadoxOS: Kernel fully initialized and running!\n");
    serial_print("ParadoxOS: Shell ready for commands!\n");
    
    // Halt and wait for interrupts
    while(1) {
        asm volatile("hlt");
    }
}
