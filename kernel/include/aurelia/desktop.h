/* AURELIA OS - desktop shell */
#pragma once

#include <aurelia/types.h>
#include <aurelia/gfx.h>
#include <aurelia/app.h>

void desktop_init(void);
void desktop_run(void) NORETURN;

bool desktop_launch_app(const char *id);
bool desktop_launch_app_instance(const char *id);

void desktop_notify(const char *title, const char *body);
void desktop_open_file(const char *path);
const char *desktop_take_pending_file(void);
void desktop_draw_app_icon(canvas_t *c, const app_desc_t *app, i32 x, i32 y, i32 size);
void desktop_lock(void);
void desktop_shutdown(void) NORETURN;
void desktop_restart(void) NORETURN;

/* Shared helpers for applications. */
u32 desktop_window_count(void);
u32 desktop_appearance_index(void);
void desktop_cycle_appearance(void);
