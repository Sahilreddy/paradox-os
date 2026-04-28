// ATA / IDE PIO disk driver.
// Polled, primary-master-only, 28-bit LBA. Just enough to read sectors.

#ifndef ATA_H
#define ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512

struct ata_device {
    bool present;
    uint64_t sectors;        // device capacity
    char model[41];          // null-terminated, trimmed
};

void ata_init();
const ata_device* ata_primary_master();

// Read `count` 512-byte sectors starting at `lba` into `buf`.
// Returns true on success.
bool ata_read(uint32_t lba, uint8_t count, void* buf);

#endif
