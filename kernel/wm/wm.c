/* AURELIA OS - window manager
 *
 * Owns window geometry, z-order, focus, chrome rendering and pointer
 * interaction. Applications only see their content rectangle.
 */
#include <aurelia/wm.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/input.h>

static window_t g_windows[WM_MAX_WINDOWS];
static u32 g_next_id = 1;
static u32 g_next_z = 1;
static u32 g_workspace = 0;
static window_t *g_drag_window;
static i32 g_drag_mode;
static i32 g_snap_zone = -1;
static i32 g_mouse_x;
static i32 g_mouse_y;

static const u32 k_close_color = 0xD97070;
static const u32 k_min_color = 0xE2B86A;
static const u32 k_max_color = 0x86C9A4;

static window_t *focused_window(void)
{
    window_t *best = NULLPTR;
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        window_t *w = &g_windows[i];
        if (!w->used || w->minimized || w->workspace != g_workspace) {
            continue;
        }
        if (!best || w->z > best->z) {
            best = w;
        }
    }
    return best;
}

window_t *wm_focused(void) { return focused_window(); }
u32 wm_workspace(void) { return g_workspace; }

bool wm_is_focused(window_t *w) { return w && focused_window() == w; }

static window_t *alloc_window(void)
{
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].used) {
            return &g_windows[i];
        }
    }
    return NULLPTR;
}

void wm_init(void)
{
    memset(g_windows, 0, sizeof(g_windows));
    g_next_id = 1;
    g_next_z = 1;
    g_workspace = 0;
    g_drag_window = NULLPTR;
    g_drag_mode = 0;
    g_snap_zone = -1;
    kprintf("[ok] window manager ready\n");
}

window_t *wm_open(const app_desc_t *app)
{
    if (!app) {
        return NULLPTR;
    }
    window_t *w = alloc_window();
    if (!w) {
        return NULLPTR;
    }
    memset(w, 0, sizeof(*w));
    w->used = true;
    w->id = g_next_id++;
    w->app = app;
    w->workspace = g_workspace;
    w->z = ++g_next_z;
    strncpy(w->title, app->name, sizeof(w->title) - 1);

    i32 width = app->width;
    i32 height = app->height;
    if (width > (i32)g_screen.width - 80) {
        width = (i32)g_screen.width - 80;
    }
    if (height > (i32)g_screen.height - 150) {
        height = (i32)g_screen.height - 150;
    }
    i32 cascade = (i32)((w->id - 1) % 5) * 28;
    w->w = width;
    w->h = height;
    w->x = (i32)g_screen.width / 2 - width / 2 + cascade - 56;
    w->y = 92 + cascade;
    if (w->y + height > (i32)g_screen.height - 120) {
        w->y = (i32)g_screen.height - 120 - height;
    }
    if (w->y < 54) {
        w->y = 54;
    }
    w->rx = w->x;
    w->ry = w->y;
    w->rw = w->w;
    w->rh = w->h;

    if (app->create) {
        w->state = app->create();
    }
    kprintf("[wm] opened %s (id %u)\n", app->name, w->id);
    return w;
}

void wm_close_window(window_t *w)
{
    if (!w || !w->used) {
        return;
    }
    if (w->app->vt->close) {
        w->app->vt->close(w);
    }
    if (w->state) {
        kfree(w->state);
    }
    if (g_drag_window == w) {
        g_drag_window = NULLPTR;
        g_drag_mode = 0;
    }
    kprintf("[wm] closed %s\n", w->title);
    memset(w, 0, sizeof(*w));
}

void wm_focus(window_t *w)
{
    if (!w || !w->used) {
        return;
    }
    if (w->minimized) {
        w->minimized = false;
    }
    w->z = ++g_next_z;
}

void wm_set_title(window_t *w, const char *title)
{
    if (!w || !title) {
        return;
    }
    strncpy(w->title, title, sizeof(w->title) - 1);
    w->title[sizeof(w->title) - 1] = '\0';
}

u32 wm_count(void) { return WM_MAX_WINDOWS; }
window_t *wm_window_at(u32 index) { return index < WM_MAX_WINDOWS ? &g_windows[index] : NULLPTR; }

u32 wm_count_on_workspace(u32 workspace)
{
    u32 count = 0;
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].workspace == workspace) {
            count++;
        }
    }
    return count;
}

window_t *wm_window_on_workspace(u32 workspace, u32 index)
{
    u32 seen = 0;
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].workspace == workspace) {
            if (seen == index) {
                return &g_windows[i];
            }
            seen++;
        }
    }
    return NULLPTR;
}

window_t *wm_find_by_app(const char *id)
{
    if (!id) {
        return NULLPTR;
    }
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].used && strcmp(g_windows[i].app->id, id) == 0) {
            return &g_windows[i];
        }
    }
    return NULLPTR;
}

void wm_minimize(window_t *w)
{
    if (!w || !w->used) {
        return;
    }
    w->minimized = true;
    w->drag_mode = 0;
}

void wm_toggle_maximize(window_t *w)
{
    if (!w || !w->used) {
        return;
    }
    if (w->maximized) {
        w->x = w->rx;
        w->y = w->ry;
        w->w = w->rw;
        w->h = w->rh;
        w->maximized = false;
    } else {
        w->rx = w->x;
        w->ry = w->y;
        w->rw = w->w;
        w->rh = w->h;
        w->x = 0;
        w->y = 38;
        w->w = (i32)g_screen.width;
        w->h = (i32)g_screen.height - 38 - 96;
        w->maximized = true;
    }
}

void wm_snap(window_t *w, i32 zone)
{
    if (!w || !w->used) {
        return;
    }
    if (zone == 2) {
        if (!w->maximized) {
            wm_toggle_maximize(w);
        }
        return;
    }
    w->x = (zone == 0) ? 0 : (i32)g_screen.width / 2;
    w->y = 38;
    w->w = (i32)g_screen.width / 2;
    w->h = (i32)g_screen.height - 38 - 96;
    w->maximized = true;
}

void wm_cycle(void)
{
    window_t *lowest = NULLPTR;
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        window_t *w = &g_windows[i];
        if (!w->used || w->minimized || w->workspace != g_workspace) {
            continue;
        }
        if (!lowest || w->z < lowest->z) {
            lowest = w;
        }
    }
    if (!lowest || lowest == focused_window()) {
        return;
    }
    lowest->z = ++g_next_z;
}

void wm_tick_all(void)
{
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        window_t *w = &g_windows[i];
        if (w->used && w->app->vt->tick) {
            w->app->vt->tick(w);
        }
    }
}

void wm_close_focused(void)
{
    window_t *w = focused_window();
    if (w) {
        wm_close_window(w);
    }
}

void wm_minimize_focused(void)
{
    window_t *w = focused_window();
    if (w) {
        wm_minimize(w);
    }
}

void wm_set_workspace(u32 workspace)
{
    if (workspace > 3) {
        return;
    }
    g_workspace = workspace;
    g_drag_mode = 0;
    g_drag_window = NULLPTR;
}

void wm_minimize_all(void)
{
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].workspace == g_workspace) {
            g_windows[i].minimized = true;
        }
    }
}

void wm_restore_all(void)
{
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].workspace == g_workspace) {
            g_windows[i].minimized = false;
        }
    }
}

bool wm_any_maximized(void)
{
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].maximized && !g_windows[i].minimized) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------- */

static void draw_traffic_light(canvas_t *c, i32 cx, i32 cy, u32 color, bool hovered)
{
    canvas_circle(c, cx, cy, hovered ? 8 : 7, color, hovered ? 255 : 225);
}

static void draw_chrome(window_t *w, canvas_t *c, bool focused)
{
    i32 x = w->x;
    i32 y = w->y;
    i32 ww = w->w;
    i32 hh = w->h;

    canvas_shadow(c, x, y, ww, hh, 14, focused ? 30 : 18, focused ? 130 : 90);

    canvas_fill_round(c, x, y, ww, hh, 14, g_theme.surface);
    canvas_clip_intersect(c, x, y, ww, hh);
    canvas_fill_round_vgrad(c, x, y, ww, WM_TITLEBAR_H + 14, 14,
                            focused ? 0x1D1E27 : 0x191A22, g_theme.surface);

    /* Titlebar controls: close, minimize, maximize */
    i32 cy = y + WM_TITLEBAR_H / 2;
    bool over_close = i32_abs(g_mouse_x - (x + 26)) <= 9 && i32_abs(g_mouse_y - cy) <= 9;
    bool over_min = i32_abs(g_mouse_x - (x + 52)) <= 9 && i32_abs(g_mouse_y - cy) <= 9;
    bool over_max = i32_abs(g_mouse_x - (x + 78)) <= 9 && i32_abs(g_mouse_y - cy) <= 9;
    draw_traffic_light(c, x + 26, cy, k_close_color, over_close);
    draw_traffic_light(c, x + 52, cy, k_min_color, over_min);
    draw_traffic_light(c, x + 78, cy, k_max_color, over_max);
    if (over_close) {
        canvas_line(c, x + 23, cy - 3, x + 29, cy + 3, 0x3A1414, 220);
        canvas_line(c, x + 29, cy - 3, x + 23, cy + 3, 0x3A1414, 220);
    }

    font_draw_centered(c, &font_ui_13, x + ww / 2, y + 14, w->title,
                       focused ? g_theme.text : g_theme.text_muted, 255);
    canvas_hline(c, x + 1, y + WM_TITLEBAR_H, ww - 2, 0xFFFFFF, focused ? 26 : 14);

    /* Content */
    i32 cx = x + 1;
    i32 cycontent = y + WM_TITLEBAR_H;
    i32 cw = ww - 2;
    i32 ch = hh - WM_TITLEBAR_H - 1;
    canvas_fill(c, cx, cycontent, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    if (w->app->vt->draw) {
        canvas_clip_intersect(c, cx, cycontent, cw, ch);
        w->app->vt->draw(w, c, cx, cycontent, cw, ch);
    }

    canvas_clip_intersect(c, x, y, ww, hh);
    canvas_outline_round(c, x, y, ww, hh, 14, 1, focused ? g_theme.accent : 0xFFFFFF,
                         focused ? 70 : 28);
}

static bool window_visible(window_t *w)
{
    return w->used && !w->minimized && w->workspace == g_workspace;
}

void wm_draw(canvas_t *c)
{
    /* Draw in ascending z. */
    window_t *order[WM_MAX_WINDOWS];
    u32 count = 0;
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        if (window_visible(&g_windows[i])) {
            order[count++] = &g_windows[i];
        }
    }
    for (u32 i = 0; i < count; i++) {
        for (u32 j = i + 1; j < count; j++) {
            if (order[j]->z < order[i]->z) {
                window_t *tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }
    for (u32 i = 0; i < count; i++) {
        canvas_clip_reset(c);
        draw_chrome(order[i], c, order[i] == focused_window());
    }
    canvas_clip_reset(c);

    if (g_drag_mode == 1 && g_snap_zone >= 0) {
        i32 px = 0, py = 38, pw = (i32)c->width, ph = (i32)c->height - 38 - 96;
        if (g_snap_zone == 0) {
            pw = (i32)c->width / 2;
        } else if (g_snap_zone == 1) {
            px = (i32)c->width / 2;
            pw = (i32)c->width / 2;
        }
        canvas_fill_round_alpha(c, px + 6, py + 6, pw - 12, ph - 12, 16,
                                g_theme.accent, 38);
        canvas_outline_round(c, px + 6, py + 6, pw - 12, ph - 12, 16, 2,
                             g_theme.accent, 120);
    }
}

/* ------------------------------------------------------------------------- */
/* Input                                                                     */
/* ------------------------------------------------------------------------- */

static window_t *top_at(i32 x, i32 y, bool content_only)
{
    window_t *best = NULLPTR;
    for (u32 i = 0; i < WM_MAX_WINDOWS; i++) {
        window_t *w = &g_windows[i];
        if (!window_visible(w)) {
            continue;
        }
        if (x < w->x || y < w->y || x >= w->x + w->w || y >= w->y + w->h) {
            continue;
        }
        if (content_only && y < w->y + WM_TITLEBAR_H) {
            continue;
        }
        if (!best || w->z > best->z) {
            best = w;
        }
    }
    return best;
}

static i32 edge_at(window_t *w, i32 x, i32 y)
{
    if (w->maximized) {
        return 0;
    }
    const i32 m = 6;
    i32 edges = 0;
    if (x >= w->x - 2 && x <= w->x + m) edges |= 1;
    if (x >= w->x + w->w - m && x <= w->x + w->w + 2) edges |= 2;
    if (y >= w->y - 2 && y <= w->y + m) edges |= 4;
    if (y >= w->y + w->h - m && y <= w->y + w->h + 2) edges |= 8;
    if (y >= w->y + WM_TITLEBAR_H) {
        /* do not start top/bottom resize inside the content area */
        edges &= ~12;
    }
    return edges;
}

static void start_move(window_t *w, i32 x, i32 y)
{
    if (w->maximized) {
        i32 nx = x - w->w / 2;
        w->x = nx;
        w->y = y - WM_TITLEBAR_H / 2;
        w->maximized = false;
        w->w = w->rw;
        w->h = w->rh;
    }
    w->drag_mode = 1;
    w->grab_dx = x - w->x;
    w->grab_dy = y - w->y;
    g_drag_window = w;
    g_drag_mode = 1;
    g_snap_zone = -1;
}

bool wm_handle_mouse(i32 x, i32 y, u8 buttons, bool pressed, bool released,
                     bool double_click)
{
    g_mouse_x = x;
    g_mouse_y = y;
    bool dirty = false;

    if (g_drag_window && g_drag_mode == 1 && (buttons & 1)) {
        window_t *w = g_drag_window;
        w->x = x - w->grab_dx;
        w->y = i32_max(38, y - w->grab_dy);
        if (w->y <= 42) {
            g_snap_zone = 2;
        } else if (x <= 8) {
            g_snap_zone = 0;
        } else if (x >= (i32)g_screen.width - 8) {
            g_snap_zone = 1;
        } else {
            g_snap_zone = -1;
        }
        return true;
    }
    if (g_drag_window && g_drag_mode == 2 && (buttons & 1)) {
        window_t *w = g_drag_window;
        i32 right = w->x + w->w;
        i32 bottom = w->y + w->h;
        if (w->resize_edges & 1) {
            i32 nx = i32_min(x, right - WM_MIN_WIDTH);
            w->w = right - nx;
            w->x = nx;
        }
        if (w->resize_edges & 2) {
            w->w = i32_max(WM_MIN_WIDTH, x - w->x);
        }
        if (w->resize_edges & 4) {
            i32 ny = i32_min(y, bottom - WM_MIN_HEIGHT);
            w->h = bottom - ny;
            w->y = i32_max(38, ny);
        }
        if (w->resize_edges & 8) {
            w->h = i32_max(WM_MIN_HEIGHT, y - w->y);
        }
        return true;
    }

    window_t *w = top_at(x, y, false);
    if (pressed && w) {
        wm_focus(w);
        dirty = true;
        bool in_title = y < w->y + WM_TITLEBAR_H;
        i32 cy = w->y + WM_TITLEBAR_H / 2;
        if (in_title) {
            if (i32_abs(x - (w->x + 26)) <= 10 && i32_abs(y - cy) <= 10) {
                wm_close_window(w);
                return true;
            }
            if (i32_abs(x - (w->x + 52)) <= 10 && i32_abs(y - cy) <= 10) {
                wm_minimize(w);
                return true;
            }
            if (i32_abs(x - (w->x + 78)) <= 10 && i32_abs(y - cy) <= 10) {
                wm_toggle_maximize(w);
                return true;
            }
            if (double_click) {
                wm_toggle_maximize(w);
                return true;
            }
            start_move(w, x, y);
            return true;
        }
        i32 edges = edge_at(w, x, y);
        if (edges) {
            w->drag_mode = 2;
            w->resize_edges = edges;
            g_drag_window = w;
            g_drag_mode = 2;
            return true;
        }
        if (w->app->vt->mouse) {
            w->app->vt->mouse(w, x - w->x - 1, y - w->y - WM_TITLEBAR_H,
                              buttons, true, false);
        }
        return true;
    }

    if (released) {
        if (g_drag_window && g_drag_mode == 1) {
            if (g_snap_zone >= 0) {
                wm_snap(g_drag_window, g_snap_zone);
            }
            g_drag_window->drag_mode = 0;
            g_drag_window = NULLPTR;
            g_drag_mode = 0;
            g_snap_zone = -1;
            return true;
        }
        if (g_drag_window && g_drag_mode == 2) {
            g_drag_window->drag_mode = 0;
            g_drag_window = NULLPTR;
            g_drag_mode = 0;
            return true;
        }
        window_t *under = top_at(x, y, true);
        if (under && under->app->vt->mouse) {
            under->app->vt->mouse(under, x - under->x - 1, y - under->y - WM_TITLEBAR_H,
                                  buttons, false, true);
            return true;
        }
    }

    /* Hover routing to the focused window for interactive controls. */
    if (!pressed && !released) {
        window_t *under = top_at(x, y, true);
        if (under && under->app->vt->mouse) {
            under->app->vt->mouse(under, x - under->x - 1, y - under->y - WM_TITLEBAR_H,
                                  buttons, false, false);
        }
    }
    return dirty;
}

bool wm_handle_key(const key_event_t *event)
{
    window_t *w = focused_window();
    if (!w) {
        return false;
    }
    if (w->app->vt->key) {
        w->app->vt->key(w, event);
        return true;
    }
    return false;
}
