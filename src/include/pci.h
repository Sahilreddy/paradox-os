#ifndef PCI_H
#define PCI_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// PCI Configuration Space Addresses
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// PCI Configuration Registers
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS           0x0B
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

// PCI Device Classes
#define PCI_CLASS_STORAGE   0x01
#define PCI_CLASS_NETWORK   0x02
#define PCI_CLASS_DISPLAY   0x03
#define PCI_CLASS_BRIDGE    0x06

// PCI Subclasses (Storage)
#define PCI_SUBCLASS_IDE    0x01
#define PCI_SUBCLASS_SATA   0x06
#define PCI_SUBCLASS_NVME   0x08

// Maximum devices to track
#define MAX_PCI_DEVICES     64

// PCI Device structure
struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint32_t bar[6];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
};

// PCI Functions
void pci_init();
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
uint16_t pci_read_vendor_id(uint8_t bus, uint8_t device, uint8_t function);
void pci_scan_bus();
uint32_t pci_get_device_count();
pci_device* pci_get_device(uint32_t index);
const char* pci_get_class_name(uint8_t class_code);
const char* pci_get_vendor_name(uint16_t vendor_id);

#ifdef __cplusplus
}
#endif

#endif // PCI_H
