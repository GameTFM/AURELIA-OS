/* AURELIA OS - AURELIA Store
 *
 * Only lists applications that actually exist in this image. There is no
 * package service or download backend yet, so no install buttons are offered
 * for software that is not present.
 */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/desktop.h>

typedef struct {
    i32 selected;
    i32 mx, my;
    char query[32];
    u32 query_len;
} store_state_t;

static store_state_t *state_of(window_t *w) { return (store_state_t *)w->state; }

static const char *category_of(const char *id)
{
    if (strcmp(id, "terminal") == 0 || strcmp(id, "editor") == 0) return "Developer";
    if (strcmp(id, "explorer") == 0 || strcmp(id, "notes") == 0) return "Productivity";
    if (strcmp(id, "settings") == 0 || strcmp(id, "monitor") == 0 ||
        strcmp(id, "about") == 0 || strcmp(id, "logs") == 0) return "System";
    if (strcmp(id, "browser") == 0) return "Internet";
    if (strcmp(id, "player") == 0 || strcmp(id, "imageviewer") == 0) return "Media";
    if (strcmp(id, "clock") == 0 || strcmp(id, "calendar") == 0) return "Utilities";
    return "Utilities";
}

static bool matches(const app_desc_t *app, const char *query)
{
    if (!query || !*query) {
        return true;
    }
    char lower[64];
    strncpy(lower, app->name, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    str_tolower(lower);
    return strstr(lower, query) != NULLPTR;
}

static void store_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    store_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    i32 card_w = (w->w - 88) / 3;
    i32 start = 136;
    i32 index = 0;
    for (u32 i = 0; i < app_count(); i++) {
        const app_desc_t *app = app_at(i);
        if (!matches(app, s->query)) {
            continue;
        }
        i32 col = index % 3;
        i32 row = index / 3;
        i32 cx = 24 + col * (card_w + 16);
        i32 cy = start + row * 100;
        bool in_card = ui_hit(x, y, cx, cy, card_w, 96);
        if (in_card) {
            s->selected = (i32)i;
            if (x >= cx + card_w - 92) {
                desktop_launch_app(app->id);
            }
            return;
        }
        index++;
    }
}

static void store_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    store_state_t *s = state_of(w);
    if (event->key == KEY_BACKSPACE) {
        if (s->query_len > 0) {
            s->query[--s->query_len] = '\0';
        }
        return;
    }
    if (event->key == KEY_ENTER) {
        for (u32 i = 0; i < app_count(); i++) {
            if (matches(app_at(i), s->query)) {
                desktop_launch_app(app_at(i)->id);
                break;
            }
        }
        return;
    }
    if (event->key >= 32 && event->key < 127 && s->query_len < sizeof(s->query) - 1) {
        s->query[s->query_len++] = (char)event->key;
        s->query[s->query_len] = '\0';
        s->selected = -1;
    }
}

static void store_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    store_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    font_draw(c, &font_ui_20, x + 24, y + 20, "AURELIA Store", g_theme.text, 255);
    font_draw(c, &font_ui_11, x + 24, y + 48,
              "Built-in applications - every listing is installed and runnable",
              g_theme.text_muted, 235);
    ui_field(c, x + cw - 260, y + 20, 236, 34, s->query, "Search applications", true);

    i32 card_w = (cw - 88) / 3;
    i32 index = 0;
    for (u32 i = 0; i < app_count(); i++) {
        const app_desc_t *app = app_at(i);
        if (!matches(app, s->query)) {
            continue;
        }
        i32 col = index % 3;
        i32 row = index / 3;
        i32 cx = x + 24 + col * (card_w + 16);
        i32 cy = y + 136 + row * 100;
        if (cy + 96 > y + ch - 70) {
            break;
        }
        bool selected = (i32)i == s->selected;
        bool hover = ui_hit(s->mx, s->my, cx, cy, card_w, 96);
        canvas_fill_round_alpha(c, cx, cy, card_w, 96, 14,
                                selected || hover ? g_theme.surface_hover : g_theme.surface, 235);
        if (selected) {
            canvas_outline_round(c, cx, cy, card_w, 96, 14, 1, g_theme.accent, 140);
        }
        desktop_draw_app_icon(c, app, cx + 16, cy + 24, 48);
        font_draw(c, &font_ui_15, cx + 80, cy + 20, app->name, g_theme.text, 255);
        font_draw(c, &font_ui_11, cx + 80, cy + 44, category_of(app->id), g_theme.accent, 220);
        font_draw_ellipsized(c, &font_ui_11, cx + 80, cy + 62, card_w - 196, app->tagline,
                             g_theme.text_muted, 235);
        ui_button(c, cx + card_w - 88, cy + 56, 72, 26, "Open", 1, hover);
        index++;
    }
    if (index == 0) {
        font_draw_centered(c, &font_ui_13, x + cw / 2, y + 190, "No applications match.",
                           g_theme.text_muted, 255);
    }

    canvas_hline(c, x + 24, y + ch - 56, cw - 48, g_theme.border, 140);
    font_draw(c, &font_ui_11, x + 24, y + ch - 42,
              "Third-party package installation is not available: the package service and "
              "signature verification are milestone 9 work.",
              g_theme.text_dim, 230);
}

static void *store_create(void)
{
    store_state_t *s = kzalloc(sizeof(store_state_t));
    if (s) {
        s->selected = -1;
    }
    return s;
}

static const app_vtable_t store_vt = {
    .draw = store_draw,
    .key = store_key,
    .mouse = store_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_store = {
    .id = "store",
    .name = "Store",
    .tagline = "Browse the applications in this image",
    .width = 1180,
    .height = 760,
    .tint = 0x3E4A6A,
    .vt = &store_vt,
    .create = store_create,
};
