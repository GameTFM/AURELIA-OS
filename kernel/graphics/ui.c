/* AURELIA OS - shared UI widget drawing */
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/kstring.h>

bool ui_hit(i32 px, i32 py, i32 x, i32 y, i32 w, i32 h)
{
    return px >= x && py >= y && px < x + w && py < y + h;
}

void ui_panel(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius)
{
    canvas_fill_round_alpha(c, x, y, w, h, radius, g_theme.surface, 245);
    canvas_outline_round(c, x, y, w, h, radius, 1, 0xFFFFFF, 20);
}

void ui_panel_flat(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 radius)
{
    canvas_fill_round(c, x, y, w, h, radius, g_theme.dark ? 0x171820 : 0xEFE9DD);
}

void ui_divider(canvas_t *c, i32 x, i32 y, i32 w)
{
    canvas_hline(c, x, y, w, g_theme.border, 160);
}

void ui_section(canvas_t *c, i32 x, i32 y, const char *label)
{
    font_draw(c, &font_ui_11, x, y, label, g_theme.text_dim, 235);
}

void ui_button(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const char *label, i32 style,
               bool hover)
{
    u32 fill;
    u32 text;
    if (style == 1) {
        fill = hover ? gfx_lighten(g_theme.accent, 18) : g_theme.accent;
        text = 0x14151C;
    } else if (style == 2) {
        fill = hover ? 0x4A2525 : 0x3A2020;
        text = 0xE8A0A0;
    } else {
        fill = hover ? g_theme.surface_hover : 0x23242E;
        text = g_theme.text;
    }
    canvas_fill_round(c, x, y, w, h, 9, fill);
    if (style == 0) {
        canvas_outline_round(c, x, y, w, h, 9, 1, 0xFFFFFF, 18);
    }
    font_draw_centered(c, &font_ui_11, x + w / 2, y + (h - font_line_height(&font_ui_11)) / 2,
                       label, text, 255);
}

void ui_toggle(canvas_t *c, i32 x, i32 y, bool on)
{
    canvas_fill_round_alpha(c, x, y, 42, 24, 12, on ? g_theme.accent : 0x35363F, 255);
    canvas_circle(c, x + (on ? 30 : 12), y + 12, 9, on ? 0x14151C : 0xE8E4DC, 255);
}

void ui_progress(canvas_t *c, i32 x, i32 y, i32 w, i32 h, i32 percent, u32 color)
{
    percent = i32_clamp(percent, 0, 100);
    canvas_fill_round(c, x, y, w, h, h / 2, g_theme.dark ? 0x2B2C35 : 0xD8D1C2);
    if (percent > 0) {
        canvas_fill_round(c, x, y, i32_max(h, w * percent / 100), h, h / 2, color);
    }
}

void ui_field(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const char *text,
              const char *placeholder, bool focused)
{
    canvas_fill_round(c, x, y, w, h, 9, g_theme.dark ? 0x1B1C24 : 0xFFFFFF);
    canvas_outline_round(c, x, y, w, h, 9, 1, focused ? g_theme.accent : g_theme.border, 170);
    bool empty = !text || !*text;
    font_draw(c, &font_ui_13, x + 12, y + (h - font_line_height(&font_ui_13)) / 2,
              empty ? placeholder : text, empty ? g_theme.text_dim : g_theme.text, 255);
}

void ui_sidebar_item(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const char *label,
                     bool active, bool hover)
{
    if (active) {
        canvas_fill_round(c, x, y, w, h, 9, g_theme.dark ? 0x24252F : 0xE3DCCC);
        canvas_fill(c, x, y + h / 2 - 5, 2, 10, g_theme.accent);
    } else if (hover) {
        canvas_fill_round_alpha(c, x, y, w, h, 9, g_theme.surface_hover, 120);
    }
    font_draw(c, &font_ui_13, x + 16, y + (h - font_line_height(&font_ui_13)) / 2, label,
              active ? g_theme.text : g_theme.text_muted, 255);
}

void ui_swatch(canvas_t *c, i32 cx, i32 cy, i32 radius, u32 color, bool selected)
{
    canvas_circle(c, cx, cy, radius, color, 255);
    if (selected) {
        canvas_ring(c, cx, cy, radius + 3, 1, color, 220);
    }
}

void ui_tag(canvas_t *c, i32 x, i32 y, const char *label)
{
    i32 w = font_text_width(&font_ui_11, label) + 16;
    canvas_fill_round_alpha(c, x, y, w, 20, 6, g_theme.accent, 26);
    font_draw(c, &font_ui_11, x + 8, y + 3, label, g_theme.accent, 235);
}

void ui_header(canvas_t *c, i32 x, i32 y, i32 w, const char *title, const char *subtitle)
{
    font_draw(c, &font_ui_20, x, y, title, g_theme.text, 255);
    if (subtitle && *subtitle) {
        font_draw(c, &font_ui_11, x, y + 28, subtitle, g_theme.text_muted, 235);
    }
    (void)w;
}
