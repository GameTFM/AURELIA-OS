/* AURELIA OS - theme implementation */
#include <aurelia/theme.h>
#include <aurelia/kstring.h>
#include <aurelia/gfx.h>

theme_t g_theme;

void theme_init(void)
{
    memset(&g_theme, 0, sizeof(g_theme));
    g_theme.dark = true;
    g_theme.bg = 0x090A0E;
    g_theme.bg_alt = 0x0C0D12;
    g_theme.surface = 0x14151C;
    g_theme.surface_raised = 0x1A1B23;
    g_theme.surface_hover = 0x24252F;
    g_theme.surface_active = 0x2D2E39;
    g_theme.text = 0xF4F0E7;
    g_theme.text_muted = 0xB4B0BA;
    g_theme.text_dim = 0x807D88;
    g_theme.accent = 0xD6B878;
    g_theme.accent_soft = 0x6E5C36;
    g_theme.border = 0x2A2B34;
    g_theme.danger = 0xD97070;
    g_theme.success = 0x86C9A4;
    g_theme.warning = 0xE2B86A;
    g_theme.overlay = 0x05060A;
    g_theme.shadow = 0x000000;
    g_theme.density = 1;
    g_theme.scale_percent = 100;
    g_theme.reduced_motion = false;
    g_theme.brightness = 100;
    g_theme.wallpaper_index = 0;
}

void theme_set_dark(bool dark)
{
    g_theme.dark = dark;
    if (dark) {
        g_theme.bg = 0x090A0E;
        g_theme.bg_alt = 0x0C0D12;
        g_theme.surface = 0x14151C;
        g_theme.surface_raised = 0x1A1B23;
        g_theme.surface_hover = 0x24252F;
        g_theme.surface_active = 0x2D2E39;
        g_theme.text = 0xF4F0E7;
        g_theme.text_muted = 0xB4B0BA;
        g_theme.text_dim = 0x807D88;
        g_theme.border = 0x2A2B34;
        g_theme.overlay = 0x05060A;
        g_theme.shadow = 0x000000;
    } else {
        g_theme.bg = 0xF2EDE3;
        g_theme.bg_alt = 0xEAE4D8;
        g_theme.surface = 0xFBF8F2;
        g_theme.surface_raised = 0xFFFFFF;
        g_theme.surface_hover = 0xEFE9DD;
        g_theme.surface_active = 0xE3DCCC;
        g_theme.text = 0x14151A;
        g_theme.text_muted = 0x5C5A63;
        g_theme.text_dim = 0x8A8790;
        g_theme.border = 0xD8D1C2;
        g_theme.overlay = 0xF7F3EA;
        g_theme.shadow = 0x4B4436;
    }
}

void theme_set_accent(u32 color)
{
    g_theme.accent = color;
    g_theme.accent_soft = gfx_mix(color, g_theme.bg, 130, 255);
}

void theme_set_wallpaper(u32 index) { g_theme.wallpaper_index = index; }

void theme_apply_brightness(void)
{
    /* Brightness is applied by the compositor as a dim overlay. */
}

const font_t *theme_font_body(void) { return &font_ui_13; }
const font_t *theme_font_small(void) { return &font_ui_11; }
const font_t *theme_font_caption(void) { return &font_ui_13; }
const font_t *theme_font_title(void) { return &font_ui_17; }
const font_t *theme_font_heading(void) { return &font_ui_20; }
const font_t *theme_font_display(void) { return &font_display_64; }
const font_t *theme_font_brand(void) { return &font_display_96; }
const font_t *theme_font_mono(void) { return &font_mono_13; }

i32 theme_pad(void)
{
    switch (g_theme.density) {
    case 0: return 8;
    case 2: return 18;
    default: return 12;
    }
}

i32 theme_row_height(void)
{
    switch (g_theme.density) {
    case 0: return 28;
    case 2: return 40;
    default: return 34;
    }
}
