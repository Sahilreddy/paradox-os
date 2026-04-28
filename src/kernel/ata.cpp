// ATA / IDE PIO disk driver — primary master only, polled.
//
// QEMU exposes a standard IDE interface on I/O ports 0x1F0–0x1F7 (cmd/data)
// and 0x3F6 (control). We use 28-bit LBA reads, which is more than enough
// for any disk QEMU hands us (137 GiB cap on 28-bit LBA, fine for a hobby OS).

#include "../include/ata.h"
#include "../include/serial.h"

#define ATA_PRI_DATA       0x1F0
#define ATA_PRI_ERR        0x1F1
#define ATA_PRI_FEATURES   0x1F1
#define ATA_PRI_SECCOUNT   0x1F2
#define ATA_PRI_LBA0       0x1F3
#define ATA_PRI_LBA1       0x1F4
#define ATA_PRI_LBA2       0x1F5
#define ATA_PRI_DRIVE      0x1F6
#define ATA_PRI_STATUS     0x1F7
#define ATA_PRI_COMMAND    0x1F7
#define ATA_PRI_CTRL       0x3F6

#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_READ_PIO   0x20

#define ATA_SR_BSY         0x80
#define ATA_SR_DRDY        0x40
#define ATA_SR_DRQ         0x08
#define ATA_SR_ERR         0x01

static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t r;
    asm volatile("inw %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void io_wait_400ns() {
    // Reading the alternate status register four times is the classic
    // "400ns wait" used after issuing an ATA command.
    for (int i = 0; i < 4; i++) (void)inb(ATA_PRI_CTRL);
}

static ata_device g_pri_master = { false, 0, {0} };

static bool wait_not_busy() {
    for (int i = 0; i < 1000000; i++) {
        if (!(inb(ATA_PRI_STATUS) & ATA_SR_BSY)) return true;
    }
    return false;
}

static bool wait_drq() {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(ATA_PRI_STATUS);
        if (s & ATA_SR_ERR) return false;
        if (!(s & ATA_SR_BSY) && (s & ATA_SR_DRQ)) return true;
    }
    return false;
}

void ata_init() {
    // Select primary master, then issue IDENTIFY.
    outb(ATA_PRI_DRIVE, 0xA0);
    io_wait_400ns();
    outb(ATA_PRI_SECCOUNT, 0);
    outb(ATA_PRI_LBA0, 0);
    outb(ATA_PRI_LBA1, 0);
    outb(ATA_PRI_LBA2, 0);
    outb(ATA_PRI_COMMAND, ATA_CMD_IDENTIFY);
    io_wait_400ns();

    uint8_t status = inb(ATA_PRI_STATUS);
    if (status == 0) {
        serial_print("ATA: no primary master\n");
        return;
    }
    if (!wait_not_busy()) {
        serial_print("ATA: identify timed out (busy)\n");
        return;
    }
    // Spec quirk: ATAPI/SATA may set lba1/lba2 to non-zero; we'd skip those.
    if (inb(ATA_PRI_LBA1) != 0 || inb(ATA_PRI_LBA2) != 0) {
        serial_print("ATA: primary master is ATAPI/SATA - not supported\n");
        return;
    }
    if (!wait_drq()) {
        serial_print("ATA: identify did not produce DRQ\n");
        return;
    }

    uint16_t info[256];
    for (int i = 0; i < 256; i++) info[i] = inw(ATA_PRI_DATA);

    // Word 60-61 = 28-bit total addressable sectors.
    g_pri_master.sectors = (uint64_t)info[60] | ((uint64_t)info[61] << 16);

    // Words 27-46 = model string, byte-swapped per word, space-padded.
    char* m = g_pri_master.model;
    for (int w = 0; w < 20; w++) {
        m[w * 2 + 0] = (char)(info[27 + w] >> 8);
        m[w * 2 + 1] = (char)(info[27 + w] & 0xFF);
    }
    m[40] = 0;
    // Trim trailing spaces.
    for (int i = 39; i >= 0 && m[i] == ' '; i--) m[i] = 0;

    g_pri_master.present = true;
    serial_print("ATA: primary master \"");
    serial_print(g_pri_master.model);
    serial_print("\" online\n");
}

const ata_device* ata_primary_master() {
    return &g_pri_master;
}

bool ata_read(uint32_t lba, uint8_t count, void* buf) {
    if (!g_pri_master.present) return false;
    if (count == 0) count = 1;

    if (!wait_not_busy()) return false;

    // Drive select + top 4 LBA bits + LBA mode bit (0xE0).
    outb(ATA_PRI_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRI_FEATURES, 0);
    outb(ATA_PRI_SECCOUNT, count);
    outb(ATA_PRI_LBA0, (uint8_t)lba);
    outb(ATA_PRI_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_PRI_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_PRI_COMMAND, ATA_CMD_READ_PIO);

    uint16_t* out = (uint16_t*)buf;
    for (uint8_t s = 0; s < count; s++) {
        if (!wait_drq()) return false;
        for (int i = 0; i < 256; i++) *out++ = inw(ATA_PRI_DATA);
        io_wait_400ns();
    }
    return true;
}
