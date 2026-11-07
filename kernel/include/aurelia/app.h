/* AURELIA OS - application model
 *
 * Applications are in-kernel modules for the alpha. Each provides a vtable and
 * draws into the window content rectangle it is handed. This is the same
 * contract a future user-mode ELF application will use over the syscall ABI.
 */
#pragma once

#include <aurelia/types.h>
#include <aurelia/gfx.h>
#include <aurelia/input.h>

typedef struct window window_t;

typedef struct app_vtable {
    /* Draw at (x,y) with content size (w,h). The canvas clip is set by the WM. */
    void (*draw)(window_t *win, canvas_t *c, i32 x, i32 y, i32 w, i32 h);
    void (*key)(window_t *win, const key_event_t *event);
    /* Local content coordinates; pressed/released describe button transitions. */
    void (*mouse)(window_t *win, i32 x, i32 y, u8 buttons, bool pressed, bool released);
    void (*close)(window_t *win);
    void (*tick)(window_t *win);
} app_vtable_t;

typedef struct app_desc {
    const char *id;
    const char *name;
    const char *tagline;
    i32 width;
    i32 height;
    u32 tint;
    const app_vtable_t *vt;
    void *(*create)(void);
} app_desc_t;

u32 app_count(void);
const app_desc_t *app_at(u32 index);
const app_desc_t *app_find(const char *id);
