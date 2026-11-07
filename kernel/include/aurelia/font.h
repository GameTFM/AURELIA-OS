/* AURELIA OS - font rendering over canvases */
#pragma once

#include <aurelia/types.h>
#include <aurelia/gfx.h>
#include <generated/fonts.h>

i32  font_text_width(const font_t *font, const char *text);
i32  font_char_width(const font_t *font, u32 codepoint);
i32  font_height(const font_t *font);
i32  font_line_height(const font_t *font);
const glyph_t *font_lookup(const font_t *font, u32 codepoint);

i32  font_draw(canvas_t *c, const font_t *font, i32 x, i32 y, const char *text,
               u32 color, u8 alpha);
i32  font_draw_wrapped(canvas_t *c, const font_t *font, i32 x, i32 y, i32 max_width,
                       const char *text, u32 color, u8 alpha);
i32  font_draw_centered(canvas_t *c, const font_t *font, i32 cx, i32 y,
                        const char *text, u32 color, u8 alpha);
i32  font_draw_right(canvas_t *c, const font_t *font, i32 right_x, i32 y,
                     const char *text, u32 color, u8 alpha);
i32  font_draw_ellipsized(canvas_t *c, const font_t *font, i32 x, i32 y, i32 max_width,
                          const char *text, u32 color, u8 alpha);
