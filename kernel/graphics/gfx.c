/* AURELIA OS - canvas drawing primitives (software renderer) */
#include <aurelia/gfx.h>
#include <aurelia/fb.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>

canvas_t g_screen;

void gfx_init(void)
{
    canvas_bind_screen(&g_screen);
}

void canvas_bind(canvas_t *c, u32 *pixels, i32 width, i32 height, i32 stride)
{
    c->pixels = pixels;
    c->width = width;
    c->height = height;
    c->stride = stride;
    c->clip_x = 0;
    c->clip_y = 0;
    c->clip_w = width;
    c->clip_h = height;
}

void canvas_bind_screen(canvas_t *c)
{
    canvas_bind(c, g_fb.pixels, (i32)g_fb.width, (i32)g_fb.height, (i32)g_fb.pitch_pixels);
}

void canvas_clip(canvas_t *c, i32 x, i32 y, i32 w, i32 h)
{
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > c->width) {
        w = c->width - x;
    }
    if (y + h > c->height) {
        h = c->height - y;
    }
    if (w < 0) {
        w = 0;
    }
    if (h < 0) {
        h = 0;
    }
    c->clip_x = x;
    c->clip_y = y;
    c->clip_w = w;
    c->clip_h = h;
}

void canvas_clip_reset(canvas_t *c)
{
    c->clip_x = 0;
    c->clip_y = 0;
    c->clip_w = c->width;
    c->clip_h = c->height;
}

void canvas_clip_intersect(canvas_t *c, i32 x, i32 y, i32 w, i32 h)
{
    i32 x0 = i32_max(c->clip_x, x);
    i32 y0 = i32_max(c->clip_y, y);
    i32 x1 = i32_min(c->clip_x + c->clip_w, x + w);
    i32 y1 = i32_min(c->clip_y + c->clip_h, y + h);
    c->clip_x = x0;
    c->clip_y = y0;
    c->clip_w = i32_max(0, x1 - x0);
    c->clip_h = i32_max(0, y1 - y0);
}

u32 gfx_pack(u32 rgb)
{
    u32 r = (rgb >> 16) & 0xFF;
    u32 g = (rgb >> 8) & 0xFF;
    u32 b = rgb & 0xFF;
    return (r << g_fb.red_shift) | (g << g_fb.green_shift) | (b << g_fb.blue_shift);
}

u32 gfx_unpack(u32 native)
{
    u32 r = (native >> g_fb.red_shift) & 0xFF;
    u32 g = (native >> g_fb.green_shift) & 0xFF;
    u32 b = (native >> g_fb.blue_shift) & 0xFF;
    return (r << 16) | (g << 8) | b;
}

u32 gfx_mix(u32 a, u32 b, u32 t, u32 max)
{
    if (max == 0) {
        return a;
    }
    i32 ar = (i32)((a >> 16) & 0xFF), ag = (i32)((a >> 8) & 0xFF), ab = (i32)(a & 0xFF);
    i32 br = (i32)((b >> 16) & 0xFF), bg = (i32)((b >> 8) & 0xFF), bb = (i32)(b & 0xFF);
    i32 r = ar + ((br - ar) * (i32)t) / (i32)max;
    i32 g = ag + ((bg - ag) * (i32)t) / (i32)max;
    i32 bl = ab + ((bb - ab) * (i32)t) / (i32)max;
    r = i32_clamp(r, 0, 255);
    g = i32_clamp(g, 0, 255);
    bl = i32_clamp(bl, 0, 255);
    return ((u32)r << 16) | ((u32)g << 8) | (u32)bl;
}

u32 gfx_lighten(u32 color, u32 amount)
{
    return gfx_mix(color, 0xFFFFFF, i32_min((i32)amount, 255), 255);
}

u32 gfx_darken(u32 color, u32 amount)
{
    return gfx_mix(color, 0x000000, i32_min((i32)amount, 255), 255);
}

u32 gfx_alpha(u32 color, u8 alpha)
{
    (void)alpha;
    return color;
}

void canvas_pixel(canvas_t *c, i32 x, i32 y, u32 color)
{
    if (x < c->clip_x || y < c->clip_y || x >= c->clip_x + c->clip_w || y >= c->clip_y + c->clip_h) {
        return;
    }
    c->pixels[(u64)y * c->stride + x] = gfx_pack(color);
}

void canvas_blend_pixel(canvas_t *c, i32 x, i32 y, u32 color, u8 alpha)
{
    if (x < c->clip_x || y < c->clip_y || x >= c->clip_x + c->clip_w || y >= c->clip_y + c->clip_h) {
        return;
    }
    if (alpha == 0) {
        return;
    }
    u32 *dst = &c->pixels[(u64)y * c->stride + x];
    if (alpha == 255) {
        *dst = gfx_pack(color);
        return;
    }
    u32 current = gfx_unpack(*dst);
    u32 mixed = gfx_mix(current, color, alpha, 255);
    *dst = gfx_pack(mixed);
}

void canvas_clear(canvas_t *c, u32 color)
{
    canvas_fill(c, c->clip_x, c->clip_y, c->clip_w, c->clip_h, color);
}

void canvas_fill(canvas_t *c, i32 x, i32 y, i32 w, i32 h, u32 color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    i32 x0 = i32_max(x, c->clip_x);
    i32 y0 = i32_max(y, c->clip_y);
    i32 x1 = i32_min(x + w, c->clip_x + c->clip_w);
    i32 y1 = i32_min(y + h, c->clip_y + c->clip_h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    u32 packed = gfx_pack(color);
    for (i32 yy = y0; yy < y1; yy++) {
        u32 *row = c->pixels + (u64)yy * c->stride;
        for (i32 xx = x0; xx < x1; xx++) {
            row[xx] = packed;
        }
    }
}

void canvas_fill_alpha(canvas_t *c, i32 x, i32 y, i32 w, i32 h, u32 color, u8 alpha)
{
    if (alpha == 255) {
        canvas_fill(c, x, y, w, h, color);
        return;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    i32 x0 = i32_max(x, c->clip_x);
    i32 y0 = i32_max(y, c->clip_y);
    i32 x1 = i32_min(x + w, c->clip_x + c->clip_w);
    i32 y1 = i32_min(y + h, c->clip_y + c->clip_h);
    for (i32 yy = y0; yy < y1; yy++) {
        for (i32 xx = x0; xx < x1; xx++) {
            canvas_blend_pixel(c, xx, yy, color, alpha);
        }
    }
}

static inline bool inside_round(i32 px, i32 py, i32 x, i32 y, i32 w, i32 h, i32 radius)
{
    if (px < x || py < y || px >= x + w || py >= y + h) {
        return false;
    }
    if (radius <= 0) {
        return true;
    }
    i32 cx;
    i32 cy;
    if (px < x + radius) {
        cx = x + radius;
    } else if (px >= x + w - radius) {
        cx = x + w - radius - 1;
    } else {
        return true;
    }
    if (py < y + radius) {
        cy = y + radius;
    } else if (py >= y + h - radius) {
        cy = y + h - radius - 1;
    } else {
        return true;
    }
    i32 dx = px - cx;
    i32 dy = py - cy;
    return dx * dx + dy * dy <= radius * radius;
}

void canvas_fill_round_alpha(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                             u32 color, u8 alpha)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    i32 r = i32_min(radius, i32_min(w, h) / 2);
    i32 x0 = i32_max(x, c->clip_x);
    i32 y0 = i32_max(y, c->clip_y);
    i32 x1 = i32_min(x + w, c->clip_x + c->clip_w);
    i32 y1 = i32_min(y + h, c->clip_y + c->clip_h);
    for (i32 yy = y0; yy < y1; yy++) {
        for (i32 xx = x0; xx < x1; xx++) {
            if (!inside_round(xx, yy, x, y, w, h, r)) {
                continue;
            }
            if (alpha == 255) {
                c->pixels[(u64)yy * c->stride + xx] = gfx_pack(color);
            } else {
                canvas_blend_pixel(c, xx, yy, color, alpha);
            }
        }
    }
}

void canvas_fill_round(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius, u32 color)
{
    canvas_fill_round_alpha(c, x, y, w, h, radius, color, 255);
}

void canvas_fill_round_vgrad(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                             u32 top, u32 bottom)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    i32 r = i32_min(radius, i32_min(w, h) / 2);
    for (i32 row = 0; row < h; row++) {
        u32 color = gfx_mix(top, bottom, (u32)row, (u32)(h > 1 ? h - 1 : 1));
        i32 yy = y + row;
        if (yy < c->clip_y || yy >= c->clip_y + c->clip_h) {
            continue;
        }
        for (i32 col = 0; col < w; col++) {
            if (!inside_round(x + col, yy, x, y, w, h, r)) {
                continue;
            }
            canvas_pixel(c, x + col, yy, color);
        }
    }
}

void canvas_outline_round(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                          i32 thickness, u32 color, u8 alpha)
{
    if (w <= 0 || h <= 0 || thickness <= 0) {
        return;
    }
    i32 r = i32_min(radius, i32_min(w, h) / 2);
    for (i32 yy = y; yy < y + h; yy++) {
        for (i32 xx = x; xx < x + w; xx++) {
            if (!inside_round(xx, yy, x, y, w, h, r)) {
                continue;
            }
            bool inner = inside_round(xx, yy, x + thickness, y + thickness,
                                      w - thickness * 2, h - thickness * 2,
                                      i32_max(0, r - thickness));
            if (!inner) {
                canvas_blend_pixel(c, xx, yy, color, alpha);
            }
        }
    }
}

void canvas_vgradient(canvas_t *c, i32 x, i32 y, i32 w, i32 h, u32 top, u32 bottom)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    for (i32 row = 0; row < h; row++) {
        u32 color = gfx_mix(top, bottom, (u32)row, (u32)(h > 1 ? h - 1 : 1));
        canvas_fill(c, x, y + row, w, 1, color);
    }
}

void canvas_hline(canvas_t *c, i32 x, i32 y, i32 w, u32 color, u8 alpha)
{
    canvas_fill_alpha(c, x, y, w, 1, color, alpha);
}

void canvas_line(canvas_t *c, i32 x0, i32 y0, i32 x1, i32 y1, u32 color, u8 alpha)
{
    i32 dx = x1 - x0;
    i32 dy = y1 - y0;
    i32 sx = dx > 0 ? 1 : -1;
    i32 sy = dy > 0 ? 1 : -1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    i32 err = dx - dy;
    for (;;) {
        canvas_blend_pixel(c, x0, y0, color, alpha);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        i32 e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void canvas_circle(canvas_t *c, i32 cx, i32 cy, i32 radius, u32 color, u8 alpha)
{
    for (i32 dy = -radius; dy <= radius; dy++) {
        for (i32 dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                canvas_blend_pixel(c, cx + dx, cy + dy, color, alpha);
            }
        }
    }
}

void canvas_ring(canvas_t *c, i32 cx, i32 cy, i32 radius, i32 thickness, u32 color, u8 alpha)
{
    i32 inner = radius - thickness;
    if (inner < 0) {
        inner = 0;
    }
    for (i32 dy = -radius; dy <= radius; dy++) {
        for (i32 dx = -radius; dx <= radius; dx++) {
            i32 d2 = dx * dx + dy * dy;
            if (d2 <= radius * radius && d2 >= inner * inner) {
                canvas_blend_pixel(c, cx + dx, cy + dy, color, alpha);
            }
        }
    }
}

void canvas_arc(canvas_t *c, i32 cx, i32 cy, i32 radius, i32 thickness,
                i32 start_deg, i32 end_deg, u32 color, u8 alpha)
{
    /* Integer sine table for 0..360 degrees. */
    static const i16 sin_table[91] = {
        0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259,
        276, 292, 309, 326, 342, 358, 375, 391, 407, 423, 438, 454, 469, 485, 500,
        515, 530, 545, 559, 574, 588, 602, 616, 629, 643, 656, 669, 682, 695, 707,
        719, 731, 743, 755, 766, 777, 788, 799, 809, 819, 829, 839, 848, 857, 866,
        875, 883, 891, 899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966,
        970, 974, 978, 982, 985, 988, 990, 993, 995, 996, 998, 999, 999, 1000,
        1000, 1000
    };
    i32 inner = radius - thickness;
    if (inner < 0) {
        inner = 0;
    }
    i32 start = ((start_deg % 360) + 360) % 360;
    i32 end = ((end_deg % 360) + 360) % 360;
    for (i32 dy = -radius; dy <= radius; dy++) {
        for (i32 dx = -radius; dx <= radius; dx++) {
            i32 d2 = dx * dx + dy * dy;
            if (d2 > radius * radius || d2 < inner * inner) {
                continue;
            }
            /* angle of this pixel, 0 = +x axis, clockwise */
            i32 angle = 0;
            i32 ax = dx;
            i32 ay = -dy;
            i32 adx = ax < 0 ? -ax : ax;
            i32 ady = ay < 0 ? -ay : ay;
            if (adx >= ady) {
                angle = (ay >= 0) ? (adx ? (i32)(45 - (45 * ady) / adx) : 0)
                                  : -(adx ? (i32)(45 - (45 * ady) / adx) : 0);
            } else {
                angle = (ax >= 0) ? (i32)(45 + (45 * adx) / ady)
                                  : (i32)(135 - (45 * adx) / ady);
            }
            while (angle < 0) {
                angle += 360;
            }
            bool in_arc = (start <= end) ? (angle >= start && angle <= end)
                                         : (angle >= start || angle <= end);
            if (in_arc) {
                canvas_blend_pixel(c, cx + dx, cy + dy, color, alpha);
            }
            (void)sin_table;
        }
    }
}

void canvas_shadow(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius,
                   i32 spread, u8 strength)
{
    if (spread <= 0) {
        return;
    }
    i32 r = i32_min(radius, i32_min(w, h) / 2);
    i32 x0 = i32_max(x - spread, c->clip_x);
    i32 y0 = i32_max(y - spread, c->clip_y);
    i32 x1 = i32_min(x + w + spread, c->clip_x + c->clip_w);
    i32 y1 = i32_min(y + h + spread, c->clip_y + c->clip_h);
    for (i32 yy = y0; yy < y1; yy++) {
        for (i32 xx = x0; xx < x1; xx++) {
            if (yy >= y && yy < y + h && xx >= x && xx < x + w &&
                inside_round(xx, yy, x, y, w, h, r)) {
                continue; /* window body covers its own shadow */
            }
            i32 dxx = 0;
            i32 dyy = 0;
            if (xx < x) {
                dxx = x - xx;
            } else if (xx >= x + w) {
                dxx = xx - (x + w - 1);
            }
            if (yy < y) {
                dyy = y - yy;
            } else if (yy >= y + h) {
                dyy = yy - (y + h - 1);
            }
            i32 dist = (dxx > dyy) ? dxx : dyy;
            if (dist > spread) {
                continue;
            }
            u32 falloff = (u32)((spread - dist) * (spread - dist) * strength) /
                          (u32)(spread * spread);
            canvas_blend_pixel(c, xx, yy, 0x000000, (u8)i32_min((i32)falloff, 255));
        }
    }
}

void canvas_blit(canvas_t *c, const u32 *src, i32 src_w, i32 src_h, i32 dx, i32 dy)
{
    i32 x0 = i32_max(dx, c->clip_x);
    i32 y0 = i32_max(dy, c->clip_y);
    i32 x1 = i32_min(dx + src_w, c->clip_x + c->clip_w);
    i32 y1 = i32_min(dy + src_h, c->clip_y + c->clip_h);
    for (i32 yy = y0; yy < y1; yy++) {
        u32 *dst_row = c->pixels + (u64)yy * c->stride;
        const u32 *src_row = src + (u64)(yy - dy) * src_w;
        memcpy(dst_row + x0, src_row + (x0 - dx), (size_t)(x1 - x0) * sizeof(u32));
    }
}

void canvas_blit_scaled(canvas_t *c, const u32 *src, i32 src_w, i32 src_h,
                        i32 dx, i32 dy, i32 dw, i32 dh)
{
    if (dw <= 0 || dh <= 0) {
        return;
    }
    for (i32 yy = dy; yy < dy + dh; yy++) {
        i32 sy = (i32)(((i64)(yy - dy) * src_h) / dh);
        for (i32 xx = dx; xx < dx + dw; xx++) {
            i32 sx = (i32)(((i64)(xx - dx) * src_w) / dw);
            if (xx < c->clip_x || yy < c->clip_y ||
                xx >= c->clip_x + c->clip_w || yy >= c->clip_y + c->clip_h) {
                continue;
            }
            c->pixels[(u64)yy * c->stride + xx] = src[(u64)sy * src_w + sx];
        }
    }
}
