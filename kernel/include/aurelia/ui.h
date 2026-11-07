/* AURELIA OS - shared UI widgets (drawn with the canvas primitives) */
#pragma once

#include <aurelia/types.h>
#include <aurelia/gfx.h>
#include <aurelia/font.h>

bool ui_hit(i32 px, i32 py, i32 x, i32 y, i32 w, i32 h);
void ui_panel(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius);
void ui_panel_flat(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius);
void ui_divider(canvas_t *c, i32 x, i32 y, i32 w);
void ui_section(canvas_t *c, i32 x, i32 y, const char *label);
void ui_button(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const char *label, i32 style,
               bool hover);
void ui_toggle(canvas_t *c, i32 x, i32 y, bool on);
void ui_progress(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 percent, u32 color);
void ui_field(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const char *text,
              const char *placeholder, bool focused);
void ui_sidebar_item(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const char *label,
                     bool active, bool hover);
void ui_swatch(canvas_t *c, i32 cx, i32 cy, i32 radius, u32 color, bool selected);
void ui_tag(canvas_t *c, i32 x, i32 y, const char *label);
void ui_header(canvas_t *c, i32 x, i32 y, i32 w, const char *title,
               const char *subtitle);
