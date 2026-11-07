/* AURELIA OS - RAM filesystem
 *
 * A deliberately small, allocation-conservative in-memory tree. The public API
 * is filesystem-agnostic so a block-device driver (ATA/NVMe) can register a
 * second backend later; only this backend exists today.
 */
#include <aurelia/vfs.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/timer.h>

static vfs_node_t g_nodes[VFS_MAX_NODES];
static vfs_node_t *g_root;
static u32 g_used_bytes;

static vfs_node_t *node_alloc(void)
{
    for (u32 i = 0; i < VFS_MAX_NODES; i++) {
        if (!g_nodes[i].used) {
            memset(&g_nodes[i], 0, sizeof(vfs_node_t));
            g_nodes[i].used = true;
            g_nodes[i].created_tick = timer_ticks();
            g_nodes[i].modified_tick = g_nodes[i].created_tick;
            return &g_nodes[i];
        }
    }
    return NULLPTR;
}

static bool name_valid(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len >= VFS_NAME_MAX) {
        return false;
    }
    if (strchr(name, '/')) {
        return false;
    }
    return true;
}

static vfs_node_t *child_find(vfs_node_t *dir, const char *name)
{
    if (!dir || !dir->is_dir) {
        return NULLPTR;
    }
    for (vfs_node_t *child = dir->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
    }
    return NULLPTR;
}

static void attach(vfs_node_t *parent, vfs_node_t *node)
{
    node->parent = parent;
    node->next = parent->children;
    parent->children = node;
}

vfs_node_t *vfs_root(void) { return g_root; }
bool vfs_is_dir(vfs_node_t *node) { return node && node->is_dir; }
const char *vfs_name(vfs_node_t *node) { return node ? node->name : ""; }

vfs_node_t *vfs_create_at(vfs_node_t *parent, const char *name, bool is_dir)
{
    if (!parent || !parent->is_dir || !name_valid(name)) {
        return NULLPTR;
    }
    if (child_find(parent, name)) {
        return NULLPTR;
    }
    vfs_node_t *node = node_alloc();
    if (!node) {
        return NULLPTR;
    }
    strncpy(node->name, name, VFS_NAME_MAX - 1);
    node->is_dir = is_dir;
    node->mode = is_dir ? 0755 : 0644;
    attach(parent, node);
    return node;
}

static vfs_node_t *walk(vfs_node_t *start, const char *path)
{
    vfs_node_t *node = start;
    char component[VFS_NAME_MAX];
    const char *cursor = path;
    while (*cursor == '/') {
        node = g_root;
        cursor++;
    }
    while (*cursor) {
        size_t len = 0;
        while (cursor[len] && cursor[len] != '/') {
            len++;
        }
        if (len == 0) {
            cursor++;
            continue;
        }
        if (len >= VFS_NAME_MAX) {
            return NULLPTR;
        }
        memcpy(component, cursor, len);
        component[len] = '\0';
        if (strcmp(component, ".") == 0) {
            /* stay */
        } else if (strcmp(component, "..") == 0) {
            if (node && node->parent) {
                node = node->parent;
            }
        } else {
            node = child_find(node, component);
            if (!node) {
                return NULLPTR;
            }
        }
        cursor += len;
        while (*cursor == '/') {
            cursor++;
        }
    }
    return node;
}

vfs_node_t *vfs_lookup(const char *path)
{
    if (!path || path[0] != '/') {
        return NULLPTR;
    }
    return walk(g_root, path);
}

vfs_node_t *vfs_resolve(vfs_node_t *cwd, const char *path)
{
    if (!path || !*path) {
        return cwd ? cwd : g_root;
    }
    if (path[0] == '/') {
        return vfs_lookup(path);
    }
    return walk(cwd ? cwd : g_root, path);
}

char *vfs_absolute(vfs_node_t *node, char *buffer, size_t size)
{
    if (!buffer || size == 0) {
        return buffer;
    }
    if (!node || node == g_root) {
        strncpy(buffer, "/", size - 1);
        buffer[size - 1] = '\0';
        return buffer;
    }
    char parts[16][VFS_NAME_MAX];
    i32 count = 0;
    for (vfs_node_t *cur = node; cur && cur->parent && count < 16; cur = cur->parent) {
        strncpy(parts[count++], cur->name, VFS_NAME_MAX - 1);
        parts[count - 1][VFS_NAME_MAX - 1] = '\0';
    }
    size_t used = 0;
    buffer[0] = '\0';
    for (i32 i = count - 1; i >= 0; i--) {
        size_t len = strlen(parts[i]);
        if (used + len + 2 >= size) {
            break;
        }
        buffer[used++] = '/';
        memcpy(buffer + used, parts[i], len);
        used += len;
        buffer[used] = '\0';
    }
    if (used == 0 && size > 1) {
        strncpy(buffer, "/", size - 1);
        buffer[size - 1] = '\0';
    }
    return buffer;
}

char *vfs_join(vfs_node_t *cwd, const char *path, char *buffer, size_t size)
{
    if (path && path[0] == '/') {
        strncpy(buffer, path, size - 1);
        buffer[size - 1] = '\0';
        return buffer;
    }
    char base[VFS_PATH_MAX];
    vfs_absolute(cwd ? cwd : g_root, base, sizeof(base));
    size_t blen = strlen(base);
    bool needs_slash = blen == 0 || base[blen - 1] != '/';
    if (blen + (needs_slash ? 1 : 0) + strlen(path ? path : "") + 1 > size) {
        strncpy(buffer, base, size - 1);
        buffer[size - 1] = '\0';
        return buffer;
    }
    strcpy(buffer, base);
    if (needs_slash) {
        size_t len = strlen(buffer);
        buffer[len] = '/';
        buffer[len + 1] = '\0';
    }
    strcpy(buffer + strlen(buffer), path ? path : "");
    return buffer;
}

vfs_node_t *vfs_create(const char *path, bool is_dir)
{
    if (!path || !path[0]) {
        return NULLPTR;
    }
    char buffer[VFS_PATH_MAX];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    char *slash = strrchr(buffer, '/');
    const char *name;
    vfs_node_t *parent;
    if (!slash || slash == buffer) {
        parent = g_root;
        name = slash ? slash + 1 : buffer;
    } else {
        *slash = '\0';
        parent = vfs_lookup(buffer);
        name = slash + 1;
    }
    return vfs_create_at(parent, name, is_dir);
}

bool vfs_write(vfs_node_t *node, const void *data, u32 size)
{
    if (!node || node->is_dir) {
        return false;
    }
    if (size > VFS_MAX_SIZE) {
        return false;
    }
    if (size > node->capacity) {
        u32 capacity = align_up(size + 1, 512);
        char *fresh = kmalloc(capacity);
        if (!fresh) {
            return false;
        }
        if (node->data && node->size) {
            memcpy(fresh, node->data, node->size);
        }
        if (node->data) {
            kfree(node->data);
            g_used_bytes -= node->capacity;
        }
        node->data = fresh;
        node->capacity = capacity;
    }
    if (size) {
        memcpy(node->data, data, size);
    }
    node->size = size;
    node->modified_tick = timer_ticks();
    g_used_bytes += node->capacity;
    return true;
}

bool vfs_append(vfs_node_t *node, const void *data, u32 size)
{
    if (!node || node->is_dir) {
        return false;
    }
    if (node->size + size > VFS_MAX_SIZE) {
        return false;
    }
    u32 total = node->size + size;
    if (total + 1 > node->capacity) {
        u32 capacity = align_up(total + 1, 512);
        char *fresh = kmalloc(capacity);
        if (!fresh) {
            return false;
        }
        if (node->data && node->size) {
            memcpy(fresh, node->data, node->size);
        }
        if (node->data) {
            kfree(node->data);
            g_used_bytes -= node->capacity;
        }
        node->data = fresh;
        node->capacity = capacity;
        g_used_bytes += node->capacity;
    }
    memcpy(node->data + node->size, data, size);
    node->size = total;
    node->data[node->size] = '\0';
    node->modified_tick = timer_ticks();
    return true;
}

static void detach(vfs_node_t *node)
{
    if (!node || !node->parent) {
        return;
    }
    vfs_node_t **slot = &node->parent->children;
    while (*slot) {
        if (*slot == node) {
            *slot = node->next;
            return;
        }
        slot = &(*slot)->next;
    }
}

static void release_subtree(vfs_node_t *node)
{
    while (node) {
        vfs_node_t *next = node->next;
        if (node->children) {
            release_subtree(node->children);
        }
        if (node->data) {
            g_used_bytes -= node->capacity;
            kfree(node->data);
        }
        memset(node, 0, sizeof(vfs_node_t));
        node = next;
    }
}

bool vfs_delete(vfs_node_t *node)
{
    if (!node || node == g_root) {
        return false;
    }
    detach(node);
    release_subtree(node);
    return true;
}

bool vfs_rename(vfs_node_t *node, const char *new_name)
{
    if (!node || node == g_root || !name_valid(new_name)) {
        return false;
    }
    if (node->parent && child_find(node->parent, new_name)) {
        return false;
    }
    strncpy(node->name, new_name, VFS_NAME_MAX - 1);
    node->name[VFS_NAME_MAX - 1] = '\0';
    node->modified_tick = timer_ticks();
    return true;
}

u32 vfs_child_count(vfs_node_t *dir)
{
    u32 count = 0;
    if (!dir) {
        return 0;
    }
    for (vfs_node_t *child = dir->children; child; child = child->next) {
        count++;
    }
    return count;
}

u32 vfs_used_bytes(void) { return g_used_bytes; }

static void seed_file(const char *path, const char *content)
{
    vfs_node_t *node = vfs_create(path, false);
    if (node) {
        vfs_write(node, content, (u32)strlen(content));
    }
}

static void seed_dir(const char *path) { vfs_create(path, true); }

void vfs_init(void)
{
    memset(g_nodes, 0, sizeof(g_nodes));
    g_used_bytes = 0;
    g_root = node_alloc();
    if (!g_root) {
        kpanic("vfs: cannot allocate root node");
    }
    strcpy(g_root->name, "/");
    g_root->is_dir = true;
    g_root->mode = 0755;

    seed_dir("/System");
    seed_dir("/Applications");
    seed_dir("/Users");
    seed_dir("/Users/aurelia");
    seed_dir("/Users/aurelia/Documents");
    seed_dir("/Users/aurelia/Documents/Notes");
    seed_dir("/Users/aurelia/Pictures");
    seed_dir("/Users/aurelia/Music");
    seed_dir("/Users/aurelia/Downloads");
    seed_dir("/Home");
    seed_dir("/Library");
    seed_dir("/Drivers");
    seed_dir("/Configuration");
    seed_dir("/Temporary");
    seed_dir("/Recovery");

    seed_file("/System/version",
              "AURELIA OS 1.0.0 \"Solstice\" alpha\n"
              "Kernel: AURELIA 1.0.0 (x86_64, higher-half)\n"
              "Build: freestanding C11\n");
    seed_file("/System/kernel.log",
              "Boot log is mirrored to the serial port (COM1, 115200 baud).\n"
              "The in-memory kernel ring buffer is available via the log app.\n");
    seed_file("/System/packages.db",
              "aurelia-terminal 1.0.0 builtin\n"
              "aurelia-explorer 1.0.0 builtin\n"
              "aurelia-settings 1.0.0 builtin\n"
              "aurelia-notes 1.0.0 builtin\n"
              "aurelia-calculator 1.0.0 builtin\n"
              "aurelia-monitor 1.0.0 builtin\n");
    seed_file("/Users/aurelia/Documents/welcome.txt",
              "Welcome to AURELIA OS.\n\n"
              "This is a real operating system running on real hardware emulation.\n"
              "Everything on screen is drawn by the AURELIA compositor, written from\n"
              "scratch in C with no libc, no X11, no Wayland and no external toolkit.\n\n"
              "Things to try:\n"
              "  - Open Terminal and run: status, ls, uptime, neofetch\n"
              "  - Drag windows by their title bars, resize from any edge\n"
              "  - Press Super (Windows key) to open the launcher\n"
              "  - Alt+Tab cycles windows, Super+Left/Right snaps them\n"
              "  - Open Settings to change the theme, accent and wallpaper\n\n"
              "The filesystem you are reading is a RAM filesystem. It is real, but\n"
              "it does not survive a reboot yet: the ATA driver is not implemented.\n");
    seed_file("/Users/aurelia/Documents/design-notes.md",
              "# AURELIA design language\n\n"
              "Midnight #090A0E, Champagne #D6B878, Ivory #F4F0E7.\n"
              "One accent per surface. Generous negative space. Rounded corners of\n"
              "12-20 px. Type set in Inter for UI and Cormorant for display.\n\n"
              "Motion is short (120-220 ms), eased, and never blocks input.\n");
    seed_file("/Users/aurelia/Documents/Notes/shopping.md",
              "- Espresso beans\n- Olive oil\n- Notebook\n");
    seed_file("/Users/aurelia/Documents/Notes/ideas.md",
              "Solstice: a desktop that feels calm at 3am.\n");
    seed_file("/Users/aurelia/Pictures/README.txt",
              "The image viewer renders generated AURELIA artwork because no image\n"
              "codec or disk driver exists yet.\n");
    seed_file("/Library/README.txt",
              "Shared libraries will live here once user mode and ELF loading exist.\n");
    seed_file("/Drivers/README.txt",
              "Driver model: each driver registers a device and an IRQ handler.\n"
              "Implemented: framebuffer, PS/2 keyboard, PS/2 mouse, PIT, PIC, serial.\n"
              "Not implemented: ATA/NVMe, PCI enumeration, ACPI, APIC, audio, network.\n");
    seed_file("/Configuration/theme.conf",
              "mode=dark\naccent=D6B878\nwallpaper=solstice\ndensity=comfortable\n");
    seed_file("/Configuration/network.conf",
              "state=unconfigured\nnote=No network device driver is implemented.\n");
    seed_file("/Recovery/README.txt",
              "Boot with recover=1 on the kernel command line to enter recovery mode.\n"
              "Recovery mode is planned with the update architecture (milestone 9).\n");
    seed_file("/Home/README.txt",
              "Home is a compatibility mount point. User data lives in /Users/aurelia.\n");
    seed_file("/Temporary/README.txt", "Scratch space. Cleared at boot.\n");

    kprintf("[ok] vfs: %u nodes, %u bytes in RAM tree\n", VFS_MAX_NODES, vfs_used_bytes());
}

static void dump_node(vfs_node_t *node, i32 depth)
{
    char indent[32];
    i32 count = i32_min(depth * 2, 30);
    for (i32 i = 0; i < count; i++) {
        indent[i] = ' ';
    }
    indent[count] = '\0';
    kprintf("%s%s%s%s\n", indent, node->name, node->is_dir ? "/" : "",
            node->is_dir ? "" : " (file)");
    if (node->is_dir) {
        for (vfs_node_t *child = node->children; child; child = child->next) {
            dump_node(child, depth + 1);
        }
    }
}

void vfs_dump(void)
{
    dump_node(g_root, 0);
}
