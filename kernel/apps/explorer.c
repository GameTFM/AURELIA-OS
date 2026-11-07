/* AURELIA OS - AURELIA Explorer */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/vfs.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/timer.h>
#include <aurelia/desktop.h>

#define EXP_TABS 3
#define EXP_FIELD 48

typedef struct {
    vfs_node_t *cwd;
    vfs_node_t *selected;
    vfs_node_t *tabs[EXP_TABS];
    i32 tab_count;
    i32 active_tab;
    char query[32];
    u32 query_len;
    i32 mode; /* 0 none, 1 new folder, 2 new file, 3 rename */
    char field[EXP_FIELD];
    u32 field_len;
    char status[96];
    i32 mx, my;
    i32 row_at_mouse;
    u64 last_click_ms;
    vfs_node_t *last_click_node;
} exp_state_t;

static const struct {
    const char *label;
    const char *path;
} k_places[] = {
    {"Home", "/Users/aurelia"},
    {"Documents", "/Users/aurelia/Documents"},
    {"Notes", "/Users/aurelia/Documents/Notes"},
    {"Pictures", "/Users/aurelia/Pictures"},
    {"Downloads", "/Users/aurelia/Downloads"},
    {"Applications", "/Applications"},
    {"System", "/System"},
    {"Configuration", "/Configuration"},
    {"Recovery", "/Recovery"},
};

static exp_state_t *state_of(window_t *w) { return (exp_state_t *)w->state; }

static bool matches(const char *name, const char *query)
{
    if (!query || !*query) {
        return true;
    }
    char lower[VFS_NAME_MAX];
    strncpy(lower, name, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    str_tolower(lower);
    char q[32];
    strncpy(q, query, sizeof(q) - 1);
    q[sizeof(q) - 1] = '\0';
    str_tolower(q);
    return strstr(lower, q) != NULLPTR;
}

static void set_cwd(exp_state_t *s, vfs_node_t *node)
{
    if (!node || !node->is_dir) {
        return;
    }
    s->cwd = node;
    s->selected = NULLPTR;
    s->tabs[s->active_tab] = node;
    s->query_len = 0;
    s->query[0] = '\0';
}

static void exp_status(exp_state_t *s, const char *text)
{
    strncpy(s->status, text, sizeof(s->status) - 1);
    s->status[sizeof(s->status) - 1] = '\0';
}

static i32 list_rows(exp_state_t *s, vfs_node_t **out, i32 max)
{
    i32 count = 0;
    if (!s->cwd) {
        return 0;
    }
    for (vfs_node_t *child = s->cwd->children; child && count < max; child = child->next) {
        if (!matches(child->name, s->query)) {
            continue;
        }
        if (out) {
            out[count] = child;
        }
        count++;
    }
    return count;
}

static void exp_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    exp_state_t *s = state_of(w);
    if (s->mode != 0) {
        if (event->key == KEY_ENTER) {
            char name[EXP_FIELD];
            strncpy(name, s->field, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            if (s->mode == 3 && s->selected) {
                if (vfs_rename(s->selected, name)) {
                    exp_status(s, "Renamed.");
                } else {
                    exp_status(s, "Rename failed: name in use.");
                }
            } else {
                char path[VFS_PATH_MAX];
                vfs_join(s->cwd, name, path, sizeof(path));
                if (vfs_lookup(path)) {
                    exp_status(s, "An item with that name already exists.");
                } else if (vfs_create(path, s->mode == 1)) {
                    exp_status(s, s->mode == 1 ? "Folder created." : "File created.");
                } else {
                    exp_status(s, "Could not create item.");
                }
            }
            s->mode = 0;
            s->field_len = 0;
            s->field[0] = '\0';
            return;
        }
        if (event->key == KEY_ESCAPE) {
            s->mode = 0;
            return;
        }
        if (event->key == KEY_BACKSPACE) {
            if (s->field_len > 0) {
                s->field[--s->field_len] = '\0';
            }
            return;
        }
        if (event->key >= 32 && event->key < 127 && s->field_len < EXP_FIELD - 1) {
            s->field[s->field_len++] = (char)event->key;
            s->field[s->field_len] = '\0';
        }
        return;
    }
    if (event->key == KEY_BACKSPACE) {
        if (s->query_len > 0) {
            s->query[--s->query_len] = '\0';
        }
        return;
    }
    if (event->key == KEY_ESCAPE) {
        s->query_len = 0;
        s->query[0] = '\0';
        return;
    }
    if (event->key == KEY_ENTER && s->selected) {
        if (s->selected->is_dir) {
            set_cwd(s, s->selected);
        } else {
            char path[VFS_PATH_MAX];
            vfs_absolute(s->selected, path, sizeof(path));
            desktop_open_file(path);
        }
        return;
    }
    if (event->key == KEY_DELETE && s->selected) {
        if (vfs_delete(s->selected)) {
            exp_status(s, "Deleted.");
            s->selected = NULLPTR;
        } else {
            exp_status(s, "Delete failed: folder not empty.");
        }
        return;
    }
    if (event->key >= 32 && event->key < 127 && s->query_len < sizeof(s->query) - 1) {
        s->query[s->query_len++] = (char)event->key;
        s->query[s->query_len] = '\0';
    }
}

static void exp_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)released;
    exp_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }

    /* Sidebar */
    if (x < 172 && y >= 42 && y < 42 + (i32)ARRAY_SIZE(k_places) * 34) {
        i32 index = (y - 42) / 34;
        if (index >= 0 && index < (i32)ARRAY_SIZE(k_places)) {
            vfs_node_t *node = vfs_lookup(k_places[index].path);
            if (node) {
                set_cwd(s, node);
                exp_status(s, k_places[index].label);
            }
        }
        return;
    }

    /* Tabs */
    if (y >= 12 && y < 42) {
        if (x >= 200 && x < 200 + s->tab_count * 112) {
            s->active_tab = (x - 200) / 112;
            if (s->tabs[s->active_tab]) {
                s->cwd = s->tabs[s->active_tab];
                s->selected = NULLPTR;
            }
        } else if (s->tab_count < EXP_TABS && x >= 208 + s->tab_count * 112 && x < 236 + s->tab_count * 112) {
            s->tabs[s->tab_count++] = vfs_lookup("/Users/aurelia");
            s->active_tab = s->tab_count - 1;
            s->cwd = s->tabs[s->active_tab];
        }
        return;
    }

    /* Toolbar buttons */
    if (y >= 50 && y < 80) {
        if (x >= 200 && x < 252) {
            if (s->cwd && s->cwd->parent) {
                set_cwd(s, s->cwd->parent);
            }
            return;
        }
        if (x >= 260 && x < 368) {
            s->mode = 1;
            s->field_len = 0;
            s->field[0] = '\0';
            exp_status(s, "New folder name:");
            return;
        }
        if (x >= 376 && x < 468) {
            s->mode = 2;
            s->field_len = 0;
            s->field[0] = '\0';
            exp_status(s, "New file name:");
            return;
        }
        if (x >= 476 && x < 568 && s->selected) {
            if (vfs_delete(s->selected)) {
                s->selected = NULLPTR;
                exp_status(s, "Deleted.");
            } else {
                exp_status(s, "Delete failed: folder not empty.");
            }
            return;
        }
        if (x >= 576 && x < 668 && s->selected) {
            s->mode = 3;
            strncpy(s->field, s->selected->name, sizeof(s->field) - 1);
            s->field[sizeof(s->field) - 1] = '\0';
            s->field_len = (u32)strlen(s->field);
            exp_status(s, "Rename to:");
            return;
        }
    }

    /* File list rows */
    vfs_node_t *nodes[64];
    i32 count = list_rows(s, nodes, 64);
    i32 list_y = 118;
    for (i32 i = 0; i < count; i++) {
        i32 ry = list_y + i * 34;
        if (ui_hit(x, y, 188, ry, 1000, 32)) {
            bool double_click = s->last_click_node == nodes[i] &&
                                timer_ms() - s->last_click_ms < 420;
            s->selected = nodes[i];
            s->last_click_node = nodes[i];
            s->last_click_ms = timer_ms();
            if (double_click) {
                if (nodes[i]->is_dir) {
                    set_cwd(s, nodes[i]);
                } else {
                    char path[VFS_PATH_MAX];
                    vfs_absolute(nodes[i], path, sizeof(path));
                    desktop_open_file(path);
                }
                s->last_click_node = NULLPTR;
            }
            return;
        }
    }
}

static void exp_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    exp_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);

    /* Sidebar */
    canvas_fill(c, x, y, 172, ch, g_theme.dark ? 0x14151C : 0xF1EBDF);
    canvas_hline(c, x + 172, y, 1, g_theme.border, 160);
    ui_section(c, x + 16, y + 22, "PLACES");
    for (u32 i = 0; i < ARRAY_SIZE(k_places); i++) {
        i32 iy = y + 42 + (i32)i * 34;
        vfs_node_t *node = vfs_lookup(k_places[i].path);
        bool active = (node == s->cwd);
        ui_sidebar_item(c, x + 8, iy, 156, 30, k_places[i].label, active,
                        ui_hit(s->mx, s->my, 8, iy, 156, 30));
    }
    ui_section(c, x + 16, y + ch - 62, "STORAGE");
    u32 used = vfs_used_bytes();
    ui_progress(c, x + 16, y + ch - 42, 140, 6, (i32)(used / (256 * 1024)), g_theme.accent);
    char used_text[32];
    ksnprintf(used_text, sizeof(used_text), "%u KiB in RAM", used / 1024);
    font_draw(c, &font_ui_11, x + 16, y + ch - 28, used_text, g_theme.text_dim, 230);

    /* Tabs */
    for (i32 i = 0; i < s->tab_count; i++) {
        i32 tx = x + 200 + i * 112;
        bool active = (i == s->active_tab);
        canvas_fill_round_alpha(c, tx, y + 12, 106, 30, 8,
                                active ? g_theme.surface_hover : g_theme.surface, active ? 255 : 160);
        char label[24];
        vfs_node_t *tab = s->tabs[i];
        ksnprintf(label, sizeof(label), "%s", tab && tab->parent ? tab->name : "/");
        font_draw(c, &font_ui_11, tx + 12, y + 20, label, active ? g_theme.text : g_theme.text_dim, 255);
        if (active) {
            canvas_fill(c, tx + 8, y + 40, 90, 2, g_theme.accent);
        }
    }
    if (s->tab_count < EXP_TABS) {
        font_draw(c, &font_ui_15, x + 206 + s->tab_count * 112, y + 16, "+", g_theme.text_muted, 200);
    }

    /* Toolbar */
    i32 ty = y + 50;
    bool has_sel = s->selected != NULLPTR;
    ui_button(c, x + 200, ty, 52, 30, "Up", 0, ui_hit(s->mx, s->my, 200, ty, 52, 30));
    ui_button(c, x + 260, ty, 108, 30, "New folder", 0, ui_hit(s->mx, s->my, 260, ty, 108, 30));
    ui_button(c, x + 376, ty, 92, 30, "New file", 0, ui_hit(s->mx, s->my, 376, ty, 92, 30));
    ui_button(c, x + 476, ty, 92, 30, "Delete", has_sel ? 2 : 0,
              has_sel && ui_hit(s->mx, s->my, 476, ty, 92, 30));
    ui_button(c, x + 576, ty, 92, 30, "Rename", 0,
              has_sel && ui_hit(s->mx, s->my, 576, ty, 92, 30));

    /* Search */
    ui_field(c, x + cw - 232, ty, 200, 30, s->query, "Filter this folder...", true);
    font_draw(c, &font_ui_11, x + cw - 24, ty + 9, "F", g_theme.text_dim, 160);

    /* Path */
    char path[VFS_PATH_MAX];
    vfs_absolute(s->cwd, path, sizeof(path));
    font_draw(c, &font_ui_13, x + 196, y + 92, path, g_theme.text_muted, 255);
    ui_divider(c, x + 188, y + 112, cw - 200);

    /* File list */
    vfs_node_t *nodes[64];
    i32 count = list_rows(s, nodes, 64);
    i32 list_y = y + 118;
    s->row_at_mouse = -1;
    for (i32 i = 0; i < count; i++) {
        i32 ry = list_y + i * 34;
        if (ry + 34 > y + ch - 40) {
            break;
        }
        vfs_node_t *node = nodes[i];
        bool selected = (node == s->selected);
        bool hover = ui_hit(s->mx, s->my, 188, ry, cw - 196, 32) && !selected;
        if (selected) {
            canvas_fill_round(c, x + 188, ry, cw - 200, 32, 7, g_theme.dark ? 0x24252F : 0xEDE6D8);
            canvas_fill(c, x + 192, ry + 8, 2, 16, g_theme.accent);
        } else if (hover) {
            canvas_fill_round_alpha(c, x + 188, ry, cw - 200, 32, 7, g_theme.surface_hover, 100);
            s->row_at_mouse = i;
        }
        i32 icon_x = x + 204;
        if (node->is_dir) {
            canvas_fill_round_alpha(c, icon_x, ry + 9, 24, 15, 3, g_theme.accent, 190);
            canvas_fill_round_alpha(c, icon_x + 2, ry + 6, 10, 6, 2, g_theme.accent, 190);
        } else {
            canvas_fill_round_alpha(c, icon_x + 2, ry + 6, 18, 21, 2, 0x8C8A93, 190);
            canvas_fill(c, icon_x + 6, ry + 13, 10, 1, 0xE8E4DC);
            canvas_fill(c, icon_x + 6, ry + 17, 8, 1, 0xE8E4DC);
        }
        font_draw(c, &font_ui_13, x + 240, ry + 8, node->name,
                  selected ? g_theme.text : g_theme.text_muted, 255);
        if (node->is_dir) {
            char items[24];
            ksnprintf(items, sizeof(items), "%u items", vfs_child_count(node));
            font_draw(c, &font_ui_11, x + cw - 330, ry + 10, items, g_theme.text_dim, 200);
        } else {
            char size[24];
            if (node->size >= 1024) {
                ksnprintf(size, sizeof(size), "%u.%u KiB", node->size / 1024,
                          (node->size % 1024) * 10 / 1024);
            } else {
                ksnprintf(size, sizeof(size), "%u bytes", node->size);
            }
            font_draw(c, &font_ui_11, x + cw - 330, ry + 10, size, g_theme.text_dim, 200);
        }
        char modified[24];
        u64 secs = node->modified_tick / 1000;
        ksnprintf(modified, sizeof(modified), "%02llu:%02llu", (secs / 3600) % 24, (secs / 60) % 60);
        font_draw_right(c, &font_ui_11, x + cw - 20, ry + 10, modified, g_theme.text_dim, 170);
    }
    if (count == 0) {
        font_draw_centered(c, &font_ui_13, x + 188 + (cw - 188) / 2, list_y + 30,
                           s->query_len ? "No items match the filter." : "This folder is empty.",
                           g_theme.text_dim, 255);
    }

    /* Footer */
    canvas_hline(c, x, y + ch - 38, cw, g_theme.border, 140);
    if (s->mode != 0) {
        font_draw(c, &font_ui_11, x + 20, y + ch - 26, s->status, g_theme.accent, 255);
        ui_field(c, x + 220, y + ch - 32, 320, 26, s->field, "", true);
        font_draw(c, &font_ui_11, x + 552, y + ch - 26, "Enter to confirm, Esc to cancel",
                  g_theme.text_dim, 220);
    } else {
        char footer[128];
        if (s->selected) {
            ksnprintf(footer, sizeof(footer), "%s  -  %s  -  %u bytes",
                      s->selected->name, s->selected->is_dir ? "Folder" : "File",
                      s->selected->size);
        } else {
            ksnprintf(footer, sizeof(footer), "%d items  -  RAM filesystem", count);
        }
        font_draw(c, &font_ui_11, x + 196, y + ch - 26, footer, g_theme.text_dim, 235);
        if (s->status[0]) {
            font_draw_right(c, &font_ui_11, x + cw - 20, y + ch - 26, s->status,
                            g_theme.success, 220);
        }
    }
}

static void *exp_create(void)
{
    exp_state_t *s = kzalloc(sizeof(exp_state_t));
    if (!s) {
        return NULLPTR;
    }
    s->cwd = vfs_lookup("/Users/aurelia");
    s->tabs[0] = s->cwd;
    s->tab_count = 1;
    s->active_tab = 0;
    s->last_click_node = NULLPTR;
    exp_status(s, "Welcome to AURELIA Explorer");
    return s;
}

static const app_vtable_t explorer_vt = {
    .draw = exp_draw,
    .key = exp_key,
    .mouse = exp_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_explorer = {
    .id = "explorer",
    .name = "Explorer",
    .tagline = "Browse and manage the filesystem",
    .width = 1000,
    .height = 620,
    .tint = 0x8A6C3A,
    .vt = &explorer_vt,
    .create = exp_create,
};
