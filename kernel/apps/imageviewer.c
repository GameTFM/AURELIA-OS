/* AURELIA OS - Image Viewer
 *
 * Renders AURELIA's generated artwork directly with the compositor. There is no
 * image codec or disk format support in this build, which the footer states
 * plainly instead of implying JPEG/PNG support.
 */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/heap.h>

typedef struct {
    i32 index;
    i32 mx, my;
} image_state_t;

static image_state_t *state_of(window_t *w) { return (image_state_t *)w->state; }

static const char *const k_titles[4] = {
    "Solstice", "Monolith", "Dune", "Aurora",
};

static void draw_art(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 index)
{
    if (index == 0) {
        canvas_vgradient(c, x, y, w, h, 0x0B0C12, 0x1A140B);
        canvas_circle(c, x + w * 3 / 4, y + h / 2, h / 2, 0x1B1509, 180);
        canvas_ring(c, x + w * 3 / 4, y + h / 2, h / 3, 1, 0x8A7642, 170);
        canvas_ring(c, x + w * 3 / 4, y + h / 2, h / 4, 1, 0xB39A5E, 140);
        canvas_circle(c, x + w * 3 / 4, y + h / 2, h / 9, 0xD6B878, 60);
        for (i32 i = 0; i < 40; i++) {
            canvas_pixel(c, x + (i * 173 + 31) % w, y + (i * 97 + 11) % h, 0x8A8266);
        }
    } else if (index == 1) {
        canvas_vgradient(c, x, y, w, h, 0x16171D, 0x090A0E);
        for (i32 i = 0; i < 12; i++) {
            canvas_fill_alpha(c, x + w / 8 + i * 12, y + 30, 2, h - 60,
                              0xFFFFFF, (u8)(6 + i * 2));
        }
        canvas_ring(c, x + w / 2, y + h + 40, h / 2 + 90, 2, 0xD6B878, 150);
        canvas_line(c, x + 30, y + h * 3 / 4, x + w - 30, y + h / 3, 0xD6B878, 60);
    } else if (index == 2) {
        canvas_vgradient(c, x, y, w, h, 0xF7F2E9, 0xD8CEBD);
        canvas_circle(c, x + w * 2 / 3, y + h / 3, h / 4, 0xEFE3C8, 210);
        for (i32 i = 0; i < 6; i++) {
            canvas_fill_round_alpha(c, x - 40 + i * 70, y + h * 2 / 3 + i * 12, w + 80, 90,
                                    60, i % 2 ? 0xE6DCCB : 0xC9BDA6, 180);
        }
        canvas_hline(c, x, y + h / 2, w, 0xFFFFFF, 80);
    } else {
        canvas_vgradient(c, x, y, w, h, 0x07120F, 0x0A0B10);
        canvas_circle(c, x + w / 2, y + h + 120, h / 2 + 140, 0x10251F, 220);
        canvas_arc(c, x + w / 2, y + h + 130, h / 2 + 120, 3, 195, 345, 0x7FC9A8, 150);
        canvas_arc(c, x + w / 2, y + h + 130, h / 2 + 60, 1, 200, 340, 0xB9E4CC, 110);
        for (i32 i = 0; i < 32; i++) {
            canvas_pixel(c, x + (i * 251 + 17) % w, y + (i * 61 + 7) % h, 0x9FB7AE);
        }
    }
}

static void image_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    image_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    if (y >= w->h - 70) {
        if (x < 90) {
            s->index = (s->index + 3) % 4;
        } else if (x < 180) {
            s->index = (s->index + 1) % 4;
        }
    }
    for (i32 i = 0; i < 4; i++) {
        i32 tx = w->w - 4 * 44 - 20 + i * 44;
        if (x >= tx && x < tx + 36 && y >= w->h - 64 && y < w->h - 28) {
            s->index = i;
        }
    }
}

static void image_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    image_state_t *s = state_of(w);
    if (event->key == KEY_LEFT) {
        s->index = (s->index + 3) % 4;
    } else if (event->key == KEY_RIGHT) {
        s->index = (s->index + 1) % 4;
    }
}

static void image_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    image_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    font_draw(c, &font_ui_17, x + 20, y + 14, k_titles[s->index], g_theme.text, 255);
    font_draw_right(c, &font_ui_11, x + cw - 20, y + 20, "GENERATED ARTWORK", g_theme.text_dim, 220);

    i32 view_x = x + 20;
    i32 view_y = y + 46;
    i32 view_w = cw - 40;
    i32 view_h = ch - 156;
    canvas_fill_round(c, view_x - 2, view_y - 2, view_w + 4, view_h + 4, 8, 0x05060A);
    draw_art(c, view_x, view_y, view_w, view_h, s->index);

    ui_button(c, x + 20, y + ch - 68, 64, 32, "Prev", 0,
              ui_hit(s->mx, s->my, 20, y + ch - 68, 64, 32));
    ui_button(c, x + 92, y + ch - 68, 64, 32, "Next", 0,
              ui_hit(s->mx, s->my, 92, y + ch - 68, 64, 32));
    font_draw(c, &font_ui_11, x + 176, y + ch - 58,
              "No image codec yet: these pieces are rendered by the compositor.",
              g_theme.text_dim, 225);

    for (i32 i = 0; i < 4; i++) {
        i32 tx = x + cw - 4 * 44 - 20 + i * 44;
        i32 ty = y + ch - 64;
        canvas_fill_round(c, tx, ty, 36, 36, 8,
                          i == s->index ? g_theme.accent : g_theme.surface);
        i32 inner = i == s->index ? 0x14151C : 0xFFFFFF;
        if (i == 0) {
            canvas_ring(c, tx + 18, ty + 18, 9, 1, inner, 170);
        } else if (i == 1) {
            canvas_fill(c, tx + 12, ty + 12, 12, 12, inner);
        } else if (i == 2) {
            canvas_circle(c, tx + 18, ty + 16, 6, inner, 170);
        } else {
            canvas_arc(c, tx + 18, ty + 22, 9, 1, 200, 340, inner, 200);
        }
    }
}

static void *image_create(void)
{
    image_state_t *s = kzalloc(sizeof(image_state_t));
    return s;
}

static const app_vtable_t image_vt = {
    .draw = image_draw,
    .key = image_key,
    .mouse = image_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_imageviewer = {
    .id = "imageviewer",
    .name = "Image Viewer",
    .tagline = "Generated AURELIA artwork",
    .width = 860,
    .height = 620,
    .tint = 0x4A3C5A,
    .vt = &image_vt,
    .create = image_create,
};
