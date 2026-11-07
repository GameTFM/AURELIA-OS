/* AURELIA OS - Notes */
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

#define NOTES_DIR "/Users/aurelia/Documents/Notes"
#define NOTES_MAX 32
#define NOTES_BUFFER 4096

typedef struct {
    vfs_node_t *items[NOTES_MAX];
    i32 count;
    i32 selected;
    char buffer[NOTES_BUFFER];
    u32 length;
    bool dirty;
    char title[64];
    i32 mx, my;
} notes_state_t;

static notes_state_t *state_of(window_t *w) { return (notes_state_t *)w->state; }

static void notes_refresh(notes_state_t *s)
{
    s->count = 0;
    vfs_node_t *dir = vfs_lookup(NOTES_DIR);
    if (!dir) {
        return;
    }
    for (vfs_node_t *child = dir->children; child && s->count < NOTES_MAX; child = child->next) {
        if (!child->is_dir) {
            s->items[s->count++] = child;
        }
    }
    if (s->selected >= s->count) {
        s->selected = s->count - 1;
    }
}

static void notes_load(notes_state_t *s, i32 index)
{
    if (index < 0 || index >= s->count) {
        return;
    }
    s->selected = index;
    vfs_node_t *node = s->items[index];
    u32 len = node->size < NOTES_BUFFER - 1 ? node->size : NOTES_BUFFER - 1;
    if (node->data && len) {
        memcpy(s->buffer, node->data, len);
    }
    s->buffer[len] = '\0';
    s->length = len;
    s->dirty = false;
    strncpy(s->title, node->name, sizeof(s->title) - 1);
    s->title[sizeof(s->title) - 1] = '\0';
}

static void notes_save(notes_state_t *s)
{
    if (s->selected < 0 || s->selected >= s->count) {
        return;
    }
    vfs_write(s->items[s->selected], s->buffer, s->length);
    s->dirty = false;
}

static void notes_new(notes_state_t *s)
{
    char path[VFS_PATH_MAX];
    char name[32];
    for (i32 i = 1; i < 99; i++) {
        ksnprintf(name, sizeof(name), "note-%02d.md", i);
        ksnprintf(path, sizeof(path), NOTES_DIR "/%s", name);
        if (!vfs_lookup(path)) {
            break;
        }
    }
    vfs_node_t *node = vfs_create(path, false);
    if (!node) {
        return;
    }
    const char *seed = "# New note\n\n";
    vfs_write(node, seed, (u32)strlen(seed));
    notes_refresh(s);
    for (i32 i = 0; i < s->count; i++) {
        if (s->items[i] == node) {
            notes_load(s, i);
            break;
        }
    }
    s->dirty = true;
}

static void notes_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)released;
    notes_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    if (x >= 12 && x < 220) {
        if (y >= 68 && y < 100) {
            notes_new(s);
            return;
        }
        if (y >= 108 && y < 140) {
            if (s->selected >= 0 && s->selected < s->count) {
                vfs_delete(s->items[s->selected]);
                s->selected = -1;
                s->buffer[0] = '\0';
                s->length = 0;
                notes_refresh(s);
                if (s->count > 0) {
                    notes_load(s, 0);
                }
            }
            return;
        }
        i32 index = (y - 150) / 52;
        if (index >= 0 && index < s->count && y >= 150) {
            if (s->dirty) {
                notes_save(s);
            }
            notes_load(s, index);
        }
        return;
    }
    if (x > 232 && y > 54 && y < 96) {
        notes_save(s);
    }
}

static void notes_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    notes_state_t *s = state_of(w);
    if (event->mods & MOD_CTRL) {
        if (event->key == 's' || event->key == 'S') {
            notes_save(s);
        }
        if (event->key == 'n' || event->key == 'N') {
            notes_new(s);
        }
        return;
    }
    if (event->key == KEY_BACKSPACE) {
        if (s->length > 0) {
            s->buffer[--s->length] = '\0';
            s->dirty = true;
        }
        return;
    }
    if (event->key == KEY_ENTER) {
        if (s->length < NOTES_BUFFER - 1) {
            s->buffer[s->length++] = '\n';
            s->buffer[s->length] = '\0';
            s->dirty = true;
        }
        return;
    }
    if (event->key == KEY_TAB) {
        for (i32 i = 0; i < 2 && s->length < NOTES_BUFFER - 1; i++) {
            s->buffer[s->length++] = ' ';
        }
        s->buffer[s->length] = '\0';
        s->dirty = true;
        return;
    }
    if (event->key >= 32 && event->key < 127 && s->length < NOTES_BUFFER - 1) {
        s->buffer[s->length++] = (char)event->key;
        s->buffer[s->length] = '\0';
        s->dirty = true;
    }
}

static void notes_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    notes_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    canvas_fill(c, x, y, 220, ch, g_theme.dark ? 0x14151C : 0xF1EBDF);

    font_draw(c, &font_ui_20, x + 18, y + 20, "Notes", g_theme.text, 255);
    ui_button(c, x + 14, y + 68, 94, 30, "New", 1, ui_hit(s->mx, s->my, 14, y + 68, 94, 30));
    ui_button(c, x + 114, y + 68, 94, 30, "Delete", 2, ui_hit(s->mx, s->my, 114, y + 68, 94, 30));

    for (i32 i = 0; i < s->count; i++) {
        i32 iy = y + 150 + i * 52;
        if (iy + 48 > y + ch) {
            break;
        }
        vfs_node_t *node = s->items[i];
        bool active = i == s->selected;
        if (active) {
            canvas_fill_round(c, x + 10, iy, 200, 46, 9, g_theme.dark ? 0x24252F : 0xE8E1D4);
            canvas_fill(c, x + 10, iy + 12, 2, 22, g_theme.accent);
        } else if (ui_hit(s->mx, s->my, 10, iy, 200, 46)) {
            canvas_fill_round_alpha(c, x + 10, iy, 200, 46, 9, g_theme.surface_hover, 100);
        }
        font_draw_ellipsized(c, &font_ui_13, x + 22, iy + 8, 176, node->name,
                             active ? g_theme.text : g_theme.text_muted, 255);
        char meta[32];
        ksnprintf(meta, sizeof(meta), "%u bytes", node->size);
        font_draw(c, &font_ui_11, x + 22, iy + 26, meta, g_theme.text_dim, 220);
    }
    if (s->count == 0) {
        font_draw_wrapped(c, &font_ui_11, x + 20, y + 160, 180,
                          "No notes yet. Press New or Ctrl+N.", g_theme.text_dim, 235);
    }

    /* Editor */
    i32 ex = x + 236;
    i32 ey = y + 20;
    i32 ew = cw - 256;
    font_draw(c, &font_ui_17, ex, ey, s->title, g_theme.text, 255);
    ui_button(c, ex + ew - 90, ey - 2, 90, 30, s->dirty ? "Save *" : "Saved", 1,
              ui_hit(s->mx, s->my, ex + ew - 90, ey - 2, 90, 30));
    font_draw(c, &font_ui_11, ex, ey + 28, s->dirty ? "Unsaved changes - Ctrl+S" : "Saved to RAM filesystem",
              s->dirty ? g_theme.warning : g_theme.text_dim, 220);
    ui_divider(c, ex, ey + 52, ew);

    canvas_fill_round(c, ex, ey + 64, ew, ch - 88, 10, g_theme.dark ? 0x0D0E13 : 0xFFFFFF);
    font_draw_wrapped(c, &font_mono_13, ex + 18, ey + 80, ew - 40, s->buffer,
                      g_theme.text, 245);
    if ((timer_ms() / 500) % 2 == 0) {
        font_draw(c, &font_mono_13, ex + 18, ey + 80 + (int)(s->length / 80) * 16, "_",
                  g_theme.accent, 255);
    }
    font_draw_right(c, &font_ui_11, x + cw - 20, y + ch - 24, "Ctrl+S save  -  Ctrl+N new",
                    g_theme.text_dim, 200);
}

static void *notes_create(void)
{
    notes_state_t *s = kzalloc(sizeof(notes_state_t));
    if (!s) {
        return NULLPTR;
    }
    s->selected = -1;
    notes_refresh(s);
    if (s->count > 0) {
        notes_load(s, 0);
    } else {
        strncpy(s->title, "New note", sizeof(s->title) - 1);
    }
    return s;
}

static const app_vtable_t notes_vt = {
    .draw = notes_draw,
    .key = notes_key,
    .mouse = notes_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_notes = {
    .id = "notes",
    .name = "Notes",
    .tagline = "Write notes that survive the session",
    .width = 920,
    .height = 560,
    .tint = 0x9A7B46,
    .vt = &notes_vt,
    .create = notes_create,
};
