#include "../include/diskfs.h"
#include "../include/ata.h"
#include "../include/vfs.h"
#include "../include/serial.h"

struct entry {
    char     path[64];
    uint32_t size;
    uint32_t flags;
} __attribute__((packed));

struct super_block {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t reserved;
    entry    entries[DISKFS_MAX_FILES];
    // 1168 bytes — fits in 3 sectors.
} __attribute__((packed));

static uint8_t      g_sector[512];
static super_block  g_super;
static bool         g_loaded = false;

static uint32_t s_len(const char* s) {
    uint32_t n = 0; while (s && s[n]) n++; return n;
}
static bool s_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

#define DATA_START_LBA 103

static bool read_super(super_block* sb) {
    uint8_t* p = (uint8_t*)sb;
    for (uint32_t i = 0; i < 3; i++) {
        if (!ata_read(DISKFS_SUPER_LBA + i, 1, g_sector)) return false;
        uint32_t off = i * 512;
        uint32_t want = sizeof(super_block) - off;
        if (want > 512) want = 512;
        for (uint32_t j = 0; j < want; j++) p[off + j] = g_sector[j];
    }
    return true;
}

static bool write_super(const super_block* sb) {
    const uint8_t* p = (const uint8_t*)sb;
    for (uint32_t i = 0; i < 3; i++) {
        for (uint32_t j = 0; j < 512; j++) g_sector[j] = 0;
        uint32_t off = i * 512;
        uint32_t want = sizeof(super_block) - off;
        if (want > 512) want = 512;
        for (uint32_t j = 0; j < want; j++) g_sector[j] = p[off + j];
        if (!ata_write(DISKFS_SUPER_LBA + i, 1, g_sector)) return false;
    }
    return true;
}

static void vfs_path_of_into(const vfs_node* n, char* out, uint32_t cap) {
    extern void vfs_path_of(const vfs_node*, char*, uint32_t);
    vfs_path_of(n, out, cap);
}

static int find_slot(const char* path) {
    for (uint32_t i = 0; i < g_super.count; i++) {
        if (s_eq(g_super.entries[i].path, path)) return (int)i;
    }
    return -1;
}

static int alloc_slot(const char* path) {
    if (g_super.count >= DISKFS_MAX_FILES) return -1;
    int idx = (int)g_super.count++;
    uint32_t n = s_len(path);
    if (n >= sizeof(g_super.entries[0].path))
        n = sizeof(g_super.entries[0].path) - 1;
    for (uint32_t i = 0; i < n; i++)
        g_super.entries[idx].path[i] = path[i];
    g_super.entries[idx].path[n] = 0;
    g_super.entries[idx].size = 0;
    g_super.entries[idx].flags = 0;
    return idx;
}

// Walk an absolute path to its VFS node. We only restore into existing
// directories (no mkdir on restore) — /home is created in vfs_init.
static vfs_node* ensure_home_file(const char* path) {
    if (path[0] != '/') return nullptr;
    vfs_node* cur = vfs_root();
    if (!cur) return nullptr;
    const char* p = path + 1;
    while (*p) {
        char comp[32]; uint32_t ci = 0;
        while (*p && *p != '/' && ci + 1 < sizeof(comp)) comp[ci++] = *p++;
        comp[ci] = 0;
        bool last = (*p == 0);
        if (*p == '/') p++;

        vfs_node* child = cur->first_child;
        while (child) {
            bool eq = true;
            for (uint32_t i = 0; i < sizeof(comp); i++) {
                if (child->name[i] != comp[i]) { eq = false; break; }
                if (comp[i] == 0) break;
            }
            if (eq) break;
            child = child->next;
        }
        if (!child) {
            (void)last;
            return nullptr;
        }
        cur = child;
    }
    return cur;
}

void diskfs_init() {
    if (!read_super(&g_super)) {
        serial_print("diskfs: superblock read failed\n");
        return;
    }
    if (g_super.magic != DISKFS_MAGIC || g_super.version != DISKFS_VERSION) {
        // Fresh/foreign disk — write a blank superblock so saves can land.
        for (uint32_t i = 0; i < sizeof(super_block); i++)
            ((uint8_t*)&g_super)[i] = 0;
        g_super.magic = DISKFS_MAGIC;
        g_super.version = DISKFS_VERSION;
        g_super.count = 0;
        write_super(&g_super);
        serial_print("diskfs: initialized blank superblock\n");
        g_loaded = true;
        return;
    }

    serial_print("diskfs: superblock OK, restoring files\n");

    for (uint32_t i = 0; i < g_super.count; i++) {
        const entry& e = g_super.entries[i];
        if (e.size == 0 || e.path[0] == 0) continue;

        vfs_node* n = ensure_home_file(e.path);
        if (!n) {
            serial_print("diskfs: skipped (no VFS node) ");
            serial_print(e.path);
            serial_print("\n");
            continue;
        }

        static uint8_t buf[DISKFS_FILE_SECTORS * 512];
        uint32_t lba = DATA_START_LBA + i * DISKFS_FILE_SECTORS;
        bool ok = true;
        for (uint32_t s = 0; s < DISKFS_FILE_SECTORS; s++)
            ok = ok && ata_read(lba + s, 1, buf + s * 512);
        if (!ok) continue;

        uint32_t size = e.size;
        if (size > sizeof(buf)) size = sizeof(buf);
        vfs_write(n, (const char*)buf, size);
    }
    g_loaded = true;
}

bool diskfs_save(vfs_node* file) {
    if (!file) return false;
    if (!g_loaded) {
        serial_print("diskfs: superblock not loaded\n");
        return false;
    }

    char path[64];
    vfs_path_of_into(file, path, sizeof(path));

    int idx = find_slot(path);
    if (idx < 0) idx = alloc_slot(path);
    if (idx < 0) {
        serial_print("diskfs: out of slots\n");
        return false;
    }

    uint32_t size = file->len;
    if (size > DISKFS_FILE_SECTORS * 512) size = DISKFS_FILE_SECTORS * 512;
    g_super.entries[idx].size = size;

    static uint8_t buf[DISKFS_FILE_SECTORS * 512];
    for (uint32_t i = 0; i < sizeof(buf); i++) buf[i] = 0;
    for (uint32_t i = 0; i < size; i++) buf[i] = (uint8_t)file->content[i];

    uint32_t lba = DATA_START_LBA + (uint32_t)idx * DISKFS_FILE_SECTORS;
    bool ok = true;
    for (uint32_t s = 0; s < DISKFS_FILE_SECTORS; s++)
        ok = ok && ata_write(lba + s, 1, buf + s * 512);
    if (!ok) {
        serial_print("diskfs: write failed\n");
        return false;
    }

    if (!write_super(&g_super)) return false;

    serial_print("diskfs: saved ");
    serial_print(path);
    serial_print("\n");
    return true;
}
