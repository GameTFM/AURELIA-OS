/* AURELIA OS - 2D graphics: clipped canvas over the framebuffer */
#pragma once

#include <aurelia/types.h>

#define GFX_RGB(r, g, b) (((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))

typedef struct {
    u32 *pixels;
    i32 width;
    i32 height;
    i32 stride;
    i32 clip_x, clip_y, clip_w, clip_h;
} canvas_t;

extern canvas_t g_screen;

void gfx_init(void);
void canvas_bind(canvas_t *c, u32 *pixels, i32 width, i32 height, i32 stride);
void canvas_bind_screen(canvas_t *c);
void canvas_clip(canvas_t *c, i32 x, i32 y, i32 w, i32 h);
void canvas_clip_reset(canvas_t *c);
void canvas_clip_intersect(canvas_t *c, i32 x, i32 y, i32 w, i32 h);

u32  gfx_pack(u32 rgb);
u32  gfx_unpack(u32 native);
u32  gfx_mix(u32 a, u32 b, u32 t, u32 max);
u32  gfx_lighten(u32 color, u32 amount);
u32  gfx_darken(u32 color, u32 amount);
u32  gfx_alpha(u32 color, u8 alpha);

void canvas_clear(canvas_t *c, u32 color);
void canvas_pixel(canvas_t *c, i32 x, i32 y, u32 color);
void canvas_blend_pixel(canvas_t *c, i32 x, i32 y, u32 color, u8 alpha);
void canvas_fill(canvas_t *c, i32 x, i32 y, i32 w, i32 h, u32 color);
void canvas_fill_alpha(canvas_t *c, i32 x, i32 y, i32 w, i32 h, u32 color, u8 alpha);
void canvas_fill_round(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius, u32 color);
void canvas_fill_round_alpha(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                             u32 color, u8 alpha);
void canvas_fill_round_vgrad(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                             u32 top, u32 bottom);
void canvas_outline_round(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                          i32 thickness, u32 color, u8 alpha);
void canvas_vgradient(canvas_t *c, i32 x, i32 y, i32 w, i32 h, u32 top, u32 bottom);
void canvas_hline(canvas_t *c, i32 x, i32 y, i32 w, u32 color, u8 alpha);
void canvas_line(canvas_t *c, i32 x0, i32 y0, i32 x1, i32 y1, u32 color, u8 alpha);
void canvas_circle(canvas_t *c, i32 cx, i32 cy, i32 radius, u32 color, u8 alpha);
void canvas_ring(canvas_t *c, i32 cx, i32 cy, i32 radius, i32 thickness, u32 color, u8 alpha);
void canvas_arc(canvas_t *c, i32 cx, i32 cy, i32 radius, i32 thickness,
                i32 start_deg, i32 end_deg, u32 color, u8 alpha);
void canvas_shadow(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                   i32 spread, u8 strength);
void canvas_blit(canvas_t *c, const u32 *src, i32 src_w, i32 src_h, i32 dx, i32 dy);
void canvas_blit_scaled(canvas_t *c, const u32 *src, i32 src_w, i32 src_h,
                        i32 dx, i32 dy, i32 dw, i32 dh);
