/* AURELIA OS - window manager */
#pragma once

#include <aurelia/types.h>
#include <aurelia/app.h>
#include <aurelia/gfx.h>

#define WM_MAX_WINDOWS 16
#define WM_TITLEBAR_H  44
#define WM_MIN_WIDTH   360
#define WM_MIN_HEIGHT  240

struct window {
    bool used;
    u32 id;
    const app_desc_t *app;
    void *state;
    char title[96];

    i32 x, y, w, h;
    i32 rx, ry, rw, rh;      /* restore geometry after maximize/snap */

    bool minimized;
    bool maximized;
    u32 z;
    u32 workspace;

    i32 drag_mode;           /* 0 none, 1 move, 2 resize */
    i32 resize_edges;        /* bit 0 left, 1 right, 2 top, 3 bottom */
    i32 grab_dx, grab_dy;
};

void wm_init(void);
window_t *wm_open(const app_desc_t *app);
void wm_close_window(window_t *w);
void wm_focus(window_t *w);
void wm_set_title(window_t *w, const char *title);
window_t *wm_focused(void);
window_t *wm_find_by_app(const char *id);
u32 wm_count(void);
window_t *wm_window_at(u32 index);        /* z-order, bottom first */
u32 wm_count_on_workspace(u32 workspace);
window_t *wm_window_on_workspace(u32 workspace, u32 index);

void wm_draw(canvas_t *c);
bool wm_handle_key(const key_event_t *event);
bool wm_handle_mouse(i32 x, i32 y, u8 buttons, bool pressed, bool released,
                     bool double_click);

void wm_minimize(window_t *w);
void wm_toggle_maximize(window_t *w);
void wm_snap(window_t *w, i32 zone);       /* 0 left, 1 right, 2 maximize */
void wm_cycle(void);
void wm_close_focused(void);
void wm_minimize_focused(void);

void wm_tick_all(void);
void wm_set_workspace(u32 workspace);
u32 wm_workspace(void);
void wm_minimize_all(void);
void wm_restore_all(void);
bool wm_any_maximized(void);
bool wm_is_focused(window_t *w);
