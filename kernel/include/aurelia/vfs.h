/* AURELIA OS - virtual filesystem (RAM-backed, modular driver interface) */
#pragma once

#include <aurelia/types.h>

#define VFS_NAME_MAX 48
#define VFS_PATH_MAX 256
#define VFS_MAX_NODES 192
#define VFS_MAX_SIZE  (256 * 1024)

typedef struct vfs_node {
    char name[VFS_NAME_MAX];
    bool used;
    bool is_dir;
    u16 mode;              /* unix-style permission bits, displayed as 0644/0755 */
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    char *data;
    u32 size;
    u32 capacity;
    u64 created_tick;
    u64 modified_tick;
} vfs_node_t;

void vfs_init(void);
vfs_node_t *vfs_root(void);
vfs_node_t *vfs_lookup(const char *path);
vfs_node_t *vfs_resolve(vfs_node_t *cwd, const char *path);
char *vfs_absolute(vfs_node_t *node, char *buffer, size_t size);
char *vfs_join(vfs_node_t *cwd, const char *path, char *buffer, size_t size);

vfs_node_t *vfs_create(const char *path, bool is_dir);
vfs_node_t *vfs_create_at(vfs_node_t *parent, const char *name, bool is_dir);
bool vfs_write(vfs_node_t *node, const void *data, u32 size);
bool vfs_append(vfs_node_t *node, const void *data, u32 size);
bool vfs_delete(vfs_node_t *node);
bool vfs_rename(vfs_node_t *node, const char *new_name);
u32 vfs_child_count(vfs_node_t *dir);
u32 vfs_used_bytes(void);
const char *vfs_name(vfs_node_t *node);
bool vfs_is_dir(vfs_node_t *node);

/* Debug helper: dump the tree to the kernel log. */
void vfs_dump(void);
