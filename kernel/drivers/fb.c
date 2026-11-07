/* AURELIA OS - framebuffer renderer (Limine linear framebuffer) */
#include <aurelia/fb.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>

fb_info_t g_fb;

bool fb_ready(void) { return g_fb.ready; }

static bool is_canonical(u64 address)
{
    u64 top = address >> 47;
    return top == 0 || top == 0x1FFFF;
}

bool fb_init(struct limine_framebuffer *framebuffer, u64 hhdm_offset)
{
    memset(&g_fb, 0, sizeof(g_fb));
    if (!framebuffer) {
        return false;
    }
    if (framebuffer->bpp != 32 || framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB) {
        kprintf("[fb] unsupported format: %u bpp, model %u\n",
                (u32)framebuffer->bpp, (u32)framebuffer->memory_model);
        return false;
    }

    u64 reported = (u64)framebuffer->address;
    /* Limine reports the framebuffer as a physical address in older protocol
       revisions, but newer ones hand out an address already inside the HHDM. */
    u64 virt = (reported >= hhdm_offset) ? reported : reported + hhdm_offset;
    kprintf("[fb] reported %#llx hhdm %#llx -> virtual %#llx\n",
            reported, hhdm_offset, virt);

    if (!is_canonical(virt) || virt < 0xFFFF800000000000ull) {
        kprintf("[fb] refusing non-canonical framebuffer mapping\n");
        return false;
    }

    g_fb.width = (u32)framebuffer->width;
    g_fb.height = (u32)framebuffer->height;
    g_fb.pitch_pixels = (u32)(framebuffer->pitch / 4);
    g_fb.bpp = framebuffer->bpp;
    g_fb.red_shift = framebuffer->red_mask_shift;
    g_fb.green_shift = framebuffer->green_mask_shift;
    g_fb.blue_shift = framebuffer->blue_mask_shift;
    g_fb.pixels = (u32 *)virt;
    g_fb.byte_size = framebuffer->pitch * framebuffer->height;
    g_fb.ready = true;
    return true;
}

static inline u32 fb_pack(u32 color)
{
    u32 r = (color >> 16) & 0xFF;
    u32 g = (color >> 8) & 0xFF;
    u32 b = color & 0xFF;
    return (r << g_fb.red_shift) | (g << g_fb.green_shift) | (b << g_fb.blue_shift);
}

void fb_clear(u32 color)
{
    if (!g_fb.ready) {
        return;
    }
    u32 packed = fb_pack(color);
    for (u32 y = 0; y < g_fb.height; y++) {
        u32 *row = g_fb.pixels + (u64)y * g_fb.pitch_pixels;
        for (u32 x = 0; x < g_fb.width; x++) {
            row[x] = packed;
        }
    }
}

void fb_pixel(i32 x, i32 y, u32 color)
{
    if (!g_fb.ready || x < 0 || y < 0 || (u32)x >= g_fb.width || (u32)y >= g_fb.height) {
        return;
    }
    g_fb.pixels[(u64)y * g_fb.pitch_pixels + (u32)x] = fb_pack(color);
}

void fb_blend_pixel(i32 x, i32 y, u32 color, u8 alpha)
{
    if (!g_fb.ready || x < 0 || y < 0 || (u32)x >= g_fb.width || (u32)y >= g_fb.height || alpha == 0) {
        return;
    }
    if (alpha == 255) {
        fb_pixel(x, y, color);
        return;
    }
    u32 *dst = &g_fb.pixels[(u64)y * g_fb.pitch_pixels + (u32)x];
    u32 sr = (color >> 16) & 0xFF;
    u32 sg = (color >> 8) & 0xFF;
    u32 sb = color & 0xFF;
    u32 dr = (*dst >> g_fb.red_shift) & 0xFF;
    u32 dg = (*dst >> g_fb.green_shift) & 0xFF;
    u32 db = (*dst >> g_fb.blue_shift) & 0xFF;
    u32 inv = 255 - alpha;
    u32 r = (sr * alpha + dr * inv) / 255;
    u32 g = (sg * alpha + dg * inv) / 255;
    u32 b = (sb * alpha + db * inv) / 255;
    *dst = (r << g_fb.red_shift) | (g << g_fb.green_shift) | (b << g_fb.blue_shift);
}

void fb_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color)
{
    if (!g_fb.ready || w <= 0 || h <= 0) {
        return;
    }
    i32 x0 = i32_max(x, 0);
    i32 y0 = i32_max(y, 0);
    i32 x1 = i32_min(x + w, (i32)g_fb.width);
    i32 y1 = i32_min(y + h, (i32)g_fb.height);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    u32 packed = fb_pack(color);
    for (i32 yy = y0; yy < y1; yy++) {
        u32 *row = g_fb.pixels + (u64)yy * g_fb.pitch_pixels;
        for (i32 xx = x0; xx < x1; xx++) {
            row[xx] = packed;
        }
    }
}

void fb_blend_rect(i32 x, i32 y, i32 w, i32 h, u32 color, u8 alpha)
{
    if (!g_fb.ready || w <= 0 || h <= 0 || alpha == 0) {
        return;
    }
    i32 x0 = i32_max(x, 0);
    i32 y0 = i32_max(y, 0);
    i32 x1 = i32_min(x + w, (i32)g_fb.width);
    i32 y1 = i32_min(y + h, (i32)g_fb.height);
    for (i32 yy = y0; yy < y1; yy++) {
        for (i32 xx = x0; xx < x1; xx++) {
            fb_blend_pixel(xx, yy, color, alpha);
        }
    }
}

static inline u32 lerp_color(u32 a, u32 b, u32 t, u32 max)
{
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

void fb_vgradient(i32 x, i32 y, i32 w, i32 h, u32 top, u32 bottom)
{
    if (!g_fb.ready || w <= 0 || h <= 0) {
        return;
    }
    for (i32 row = 0; row < h; row++) {
        u32 t = (u32)(row < 0 ? 0 : row);
        u32 color = lerp_color(top, bottom, t, (u32)(h > 1 ? h - 1 : 1));
        fb_fill_rect(x, y + row, w, 1, color);
    }
}

void fb_radial_glow(i32 cx, i32 cy, i32 radius, u32 color, u8 max_alpha)
{
    if (!g_fb.ready || radius <= 0) {
        return;
    }
    for (i32 dy = -radius; dy <= radius; dy += 2) {
        for (i32 dx = -radius; dx <= radius; dx += 2) {
            i32 d2 = dx * dx + dy * dy;
            if (d2 > radius * radius) {
                continue;
            }
            u32 dist = (u32)((d2 * 100) / (radius * radius));
            u8 alpha = (u8)((max_alpha * (100 - dist)) / 100);
            fb_blend_rect(cx + dx, cy + dy, 2, 2, color, alpha);
        }
    }
}

void fb_line(i32 x0, i32 y0, i32 x1, i32 y1, u32 color)
{
    i32 dx = x1 - x0;
    i32 dy = y1 - y0;
    i32 sx = dx > 0 ? 1 : -1;
    i32 sy = dy > 0 ? 1 : -1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    i32 err = dx - dy;
    for (;;) {
        fb_pixel(x0, y0, color);
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

void fb_line_blend(i32 x0, i32 y0, i32 x1, i32 y1, u32 color, u8 alpha)
{
    i32 dx = x1 - x0;
    i32 dy = y1 - y0;
    i32 sx = dx > 0 ? 1 : -1;
    i32 sy = dy > 0 ? 1 : -1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    i32 err = dx - dy;
    for (;;) {
        fb_blend_pixel(x0, y0, color, alpha);
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

void fb_ring(i32 cx, i32 cy, i32 radius, i32 thickness, u32 color)
{
    i32 inner = radius - thickness;
    if (inner < 0) {
        inner = 0;
    }
    for (i32 dy = -radius; dy <= radius; dy++) {
        for (i32 dx = -radius; dx <= radius; dx++) {
            i32 d2 = dx * dx + dy * dy;
            if (d2 <= radius * radius && d2 >= inner * inner) {
                fb_pixel(cx + dx, cy + dy, color);
            }
        }
    }
}

void fb_panic_screen(const char *message)
{
    (void)message;
    if (!g_fb.ready) {
        return;
    }
    /* Panic rendering is intentionally minimal: a solid tone, no fonts or heap. */
    fb_clear(0x160B0B);
    fb_fill_rect(0, 0, (i32)g_fb.width, 4, 0xD6B878);
}
