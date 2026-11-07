/* AURELIA OS - Boot and diagnostics log viewer */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/theme.h>
#include <aurelia/ui.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/timer.h>

#define LOG_VIEW_SIZE 4096

typedef struct {
    char text[LOG_VIEW_SIZE];
    size_t length;
    i32 scroll;
    u64 refreshed;
    i32 mx, my;
} logs_state_t;

static logs_state_t *state_of(window_t *w) { return (logs_state_t *)w->state; }

static void logs_refresh(logs_state_t *s)
{
    s->length = klog_read(s->text, sizeof(s->text) - 1);
    if (s->length >= sizeof(s->text)) {
        s->length = sizeof(s->text) - 1;
    }
    s->text[s->length] = '\0';
    s->refreshed = timer_ms();
}

static void logs_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    logs_state_t *s = state_of(w);
    if (event->key == KEY_UP) {
        s->scroll = i32_max(0, s->scroll - 1);
    } else if (event->key == KEY_DOWN) {
        s->scroll++;
    } else if (event->key == KEY_PAGE_UP) {
        s->scroll = i32_max(0, s->scroll - 12);
    } else if (event->key == KEY_PAGE_DOWN) {
        s->scroll += 12;
    } else if (event->key == KEY_HOME) {
        s->scroll = 0;
    } else if (event->key == KEY_END) {
        s->scroll = 9999;
    } else if (event->key == 'r' || event->key == 'R') {
        logs_refresh(s);
    }
}

static void logs_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    logs_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x0D0E13 : 0xFFFFFF);
    font_draw(c, &font_ui_17, x + 20, y + 14, "Boot & Diagnostics", g_theme.text, 255);
    font_draw_right(c, &font_ui_11, x + cw - 20, y + 20,
                    "Serial console mirror - COM1 115200 baud", g_theme.text_dim, 220);
    ui_divider(c, x, y + 44, cw);

    i32 lh = font_line_height(&font_mono_13) + 1;
    i32 rows = (ch - 90) / lh;
    i32 total = 0;
    for (size_t i = 0; i < s->length; i++) {
        if (s->text[i] == '\n') {
            total++;
        }
    }
    i32 max_scroll = i32_max(0, total - rows);
    if (s->scroll > max_scroll) {
        s->scroll = max_scroll;
    }
    /* Skip to the first visible line. */
    size_t pos = 0;
    for (i32 line = 0; line < s->scroll && pos < s->length; line++) {
        while (pos < s->length && s->text[pos] != '\n') {
            pos++;
        }
        if (pos < s->length) {
            pos++;
        }
    }
    i32 ry = y + 58;
    for (i32 i = 0; i < rows && pos < s->length; i++) {
        char line[160];
        size_t len = 0;
        while (pos < s->length && s->text[pos] != '\n' && len < sizeof(line) - 1) {
            line[len++] = s->text[pos++];
        }
        line[len] = '\0';
        if (pos < s->length && s->text[pos] == '\n') {
            pos++;
        }
        u32 color = 0xB9B5AE;
        if (strstr(line, "[ok]")) {
            color = 0x9EC9B4;
        } else if (strstr(line, "warn") || strstr(line, "fail")) {
            color = 0xE2B86A;
        } else if (strstr(line, "panic")) {
            color = 0xD97070;
        }
        font_draw(c, &font_mono_13, x + 20, ry, line, color, 245);
        ry += lh;
    }

    canvas_hline(c, x, y + ch - 30, cw, g_theme.border, 130);
    char status[128];
    ksnprintf(status, sizeof(status), "%d lines  -  %zu bytes buffered  -  Up/Down scroll  -  R refresh",
              total, s->length);
    font_draw(c, &font_ui_11, x + 20, y + ch - 22, status, g_theme.text_dim, 225);
}

static void *logs_create(void)
{
    logs_state_t *s = kzalloc(sizeof(logs_state_t));
    if (s) {
        logs_refresh(s);
    }
    return s;
}

static void logs_tick(window_t *w)
{
    logs_state_t *s = state_of(w);
    if (timer_ms() - s->refreshed > 2000) {
        logs_refresh(s);
    }
}

static void logs_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    logs_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (pressed) {
        s->scroll += 3;
    }
}

static const app_vtable_t logs_vt = {
    .draw = logs_draw,
    .key = logs_key,
    .mouse = logs_mouse,
    .close = NULLPTR,
    .tick = logs_tick,
};

const app_desc_t app_logs = {
    .id = "logs",
    .name = "Diagnostics",
    .tagline = "Live kernel log and boot trace",
    .width = 980,
    .height = 620,
    .tint = 0x2A2B33,
    .vt = &logs_vt,
    .create = logs_create,
};
