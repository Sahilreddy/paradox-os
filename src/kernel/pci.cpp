// PCI Bus Enumeration
#include "../include/pci.h"
#include "../include/serial.h"

// I/O port operations
static inline void outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// PCI device list
static pci_device pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;

// Read from PCI configuration space
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)(
        (1 << 31) |                    // Enable bit
        ((uint32_t)bus << 16) |        // Bus number
        ((uint32_t)device << 11) |     // Device number
        ((uint32_t)function << 8) |    // Function number
        (offset & 0xFC)                // Register offset (aligned to 4 bytes)
    );
    
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

// Write to PCI configuration space
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC)
    );
    
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

// Read vendor ID (0xFFFF means no device)
uint16_t pci_read_vendor_id(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t value = pci_read_config(bus, device, function, PCI_VENDOR_ID);
    return value & 0xFFFF;
}

// Get device class name
const char* pci_get_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Communication";
        case 0x08: return "System";
        case 0x09: return "Input Device";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "Intelligent I/O";
        case 0x0F: return "Satellite";
        case 0x10: return "Encryption";
        case 0x11: return "Signal Processing";
        default: return "Unknown";
    }
}

// Get vendor name (common vendors)
const char* pci_get_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
        case 0x8086: return "Intel";
        case 0x1022: return "AMD";
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "ATI/AMD";
        case 0x1234: return "QEMU";
        case 0x15AD: return "VMware";
        case 0x80EE: return "VirtualBox";
        case 0x1AF4: return "VirtIO";
        case 0x1B36: return "Red Hat";
        case 0x106B: return "Apple";
        default: return "Unknown";
    }
}

// Scan a single function
static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_read_vendor_id(bus, device, function);
    if (vendor_id == 0xFFFF) return;  // No device
    
    if (pci_device_count >= MAX_PCI_DEVICES) {
        serial_print("PCI: Warning - device limit reached\n");
        return;
    }
    
    pci_device* dev = &pci_devices[pci_device_count++];
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = vendor_id;
    
    // Read device ID
    uint32_t device_vendor = pci_read_config(bus, device, function, PCI_VENDOR_ID);
    dev->device_id = (device_vendor >> 16) & 0xFFFF;
    
    // Read class codes
    uint32_t class_info = pci_read_config(bus, device, function, PCI_REVISION_ID);
    dev->revision = class_info & 0xFF;
    dev->prog_if = (class_info >> 8) & 0xFF;
    dev->subclass = (class_info >> 16) & 0xFF;
    dev->class_code = (class_info >> 24) & 0xFF;
    
    // Read header type
    uint32_t header_info = pci_read_config(bus, device, function, 0x0C);
    dev->header_type = (header_info >> 16) & 0xFF;
    
    // Read BARs
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read_config(bus, device, function, PCI_BAR0 + (i * 4));
    }
    
    // Read interrupt info
    uint32_t interrupt_info = pci_read_config(bus, device, function, PCI_INTERRUPT_LINE);
    dev->interrupt_line = interrupt_info & 0xFF;
    dev->interrupt_pin = (interrupt_info >> 8) & 0xFF;
    
    serial_print("PCI: Found device - ");
    serial_print(pci_get_vendor_name(vendor_id));
    serial_print(" ");
    serial_print(pci_get_class_name(dev->class_code));
    serial_print(" (");
    serial_print_hex(vendor_id);
    serial_print(":");
    serial_print_hex(dev->device_id);
    serial_print(")\n");
}

// Scan all PCI devices
void pci_scan_bus() {
    serial_print("PCI: Scanning bus...\n");
    pci_device_count = 0;
    
    // Scan all buses, devices, and functions
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            uint16_t vendor_id = pci_read_vendor_id(bus, device, 0);
            if (vendor_id == 0xFFFF) continue;  // No device
            
            // Check function 0
            pci_scan_function(bus, device, 0);
            
            // Check if multi-function device
            uint32_t header_info = pci_read_config(bus, device, 0, 0x0C);
            uint8_t header_type = (header_info >> 16) & 0xFF;
            
            if (header_type & 0x80) {  // Multi-function bit set
                // Scan functions 1-7
                for (uint8_t function = 1; function < 8; function++) {
                    if (pci_read_vendor_id(bus, device, function) != 0xFFFF) {
                        pci_scan_function(bus, device, function);
                    }
                }
            }
        }
    }
    
    serial_print("PCI: Found ");
    serial_print_hex(pci_device_count);
    serial_print(" devices\n");
}

// Initialize PCI subsystem
void pci_init() {
    serial_print("PCI: Initializing PCI bus enumeration...\n");
    pci_scan_bus();
    serial_print("PCI: Initialization complete!\n");
}

// Get device count
uint32_t pci_get_device_count() {
    return pci_device_count;
}

// Get device by index
pci_device* pci_get_device(uint32_t index) {
    if (index >= pci_device_count) return nullptr;
    return &pci_devices[index];
}
