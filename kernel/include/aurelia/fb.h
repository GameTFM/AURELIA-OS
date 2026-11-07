/* AURELIA OS - framebuffer graphics primitives */
#pragma once

#include <aurelia/types.h>
#include <aurelia/limine.h>

typedef struct {
    u32 width;
    u32 height;
    u32 pitch_pixels;
    u32 bpp;
    u8  red_shift;
    u8  green_shift;
    u8  blue_shift;
    u32 *pixels;
    u64 byte_size;
    bool ready;
} fb_info_t;

extern fb_info_t g_fb;

bool fb_init(struct limine_framebuffer *framebuffer, u64 hhdm_offset);
bool fb_ready(void);

static inline u32 fb_width(void) { return g_fb.width; }
static inline u32 fb_height(void) { return g_fb.height; }

void fb_clear(u32 color);
void fb_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color);
void fb_blend_rect(i32 x, i32 y, i32 w, i32 h, u32 color, u8 alpha);
void fb_vgradient(i32 x, i32 y, i32 w, i32 h, u32 top, u32 bottom);
void fb_radial_glow(i32 cx, i32 cy, i32 radius, u32 color, u8 max_alpha);
void fb_pixel(i32 x, i32 y, u32 color);
void fb_blend_pixel(i32 x, i32 y, u32 color, u8 alpha);
void fb_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 color);
void fb_line_blend(i32 x0, i32 y0, i32 x1, i32 y1, u32 color, u8 alpha);
void fb_ring(i32 cx, i32 cy, i32 radius, i32 thickness, u32 color);
void fb_panic_screen(const char *message);
