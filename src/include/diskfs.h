// On-disk persistence for the VFS.
//
//   sector 100  superblock:
//     u32 magic = 'PFSV' (0x50465356)
//     u32 version = 1
//     u32 count
//     entry[16] { char path[64]; u32 size; u32 flags; }
//
//   sector 103..  file N at 103 + N*8, 4 KiB cap each
//
// Files are keyed by absolute VFS path, so layout survives reordering.

#ifndef DISKFS_H
#define DISKFS_H

#include "types.h"

#define DISKFS_MAGIC          0x50465356u
#define DISKFS_VERSION        1u
#define DISKFS_MAX_FILES      16
#define DISKFS_FILE_SECTORS   8
#define DISKFS_SUPER_LBA      100

struct vfs_node;

void diskfs_init();
bool diskfs_save(vfs_node* file);

#endif
