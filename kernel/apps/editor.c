/* AURELIA OS - Text Editor */
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

#define ED_MAX 8192
#define ED_LINES 24

typedef struct {
    char path[VFS_PATH_MAX];
    char buffer[ED_MAX];
    u32 length;
    u32 caret;
    i32 scroll_line;
    bool dirty;
    char status[64];
    i32 mx, my;
} editor_state_t;

static editor_state_t *state_of(window_t *w) { return (editor_state_t *)w->state; }

static void editor_pick_file(editor_state_t *s, const char *path)
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node || node->is_dir) {
        ksnprintf(s->status, sizeof(s->status), "Cannot open %s", path ? path : "(null)");
        return;
    }
    strncpy(s->path, path, sizeof(s->path) - 1);
    s->path[sizeof(s->path) - 1] = '\0';
    u32 len = node->size < ED_MAX - 1 ? node->size : ED_MAX - 1;
    if (node->data && len) {
        memcpy(s->buffer, node->data, len);
    }
    s->buffer[len] = '\0';
    s->length = len;
    s->caret = 0;
    s->scroll_line = 0;
    s->dirty = false;
    ksnprintf(s->status, sizeof(s->status), "Opened %s", path);
}

static void editor_save(editor_state_t *s)
{
    if (!s->path[0]) {
        return;
    }
    vfs_node_t *node = vfs_lookup(s->path);
    if (!node) {
        node = vfs_create(s->path, false);
    }
    if (node && vfs_write(node, s->buffer, s->length)) {
        s->dirty = false;
        ksnprintf(s->status, sizeof(s->status), "Saved %u bytes", s->length);
    } else {
        ksnprintf(s->status, sizeof(s->status), "Save failed");
    }
}

static void editor_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    editor_state_t *s = state_of(w);
    if (event->mods & MOD_CTRL) {
        if (event->key == 's' || event->key == 'S') {
            editor_save(s);
        }
        return;
    }
    if (event->key == KEY_BACKSPACE) {
        if (s->caret > 0) {
            memmove(s->buffer + s->caret - 1, s->buffer + s->caret, s->length - s->caret);
            s->caret--;
            s->length--;
            s->dirty = true;
        }
        return;
    }
    if (event->key == KEY_DELETE) {
        if (s->caret < s->length) {
            memmove(s->buffer + s->caret, s->buffer + s->caret + 1, s->length - s->caret - 1);
            s->length--;
            s->dirty = true;
        }
        return;
    }
    if (event->key == KEY_LEFT && s->caret > 0) {
        s->caret--;
        return;
    }
    if (event->key == KEY_RIGHT && s->caret < s->length) {
        s->caret++;
        return;
    }
    if (event->key == KEY_HOME) {
        while (s->caret > 0 && s->buffer[s->caret - 1] != '\n') {
            s->caret--;
        }
        return;
    }
    if (event->key == KEY_END) {
        while (s->caret < s->length && s->buffer[s->caret] != '\n') {
            s->caret++;
        }
        return;
    }
    if (event->key == KEY_ENTER) {
        if (s->length < ED_MAX - 1) {
            memmove(s->buffer + s->caret + 1, s->buffer + s->caret, s->length - s->caret);
            s->buffer[s->caret] = '\n';
            s->caret++;
            s->length++;
            s->dirty = true;
        }
        return;
    }
    if (event->key == KEY_TAB) {
        for (i32 i = 0; i < 4 && s->length < ED_MAX - 1; i++) {
            memmove(s->buffer + s->caret + 1, s->buffer + s->caret, s->length - s->caret);
            s->buffer[s->caret++] = ' ';
            s->length++;
        }
        s->dirty = true;
        return;
    }
    if (event->key >= 32 && event->key < 127 && s->length < ED_MAX - 1) {
        memmove(s->buffer + s->caret + 1, s->buffer + s->caret, s->length - s->caret);
        s->buffer[s->caret++] = (char)event->key;
        s->length++;
        s->dirty = true;
    }
}

static i32 line_of(const editor_state_t *s)
{
    i32 line = 0;
    for (u32 i = 0; i < s->caret && i < s->length; i++) {
        if (s->buffer[i] == '\n') {
            line++;
        }
    }
    return line;
}

static void editor_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    editor_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x0D0E13 : 0xFFFFFF);

    font_draw(c, &font_ui_11, x + 18, y + 12, s->path[0] ? s->path : "No file",
              g_theme.text_muted, 255);
    if (s->dirty) {
        canvas_circle(c, x + cw - 30, y + 18, 3, g_theme.warning, 255);
    }
    ui_button(c, x + cw - 118, y + 8, 92, 26, "Save", 1,
              ui_hit(s->mx, s->my, x + cw - 118, y + 8, 92, 26));
    ui_divider(c, x, y + 40, cw);

    i32 line_y = y + 52;
    i32 lh = font_line_height(&font_mono_13) + 2;
    i32 caret_line = line_of(s);
    i32 start_line = s->scroll_line;
    if (caret_line < start_line) {
        start_line = caret_line;
    }
    if (caret_line >= start_line + ED_LINES) {
        start_line = caret_line - ED_LINES + 1;
    }
    s->scroll_line = start_line;

    i32 line = 0;
    u32 pos = 0;
    while (pos <= s->length && line < start_line) {
        if (s->buffer[pos] == '\n') {
            line++;
        }
        pos++;
    }

    for (i32 i = 0; i < ED_LINES && line < start_line + ED_LINES; i++) {
        i32 ly = line_y + i * lh;
        char num[8];
        ksnprintf(num, sizeof(num), "%3d", line + 1);
        font_draw(c, &font_mono_13, x + 14, ly, num, g_theme.text_dim, 160);
        canvas_hline(c, x + 52, ly - 2, cw - 64, 0xFFFFFF, 8);

        char text[128];
        u32 len = 0;
        while (pos + len < s->length && s->buffer[pos + len] != '\n' && len < sizeof(text) - 1) {
            text[len] = s->buffer[pos + len];
            len++;
        }
        text[len] = '\0';
        font_draw(c, &font_mono_13, x + 62, ly, text, g_theme.text, 245);
        if (line == caret_line && (timer_ms() / 500) % 2 == 0) {
            u32 col = s->caret - pos;
            if (col > len) {
                col = len;
            }
            char prefix[128];
            memcpy(prefix, text, col);
            prefix[col] = '\0';
            i32 caret_x = x + 62 + font_text_width(&font_mono_13, prefix);
            canvas_fill(c, caret_x, ly + 1, 2, lh - 3, g_theme.accent);
        }
        pos += len;
        if (s->buffer[pos] == '\n') {
            pos++;
        }
        line++;
    }

    /* Scrollbar */
    i32 total_lines = 1;
    for (u32 i = 0; i < s->length; i++) {
        if (s->buffer[i] == '\n') {
            total_lines++;
        }
    }
    if (total_lines > ED_LINES) {
        i32 track = ch - 90;
        i32 thumb = i32_max(20, track * ED_LINES / total_lines);
        i32 ty = y + 48 + track * start_line / total_lines;
        canvas_fill_round(c, x + cw - 8, ty, 3, thumb, 2, g_theme.surface_hover);
    }

    canvas_hline(c, x, y + ch - 28, cw, g_theme.border, 140);
    char status[160];
    ksnprintf(status, sizeof(status), "Ln %d  Col %u  -  %u bytes  -  %s",
              caret_line + 1, s->caret + 1, s->length, s->status);
    font_draw(c, &font_ui_11, x + 16, y + ch - 19, status, g_theme.text_dim, 235);
    font_draw_right(c, &font_ui_11, x + cw - 16, y + ch - 19, "Ctrl+S to save",
                    g_theme.text_dim, 200);
}

static void *editor_create(void)
{
    editor_state_t *s = kzalloc(sizeof(editor_state_t));
    if (!s) {
        return NULLPTR;
    }
    const char *pending = desktop_take_pending_file();
    if (pending && *pending) {
        editor_pick_file(s, pending);
    } else {
        strncpy(s->path, "/Users/aurelia/Documents/welcome.txt", sizeof(s->path) - 1);
        editor_pick_file(s, s->path);
    }
    return s;
}

static void editor_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    editor_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (pressed && x > w->w - 130 && y < 40) {
        editor_save(s);
    }
}

static const app_vtable_t editor_vt = {
    .draw = editor_draw,
    .key = editor_key,
    .mouse = editor_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_editor = {
    .id = "editor",
    .name = "Text Editor",
    .tagline = "Edit plain text files",
    .width = 920,
    .height = 600,
    .tint = 0x4A4C57,
    .vt = &editor_vt,
    .create = editor_create,
};
