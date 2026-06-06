// In-memory VFS. Files hold a writable buffer; "executables" are nodes
// with a function pointer the GUI / shell can invoke.

#ifndef VFS_H
#define VFS_H

#include "types.h"

enum vfs_kind {
    VFS_DIR  = 1,
    VFS_FILE = 2,
    VFS_EXEC = 3,
};

struct vfs_node;
typedef void (*vfs_exec_fn)(const char* args);

struct vfs_node {
    char       name[32];
    vfs_kind   kind;
    vfs_node*  parent;

    vfs_node*  first_child;
    vfs_node*  next;

    char*      content;
    uint32_t   len;
    uint32_t   cap;

    vfs_exec_fn exec_fn;
    const char* description;
};

void      vfs_init();
vfs_node* vfs_root();
vfs_node* vfs_lookup(const char* path);
void      vfs_path_of(const vfs_node* n, char* out, uint32_t cap);

bool vfs_write(vfs_node* file, const char* data, uint32_t len);
bool vfs_append_char(vfs_node* file, char c);
bool vfs_pop_char(vfs_node* file);

void vfs_run(vfs_node* node, const char* args);

#endif
