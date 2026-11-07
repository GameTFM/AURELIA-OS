/* AURELIA OS - design tokens (colours, density, motion, typography scale) */
#pragma once

#include <aurelia/types.h>
#include <aurelia/font.h>

/* Official brand palette */
#define AURELIA_MIDNIGHT   0x090A0E
#define AURELIA_GOLD       0xD6B878
#define AURELIA_IVORY      0xF4F0E7
#define AURELIA_GRAPHITE   0x202127
#define AURELIA_WARM_GRAY  0x85838A

typedef struct {
    bool dark;
    u32 bg;
    u32 bg_alt;
    u32 surface;
    u32 surface_raised;
    u32 surface_hover;
    u32 surface_active;
    u32 text;
    u32 text_muted;
    u32 text_dim;
    u32 accent;
    u32 accent_soft;
    u32 border;
    u32 danger;
    u32 success;
    u32 warning;
    u32 overlay;
    u32 shadow;
    i32 density;         /* 0 compact, 1 comfortable, 2 spacious */
    i32 scale_percent;   /* display scaling for future hi-DPI modes */
    bool reduced_motion;
    i32 brightness;      /* 100 = normal; the compositor applies a dim overlay */
    u32 wallpaper_index;
} theme_t;

extern theme_t g_theme;

void theme_init(void);
void theme_set_dark(bool dark);
void theme_set_accent(u32 color);
void theme_set_wallpaper(u32 index);
void theme_apply_brightness(void);

/* Typography helpers */
const font_t *theme_font_body(void);
const font_t *theme_font_small(void);
const font_t *theme_font_caption(void);
const font_t *theme_font_title(void);
const font_t *theme_font_heading(void);
const font_t *theme_font_display(void);
const font_t *theme_font_brand(void);
const font_t *theme_font_mono(void);

/* Density-aware spacing */
i32 theme_pad(void);
i32 theme_row_height(void);
