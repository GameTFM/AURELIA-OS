/* AURELIA OS - desktop shell
 *
 * Boot splash hands control here. The shell owns the wallpaper, the top bar,
 * the dock, the launcher, quick settings, notifications, the lock screen and
 * the main input/render loop.
 */
#include <aurelia/desktop.h>
#include <aurelia/wm.h>
#include <aurelia/app.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/pmm.h>
#include <aurelia/timer.h>
#include <aurelia/rtc.h>
#include <aurelia/vfs.h>
#include <aurelia/sha256.h>
#include <aurelia/cpu.h>
#include <aurelia/input.h>
#include <aurelia/arch.h>
#include <aurelia/ui.h>
#include <aurelia/sound.h>
#include <generated/account.h>

#define TOPBAR_H 38
#define DOCK_H   70
#define TRAY_W   226

#define LAUNCHER_W        960
#define LAUNCHER_H        620
#define LAUNCHER_COLS     5
#define LAUNCHER_CELL_H   112
#define LAUNCHER_CELL_GAP 10

typedef enum {
    SHELL_LOGIN,
    SHELL_ACTIVE,
    SHELL_LOCKED,
} shell_state_t;

typedef struct {
    char title[64];
    char body[160];
    char time[8];
} notification_t;

static struct {
    shell_state_t state;
    bool launcher_open;
    bool quick_open;
    bool notifications_open;
    char query[48];
    u32 query_len;
    i32 hover_dock;
    i32 hover_launcher;
    char password[64];
    u32 password_len;
    bool password_error;
    u64 error_until;
    bool show_widgets;
    u32 workspace;
    u32 frame_ms;
    bool first_frame;
    char user[32];
    bool signed_in;
} g_shell;

static notification_t g_notifications[8];
static i32 g_notification_count;
static u32 *g_wallpaper;
static u32 g_wallpaper_w;
static u32 g_wallpaper_h;
static bool g_wallpaper_dirty = true;
static u32 g_wallpaper_sig;
static u32 *g_frame_buffer;
static canvas_t g_view;
static bool g_view_ready;
static char g_pending_file[VFS_PATH_MAX];
static u8 g_last_buttons;
static i32 g_last_mouse_x = -1;
static i32 g_last_mouse_y = -1;
static u64 g_last_click_ms;
static i32 g_last_click_x;
static i32 g_last_click_y;
static u64 g_login_anim_start;

/* ------------------------------------------------------------------------- */
/* Icons                                                                     */
/* ------------------------------------------------------------------------- */

static void draw_folder_glyph(canvas_t *c, i32 x, i32 y, i32 s, u32 color)
{
    canvas_fill_round_alpha(c, x + s / 8, y + s / 4, s / 3, s / 6, 2, color, 220);
    canvas_fill_round_alpha(c, x + s / 8, y + s / 3, s - s / 4, s / 2, 3, color, 255);
}

void desktop_draw_app_icon(canvas_t *c, const app_desc_t *app, i32 x, i32 y, i32 size)
{
    i32 radius = size / 5;
    u32 top = gfx_lighten(app->tint, 34);
    u32 bottom = gfx_darken(app->tint, 18);
    canvas_fill_round_vgrad(c, x, y, size, size, radius, top, bottom);
    canvas_outline_round(c, x, y, size, size, radius, 1, 0xFFFFFF, 26);
    u32 fg = 0xF4F0E7;
    i32 cx = x + size / 2;
    i32 cy = y + size / 2;
    const char *id = app->id;

    if (strcmp(id, "terminal") == 0) {
        canvas_fill_round(c, x + size / 5, y + size / 4, size * 3 / 5, size / 2, 4, 0x0B0C10);
        font_draw(c, &font_mono_15, x + size / 4, y + size / 3, ">_", AURELIA_GOLD, 255);
    } else if (strcmp(id, "explorer") == 0) {
        draw_folder_glyph(c, x, y, size, 0xE8D7AE);
    } else if (strcmp(id, "settings") == 0) {
        canvas_ring(c, cx, cy, size / 5, 2, fg, 245);
        for (i32 i = 0; i < 8; i++) {
            static const i16 dx[8] = {0, 7, 10, 7, 0, -7, -10, -7};
            static const i16 dy[8] = {-10, -7, 0, 7, 10, 7, 0, -7};
            canvas_circle(c, cx + dx[i] * size / 44, cy + dy[i] * size / 44, 1, fg, 235);
        }
    } else if (strcmp(id, "notes") == 0) {
        canvas_fill_round(c, x + size / 4, y + size / 5, size / 2, size * 3 / 5, 3, 0xF4F0E7);
        for (i32 i = 0; i < 3; i++) {
            canvas_fill(c, x + size / 3, y + size / 3 + i * size / 9, size / 3, 1, 0x202127);
        }
    } else if (strcmp(id, "calculator") == 0) {
        canvas_fill_round(c, x + size / 4, y + size / 4, size / 2, size / 2, 3, 0x11131A);
        for (i32 i = 0; i < 3; i++) {
            canvas_fill(c, x + size / 3 + i * size / 8, y + size / 2, 2, 2, 0xD6B878);
        }
        canvas_fill(c, x + size / 3, y + size / 3 + 4, size / 3, 1, 0xD6B878);
    } else if (strcmp(id, "monitor") == 0) {
        for (i32 i = 0; i < 3; i++) {
            i32 bar = (i32[]){size / 3, size / 2, size / 4}[i];
            canvas_fill_round(c, x + size / 4 + i * size / 5, y + size - size / 4 - bar,
                              size / 9, bar, 2, i == 0 ? AURELIA_GOLD : fg);
        }
    } else if (strcmp(id, "store") == 0) {
        canvas_fill_round(c, x + size / 4, y + size / 3, size / 2, size / 2, 3, fg);
        canvas_arc(c, cx, y + size / 3, size / 7, 2, 180, 360, fg, 255);
    } else if (strcmp(id, "browser") == 0) {
        canvas_ring(c, cx, cy, size / 4, 1, fg, 240);
        canvas_fill(c, cx - size / 4, cy - 1, size / 2, 2, fg);
        canvas_line(c, cx, cy - size / 4, cx, cy + size / 4, fg, 200);
        canvas_line(c, cx - size / 7, cy - size / 5, cx - size / 7, cy + size / 5, fg, 150);
        canvas_line(c, cx + size / 7, cy - size / 5, cx + size / 7, cy + size / 5, fg, 150);
    } else if (strcmp(id, "imageviewer") == 0) {
        canvas_circle(c, cx + size / 8, cy - size / 6, size / 12, fg, 220);
        canvas_line(c, cx - size / 3, cy + size / 4, cx - size / 12, cy - size / 6, fg, 220);
        canvas_line(c, cx - size / 12, cy - size / 6, cx + size / 4, cy + size / 4, fg, 220);
        canvas_line(c, cx + size / 4, cy + size / 4, cx - size / 3, cy + size / 4, fg, 220);
    } else if (strcmp(id, "player") == 0) {
        canvas_circle(c, cx - size / 10, cy + size / 6, size / 10, fg, 240);
        canvas_fill(c, cx - size / 10 + size / 16, cy - size / 4, 2, size / 2 - 2, fg);
        canvas_fill(c, cx - size / 10 + size / 16, cy - size / 4, size / 5, 2, fg);
        canvas_fill(c, cx - size / 10 + size / 16 + size / 5, cy - size / 4, 2, size / 7, fg);
    } else if (strcmp(id, "logs") == 0) {
        canvas_fill(c, cx - size / 4, cy - size / 5, size / 2, 2, fg);
        canvas_fill(c, cx - size / 4, cy, size / 3, 2, fg);
        canvas_fill(c, cx - size / 4, cy + size / 5, size / 2, 2, fg);
        canvas_circle(c, cx - size / 4 - 2, cy - size / 5 + 1, 2, AURELIA_GOLD, 240);
    } else if (strcmp(id, "clock") == 0) {
        canvas_ring(c, cx, cy, size / 4, 2, fg, 240);
        canvas_line(c, cx, cy, cx, cy - size / 8, fg, 240);
        canvas_line(c, cx, cy, cx + size / 10, cy + size / 12, fg, 240);
    } else if (strcmp(id, "calendar") == 0) {
        canvas_fill_round(c, x + size / 4, y + size / 4, size / 2, size / 2, 3, fg);
        canvas_fill(c, x + size / 4, y + size / 3, size / 2, 2, 0x202127);
        font_draw_centered(c, &font_ui_13, cx, y + size / 2, "6", 0x202127, 255);
    } else if (strcmp(id, "about") == 0) {
        font_draw_centered(c, &font_ui_28, cx, y + size / 5, "A", fg, 255);
    } else if (strcmp(id, "editor") == 0) {
        canvas_fill_round(c, x + size / 4, y + size / 5, size / 2, size * 3 / 5, 2, fg);
        for (i32 i = 0; i < 4; i++) {
            canvas_fill(c, x + size / 3, y + size / 4 + i * size / 10, size / 3, 1, 0x8C8A93);
        }
    } else {
        font_draw_centered(c, &font_ui_20, cx, y + size / 3, "A", fg, 235);
    }
    if (app->id[0]) {
        (void)bottom;
    }
}

/* ------------------------------------------------------------------------- */
/* Wallpaper                                                                 */
/* ------------------------------------------------------------------------- */

typedef struct {
    i16 x;
    i16 y;
} ridge_point_t;

static i32 ridge_height_at(i32 x, i32 width, i32 base, i32 amplitude,
                           const ridge_point_t *points, i32 count)
{
    if (!points || count < 2 || width <= 1) {
        return base;
    }
    i32 pos = i32_clamp(x * 1000 / (width - 1), 0, 1000);
    i32 i = 0;
    while (i < count - 2 && pos > points[i + 1].x) {
        i++;
    }
    i32 span = (i32)points[i + 1].x - points[i].x;
    i32 t = span > 0 ? i32_clamp((pos - points[i].x) * 256 / span, 0, 256) : 256;
    i32 curve = t * t * (3 * 256 - 2 * t) / (256 * 256);
    i32 value = (i32)points[i].y +
                ((i32)points[i + 1].y - (i32)points[i].y) * curve / 256;
    return base - amplitude * value / 100;
}

static void draw_ridge_layer(canvas_t *c, i32 w, i32 h, i32 base, i32 amplitude,
                             const ridge_point_t *points, i32 count, u32 color,
                             u32 rim, u8 rim_alpha)
{
    i32 previous_y = ridge_height_at(0, w, base, amplitude, points, count);
    previous_y = i32_clamp(previous_y, 0, h);
    for (i32 x = 0; x < w; x++) {
        i32 y = ridge_height_at(x, w, base, amplitude, points, count);
        y = i32_clamp(y, 0, h);
        canvas_fill(c, x, y, 1, h - y, color);
        if (x > 0) {
            canvas_line(c, x - 1, previous_y, x, y, rim, rim_alpha);
        }
        previous_y = y;
    }
}

static void paint_solstice_wallpaper(canvas_t *c, i32 w, i32 h)
{
    static const ridge_point_t far_ridge[] = {
        {0, 22}, {120, 46}, {210, 18}, {330, 72}, {450, 35}, {560, 88},
        {690, 30}, {820, 62}, {920, 26}, {1000, 44},
    };
    static const ridge_point_t mid_ridge[] = {
        {0, 30}, {160, 20}, {280, 70}, {380, 32}, {520, 85},
        {640, 38}, {760, 92}, {860, 24}, {1000, 50},
    };
    static const ridge_point_t near_ridge[] = {
        {0, 40}, {140, 62}, {240, 20}, {370, 52}, {500, 25},
        {620, 78}, {740, 22}, {880, 58}, {1000, 34},
    };

    canvas_vgradient(c, 0, 0, w, h, 0x06080F, 0x1B1510);

    /* A quiet field of stars above the horizon. */
    for (i32 i = 0; i < 150; i++) {
        i32 sx = (i * 431 + 97) % w;
        i32 sy = (i * 197 + 29) % (h * 56 / 100);
        u8 alpha = (u8)(45 + (i * 53) % 120);
        canvas_blend_pixel(c, sx, sy, 0xE8E2D4, alpha);
        if (i % 31 == 0) {
            canvas_blend_pixel(c, sx + 1, sy, 0xBEB6A6, (u8)(alpha / 2));
            canvas_blend_pixel(c, sx, sy + 1, 0xBEB6A6, (u8)(alpha / 2));
        }
    }

    /* Low winter sun, diffused through the haze. */
    i32 sun_x = w * 72 / 100;
    i32 sun_y = h * 39 / 100;
    for (i32 i = 0; i < 26; i++) {
        i32 radius = 318 - i * 10;
        canvas_circle(c, sun_x, sun_y, radius, 0xD8B878, 4);
    }
    canvas_circle(c, sun_x, sun_y, 44, 0xF2DCA8, 24);

    /* Warm haze gathers along the horizon. */
    for (i32 i = 0; i < 108; i++) {
        i32 y = h * 41 / 100 + i * 2;
        u8 alpha = (u8)(i < 42 ? i / 4 : (108 - i) / 5);
        canvas_hline(c, 0, y, w, 0xD6B878, alpha);
    }

    draw_ridge_layer(c, w, h, h * 62 / 100, h * 20 / 100, far_ridge,
                     (i32)ARRAY_SIZE(far_ridge), 0x1B1D27, 0xC8B184, 34);

    /* Fog settles between the ranges. */
    for (i32 i = 0; i < 56; i++) {
        i32 y = h * 57 / 100 + i * 4;
        canvas_hline(c, 0, y, w, 0xE0D4BD, (u8)(9 - i / 8));
    }

    draw_ridge_layer(c, w, h, h * 74 / 100, h * 24 / 100, mid_ridge,
                     (i32)ARRAY_SIZE(mid_ridge), 0x10131B, 0xB69A68, 42);
    draw_ridge_layer(c, w, h, h * 91 / 100, h * 17 / 100, near_ridge,
                     (i32)ARRAY_SIZE(near_ridge), 0x06080D, 0xD6B878, 52);

    /* A breath of mist along the foreground. */
    for (i32 i = 0; i < 28; i++) {
        i32 y = h * 84 / 100 + i * 3;
        canvas_hline(c, 0, y, w, 0xC9BDA8, (u8)i32_max(1, 4 - i / 8));
    }
}

static void paint_wallpaper(canvas_t *c, u32 variant)
{
    i32 w = c->width;
    i32 h = c->height;
    if (variant == 0) {
        paint_solstice_wallpaper(c, w, h);
    } else if (variant == 2 && !g_theme.dark) {
        canvas_vgradient(c, 0, 0, w, h, 0xF7F2E9, 0xDCD3C3);
        canvas_circle(c, w * 3 / 4, h / 3, h / 3, 0xEFE3C8, 90);
        for (i32 i = 0; i < 5; i++) {
            canvas_fill_round_alpha(c, -h / 3 + i * 210, h * 3 / 5 + i * 42, w + h, 260,
                                    180, i % 2 ? 0xE7DECF : 0xCFC3AE, 90);
        }
        canvas_vgradient(c, 0, 0, w, 120, 0xFFFFFF, 0xF2EDE3);
    } else if (variant == 1) {
        canvas_vgradient(c, 0, 0, w, h, 0x101116, 0x08090D);
        for (i32 i = 0; i < 9; i++) {
            canvas_fill_alpha(c, 0, h / 2 + i * 22 - 160, w, 2, i % 3 == 0 ? 0x2A2B34 : 0x16171D, 120);
        }
        canvas_arc(c, w / 2, h + 120, h / 2 + 220, 2, 200, 340, AURELIA_GOLD, 90);
        canvas_circle(c, w / 3, h / 4, 2, AURELIA_IVORY, 200);
        canvas_circle(c, w * 3 / 4, h / 5, 1, AURELIA_IVORY, 160);
    } else if (variant == 3) {
        canvas_vgradient(c, 0, 0, w, h, 0x07120F, 0x0A0B10);
        canvas_circle(c, w / 2, h + 260, h / 2 + 340, 0x10251F, 190);
        canvas_arc(c, w / 2, h + 300, h / 2 + 330, 3, 195, 345, 0x7FC9A8, 80);
        canvas_arc(c, w / 2, h + 300, h / 2 + 250, 1, 200, 340, 0xB9E4CC, 60);
        for (i32 i = 0; i < 26; i++) {
            i32 sx = (i * 47 + 113) % w;
            i32 sy = (i * 83 + 41) % (h / 2);
            canvas_pixel(c, sx, sy, 0x9FB7AE);
        }
    }
    /* Subtle vignette */
    for (i32 i = 0; i < 26; i++) {
        u32 alpha = (u32)(14 - i / 2);
        canvas_outline_round(c, -i * 8, -i * 8, w + i * 16, h + i * 16, 60 + i * 6, 6,
                             0x000000, (u8)i32_max(2, (i32)alpha));
    }
}

static void rebuild_wallpaper(void)
{
    i32 w = (i32)g_screen.width;
    i32 h = (i32)g_screen.height;
    if (!g_wallpaper || g_wallpaper_w != (u32)w || g_wallpaper_h != (u32)h) {
        if (g_wallpaper) {
            kfree(g_wallpaper);
        }
        g_wallpaper = kmalloc((size_t)w * h * sizeof(u32));
        g_wallpaper_w = (u32)w;
        g_wallpaper_h = (u32)h;
    }
    if (!g_wallpaper) {
        return;
    }
    canvas_t off;
    canvas_bind(&off, g_wallpaper, w, h, w);
    paint_wallpaper(&off, g_theme.wallpaper_index);
    g_wallpaper_sig = g_theme.wallpaper_index ^ (g_theme.dark ? 0x9E3779B9u : 0u) ^ g_theme.accent;
    g_wallpaper_dirty = false;
}

static void draw_wallpaper(canvas_t *c)
{
    u32 sig = g_theme.wallpaper_index ^ (g_theme.dark ? 0x9E3779B9u : 0u) ^ g_theme.accent;
    if (g_wallpaper_dirty || sig != g_wallpaper_sig || !g_wallpaper ||
        g_wallpaper_w != (u32)c->width) {
        rebuild_wallpaper();
    }
    if (g_wallpaper) {
        canvas_blit(c, g_wallpaper, (i32)g_wallpaper_w, (i32)g_wallpaper_h, 0, 0);
    } else {
        canvas_vgradient(c, 0, 0, c->width, c->height, 0x0B0C12, 0x15110C);
    }
}

/* ------------------------------------------------------------------------- */
/* Notifications                                                             */
/* ------------------------------------------------------------------------- */

void desktop_notify(const char *title, const char *body)
{
    if (g_notification_count >= (i32)ARRAY_SIZE(g_notifications)) {
        memmove(&g_notifications[0], &g_notifications[1],
                sizeof(notification_t) * (ARRAY_SIZE(g_notifications) - 1));
        g_notification_count = (i32)ARRAY_SIZE(g_notifications) - 1;
    }
    notification_t *n = &g_notifications[g_notification_count++];
    strncpy(n->title, title, sizeof(n->title) - 1);
    strncpy(n->body, body, sizeof(n->body) - 1);
    rtc_format_time(n->time, sizeof(n->time));
}

static void clear_notifications(void)
{
    g_notification_count = 0;
}

/* ------------------------------------------------------------------------- */
/* Clock widget                                                              */
/* ------------------------------------------------------------------------- */

static void uppercase_copy(char *dst, size_t size, const char *src)
{
    size_t i = 0;
    if (!dst || size == 0) {
        return;
    }
    while (src && src[i] && i + 1 < size) {
        char ch = src[i];
        dst[i] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
        i++;
    }
    dst[i] = '\0';
}

static void draw_tracked_centered(canvas_t *c, const font_t *font, i32 cx, i32 y,
                                  const char *text, i32 tracking, u32 color, u8 alpha)
{
    i32 width = 0;
    for (const char *p = text; *p; p++) {
        width += font_char_width(font, (u8)*p) + tracking;
    }
    if (text[0]) {
        width -= tracking;
    }
    i32 x = cx - width / 2;
    char glyph[2] = {0, 0};
    for (const char *p = text; *p; p++) {
        glyph[0] = *p;
        font_draw(c, font, x, y, glyph, color, alpha);
        x += font_char_width(font, (u8)*p) + tracking;
    }
}

static i32 shell_days_in_month(i32 month, i32 year)
{
    static const i32 days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 30;
    }
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

static i32 shell_first_weekday(i32 year, i32 month)
{
    i32 y = year;
    i32 m = month;
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    i32 k = y % 100;
    i32 j = y / 100;
    i32 h = (1 + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 6) % 7; /* 0 = Sunday */
}

static void draw_center_clock(canvas_t *c)
{
    if (!g_shell.show_widgets) {
        return;
    }
    rtc_time_t now;
    rtc_read(&now);
    i32 cx = c->width / 2;
    char weekday[16];
    char month[16];
    char time[8];
    char date[40];
    uppercase_copy(weekday, sizeof(weekday), rtc_weekday_name(now.weekday));
    uppercase_copy(month, sizeof(month), rtc_month_name(now.month));
    ksnprintf(time, sizeof(time), "%02u:%02u", now.hour, now.minute);
    ksnprintf(date, sizeof(date), "%u %s %u", now.day, month, now.year);

    canvas_hline(c, cx - 18, TOPBAR_H + 74, 36, g_theme.accent, 100);
    draw_tracked_centered(c, &font_ui_11, cx, TOPBAR_H + 86, "SOLSTICE", 5,
                          g_theme.accent, 200);
    draw_tracked_centered(c, &font_display_64, cx, TOPBAR_H + 110, weekday, 14,
                          AURELIA_IVORY, 210);
    font_draw_centered(c, &font_display_96, cx + 2, TOPBAR_H + 196, time, 0x000000, 45);
    font_draw_centered(c, &font_display_96, cx, TOPBAR_H + 194, time, AURELIA_IVORY, 255);
    draw_tracked_centered(c, &font_ui_11, cx, TOPBAR_H + 300, date, 4,
                          g_theme.text_muted, 235);
    canvas_hline(c, cx - 24, TOPBAR_H + 322, 48, g_theme.accent, 55);
}

static void draw_system_widget(canvas_t *c)
{
    if (!g_shell.show_widgets) {
        return;
    }
    i32 card_w = 300;
    i32 card_h = 164;
    i32 x = c->width - card_w - 34;
    i32 y = TOPBAR_H + 34;
    canvas_shadow(c, x, y, card_w, card_h, 20, 22, 110);
    canvas_fill_round_alpha(c, x, y, card_w, card_h, 20, 0x111218, 212);
    canvas_outline_round(c, x, y, card_w, card_h, 20, 1, 0xFFFFFF, 22);

    font_draw(c, &font_ui_11, x + 22, y + 16, "SYSTEM", 0x9A968F, 225);
    i32 stable_w = font_text_width(&font_ui_11, "STABLE");
    canvas_circle(c, x + card_w - 24 - stable_w - 9, y + 21, 2, g_theme.success, 230);
    font_draw_right(c, &font_ui_11, x + card_w - 22, y + 16, "STABLE", g_theme.success, 205);

    u32 used = (u32)(heap_used() / (1024 * 1024));
    u32 total = (u32)(heap_total() / (1024 * 1024));
    u32 pct = total ? (used * 100) / total : 0;
    char memory[32];
    ksnprintf(memory, sizeof(memory), "%u / %u MiB", used, total);
    font_draw(c, &font_ui_13, x + 22, y + 38, "Memory", 0xE4DFD5, 255);
    font_draw_right(c, &font_ui_11, x + card_w - 22, y + 40, memory, 0x8F8B96, 255);
    canvas_fill_round(c, x + 22, y + 58, card_w - 44, 5, 2, 0x26272F);
    canvas_fill_round(c, x + 22, y + 58, (card_w - 44) * (i32)pct / 100, 5, 2,
                      AURELIA_GOLD);

    char uptime[32];
    u64 secs = timer_uptime_seconds();
    ksnprintf(uptime, sizeof(uptime), "%llu:%02llu:%02llu", secs / 3600,
              (secs / 60) % 60, secs % 60);
    font_draw(c, &font_ui_13, x + 22, y + 76, "Uptime", 0xE4DFD5, 255);
    font_draw_right(c, &font_ui_11, x + card_w - 22, y + 78, uptime, 0x8F8B96, 255);

    font_draw(c, &font_ui_13, x + 22, y + 100, "Workspace", 0xE4DFD5, 255);
    char ws[16];
    ksnprintf(ws, sizeof(ws), "%u of 4", g_shell.workspace + 1);
    font_draw_right(c, &font_ui_11, x + card_w - 22, y + 102, ws, 0x8F8B96, 255);

    char frame[24];
    ksnprintf(frame, sizeof(frame), "%u ms", g_shell.frame_ms);
    font_draw(c, &font_ui_13, x + 22, y + 124, "Compositor", 0xE4DFD5, 255);
    font_draw_right(c, &font_ui_11, x + card_w - 22, y + 126, frame, 0x8F8B96, 255);
}

static void draw_calendar_widget(canvas_t *c)
{
    if (!g_shell.show_widgets) {
        return;
    }
    i32 card_w = 300;
    i32 card_h = 280;
    i32 x = 34;
    i32 y = TOPBAR_H + 34;
    canvas_shadow(c, x, y, card_w, card_h, 20, 24, 120);
    canvas_fill_round_alpha(c, x, y, card_w, card_h, 20, 0x111218, 214);
    canvas_outline_round(c, x, y, card_w, card_h, 20, 1, 0xFFFFFF, 22);

    font_draw(c, &font_ui_11, x + 20, y + 16, "CALENDAR", 0x9A968F, 225);
    font_draw_right(c, &font_ui_11, x + card_w - 20, y + 16, "SOLSTICE",
                    g_theme.accent, 180);

    rtc_time_t now;
    rtc_read(&now);
    char day_number[8];
    ksnprintf(day_number, sizeof(day_number), "%u", now.day);
    font_draw(c, &font_display_64, x + 20, y + 34, day_number, AURELIA_IVORY, 255);

    char weekday[16];
    char month[16];
    uppercase_copy(weekday, sizeof(weekday), rtc_weekday_name(now.weekday));
    uppercase_copy(month, sizeof(month), rtc_month_name(now.month));
    char month_year[32];
    ksnprintf(month_year, sizeof(month_year), "%s %u", month, now.year);
    font_draw(c, &font_ui_13, x + 88, y + 54, weekday, 0xE4DFD5, 255);
    font_draw(c, &font_ui_11, x + 88, y + 74, month_year, 0x8F8B96, 235);
    canvas_hline(c, x + 20, y + 110, card_w - 40, g_theme.border, 150);

    static const char *const headers[] = {"M", "T", "W", "T", "F", "S", "S"};
    i32 grid_x = x + 20;
    i32 grid_y = y + 142;
    i32 cell_w = (card_w - 40) / 7;
    for (i32 i = 0; i < 7; i++) {
        font_draw_centered(c, &font_ui_11, grid_x + i * cell_w + cell_w / 2, y + 120,
                           headers[i], 0x807D88, 230);
    }
    i32 first_sunday = shell_first_weekday((i32)now.year, (i32)now.month);
    i32 first_monday = (first_sunday + 6) % 7;
    i32 days = shell_days_in_month((i32)now.month, (i32)now.year);
    for (i32 day = 1; day <= days; day++) {
        i32 cell = first_monday + day - 1;
        i32 col = cell % 7;
        i32 row = cell / 7;
        i32 cx = grid_x + col * cell_w + cell_w / 2;
        i32 cy = grid_y + row * 19;
        bool today = day == (i32)now.day;
        if (today) {
            canvas_fill_round(c, cx - 11, cy - 3, 22, 17, 5, g_theme.accent);
        }
        char text[4];
        ksnprintf(text, sizeof(text), "%d", day);
        font_draw_centered(c, &font_ui_11, cx, cy, text,
                           today ? 0x14151C : g_theme.text_muted, 255);
    }
}

/* The Solstice layout composes the clock, left calendar, and right system
 * card as one desktop pass. Keep the legacy render_frame call site intact. */
static void draw_clock_widget(canvas_t *c)
{
    draw_center_clock(c);
    draw_calendar_widget(c);
    draw_system_widget(c);
}

/* ------------------------------------------------------------------------- */
/* Top bar                                                                   */
/* ------------------------------------------------------------------------- */

static void draw_status_icon_wifi(canvas_t *c, i32 x, i32 y, bool connected)
{
    u32 color = connected ? g_theme.text : g_theme.text_dim;
    canvas_arc(c, x, y + 10, 11, 2, 225, 315, color, 235);
    canvas_arc(c, x, y + 10, 7, 2, 225, 315, color, 235);
    canvas_circle(c, x, y + 9, 2, color, 235);
    if (!connected) {
        canvas_line(c, x - 8, y + 2, x + 8, y + 14, g_theme.danger, 220);
    }
}

static void draw_status_icon_volume(canvas_t *c, i32 x, i32 y, bool enabled)
{
    u32 color = enabled ? g_theme.text : g_theme.text_dim;
    canvas_fill_round(c, x - 7, y + 2, 5, 7, 1, color);
    canvas_fill(c, x - 2, y + 4, 3, 3, color);
    if (enabled) {
        canvas_arc(c, x + 1, y + 5, 6, 2, 300, 60, color, 220);
        canvas_arc(c, x + 1, y + 5, 10, 2, 300, 60, color, 200);
    } else {
        canvas_line(c, x + 1, y + 2, x + 9, y + 9, g_theme.danger, 220);
    }
}

static void draw_topbar(canvas_t *c)
{
    i32 w = c->width;
    canvas_fill_alpha(c, 0, 0, w, TOPBAR_H, g_theme.dark ? 0x0A0B10 : 0xF6F1E8, 225);
    canvas_hline(c, 0, TOPBAR_H - 1, w, g_theme.border, 120);

    font_draw(c, &font_ui_13, 20, 12, "AURELIA", g_theme.accent, 255);
    font_draw(c, &font_ui_11, 82, 13, "OS 1.0", g_theme.text_dim, 230);

    /* Workspace pills */
    i32 wx = 142;
    for (u32 i = 0; i < 4; i++) {
        bool active = (i == g_shell.workspace);
        u32 color = active ? g_theme.accent : g_theme.text_dim;
        canvas_fill_round(c, wx + (i32)i * 16, 16, active ? 10 : 5, 5, 3, color);
        if (active) {
            canvas_fill_round(c, wx + (i32)i * 16, 16, 10, 5, 3, g_theme.accent);
        }
    }

    /* Active window title */
    window_t *focused = wm_focused();
    if (focused) {
        font_draw_centered(c, &font_ui_11, w / 2, 13, focused->title, g_theme.text_muted, 230);
    }

    /* Right cluster */
    i32 right = w - 22;
    char time[8], date[16];
    rtc_format_time(time, sizeof(time));
    rtc_time_t now;
    rtc_read(&now);
    ksnprintf(date, sizeof(date), "%s %u", rtc_weekday_name(now.weekday), now.day);
    if (strlen(date) > 6) {
        /* shorten weekday to three letters */
        memcpy(date, rtc_weekday_name(now.weekday), 3);
        date[3] = ' ';
        date[4] = (char)('0' + now.day / 10);
        date[5] = (char)('0' + now.day % 10);
        date[6] = '\0';
    }
    i32 time_w = font_text_width(&font_ui_11, time);
    font_draw(c, &font_ui_11, right - time_w, 13, time, g_theme.text, 255);
    right -= time_w + 12;
    i32 date_w = font_text_width(&font_ui_11, date);
    font_draw(c, &font_ui_11, right - date_w, 13, date, g_theme.text_muted, 240);
    right -= date_w + 16;

    /* Quick settings pill */
    i32 pill_w = 74;
    i32 pill_x = right - pill_w;
    bool hover = g_shell.hover_dock == -100; /* reserved: quick settings hover */
    (void)hover;
    canvas_fill_round_alpha(c, pill_x, 7, pill_w, 24, 12,
                            g_shell.quick_open ? g_theme.surface_hover : 0x1C1D25, 200);
    draw_status_icon_wifi(c, pill_x + 18, 13, false);
    draw_status_icon_volume(c, pill_x + 42, 14, false);
    canvas_circle(c, pill_x + 62, 17, 3, g_theme.danger, 220);

}

/* ------------------------------------------------------------------------- */
/* Dock                                                                      */
/* ------------------------------------------------------------------------- */

static i32 dock_layout(i32 *out_x, i32 *out_y, i32 *out_w)
{
    u32 count = app_count() + 1; /* launcher + apps */
    i32 tile = 48;
    i32 gap = 12;
    i32 pad = 16;
    i32 width = pad * 2 + (i32)count * tile + ((i32)count - 1) * gap;
    i32 height = DOCK_H;
    i32 x = (i32)g_screen.width / 2 - width / 2;
    i32 y = (i32)g_screen.height - height - 20;
    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_w) *out_w = width;
    return tile;
}

static i32 dock_index_at(i32 mouse_x, i32 mouse_y)
{
    i32 x, y, w;
    i32 tile = dock_layout(&x, &y, &w);
    if (mouse_y < y || mouse_y >= y + DOCK_H || mouse_x < x || mouse_x >= x + w) {
        return -1;
    }
    i32 pad = 16;
    for (u32 i = 0; i < app_count() + 1; i++) {
        i32 tx = x + pad + (i32)i * (tile + 12);
        if (mouse_x >= tx && mouse_x < tx + tile) {
            return (i32)i;
        }
    }
    return -1;
}

static void draw_dock(canvas_t *c)
{
    i32 x, y, w;
    i32 tile = dock_layout(&x, &y, &w);
    canvas_shadow(c, x, y, w, DOCK_H, 24, 26, 120);
    canvas_fill_round_alpha(c, x, y, w, DOCK_H, 24, g_theme.dark ? 0x0E0F15 : 0xFBF8F2, 232);
    canvas_outline_round(c, x, y, w, DOCK_H, 24, 1, 0xFFFFFF, g_theme.dark ? 26 : 60);

    for (u32 i = 0; i < app_count() + 1; i++) {
        i32 tx = x + 16 + (i32)i * (tile + 12);
        i32 ty = y + (DOCK_H - tile) / 2 - (g_shell.hover_dock == (i32)i ? 4 : 0);
        if (i == 0) {
            canvas_fill_round_vgrad(c, tx, ty, tile, tile, 12, 0xE4C98F, 0xB9934F);
            font_draw_centered(c, &font_ui_28, tx + tile / 2, ty + 8, "A", 0x14151C, 255);
        } else {
            const app_desc_t *app = app_at(i - 1);
            if (!app) {
                continue;
            }
            desktop_draw_app_icon(c, app, tx, ty, tile);
            window_t *win = wm_find_by_app(app->id);
            if (win) {
                canvas_circle(c, tx + tile / 2, y + DOCK_H - 9, 2, g_theme.accent, 255);
            }
        }
        if (g_shell.hover_dock == (i32)i) {
            const char *label = (i == 0) ? "Launcher" : app_at(i - 1)->name;
            i32 lw = font_text_width(&font_ui_11, label) + 20;
            i32 lx = tx + tile / 2 - lw / 2;
            i32 ly = y - 34;
            canvas_fill_round_alpha(c, lx, ly, lw, 24, 8, 0x0A0B10, 235);
            canvas_outline_round(c, lx, ly, lw, 24, 8, 1, 0xFFFFFF, 22);
            font_draw_centered(c, &font_ui_11, lx + lw / 2, ly + 5, label, g_theme.text, 255);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Launcher                                                                  */
/* ------------------------------------------------------------------------- */

static bool app_matches(const app_desc_t *app)
{
    if (g_shell.query_len == 0) {
        return true;
    }
    char name[64];
    strncpy(name, app->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    str_tolower(name);
    char query[48];
    strncpy(query, g_shell.query, sizeof(query) - 1);
    query[sizeof(query) - 1] = '\0';
    str_tolower(query);
    return strstr(name, query) != NULLPTR;
}

static const app_desc_t *launcher_first_match(void)
{
    for (u32 i = 0; i < app_count(); i++) {
        const app_desc_t *app = app_at(i);
        if (app_matches(app)) {
            return app;
        }
    }
    return NULLPTR;
}

static void draw_launcher(canvas_t *c)
{
    i32 w = c->width;
    i32 h = c->height;
    canvas_fill_alpha(c, 0, 0, w, h, 0x05060A, 150);
    i32 pw = 900;
    i32 ph = 580;
    i32 px = w / 2 - pw / 2;
    i32 py = h / 2 - ph / 2;
    canvas_shadow(c, px, py, pw, ph, 24, 40, 170);
    canvas_fill_round_alpha(c, px, py, pw, ph, 24, g_theme.dark ? 0x111219 : 0xFBF8F2, 242);
    canvas_outline_round(c, px, py, pw, ph, 24, 1, 0xFFFFFF, 30);

    font_draw(c, &font_ui_20, px + 28, py + 24, "Applications", g_theme.text, 255);
    font_draw_right(c, &font_ui_11, px + pw - 28, py + 30, "ESC to close", g_theme.text_dim, 220);

    /* Search */
    i32 sx = px + 28;
    i32 sy = py + 66;
    i32 sw = pw - 56;
    canvas_fill_round(c, sx, sy, sw, 46, 12, g_theme.dark ? 0x1B1C25 : 0xEFE9DD);
    canvas_outline_round(c, sx, sy, sw, 46, 12, 1, g_theme.accent, 90);
    font_draw(c, &font_ui_15, sx + 18, sy + 13, g_shell.query_len ? g_shell.query
                                                                : "Search applications...",
              g_shell.query_len ? g_theme.text : g_theme.text_dim, 255);
    if ((timer_ms() / 500) % 2 == 0) {
        i32 caret = font_text_width(&font_ui_15, g_shell.query);
        canvas_fill(c, sx + 18 + caret + 1, sy + 14, 2, 20, g_theme.accent);
    }

    /* Grid: five columns, icon above label */
    i32 grid_y = sy + 66;
    const i32 cols = 5;
    i32 cell_w = (sw - (cols - 1) * 10) / cols;
    i32 cell_h = 118;
    i32 shown = 0;
    for (u32 i = 0; i < app_count(); i++) {
        const app_desc_t *app = app_at(i);
        if (!app_matches(app)) {
            continue;
        }
        i32 col = shown % cols;
        i32 row = shown / cols;
        i32 cx = sx + col * (cell_w + 10);
        i32 cy = grid_y + row * (cell_h + 10);
        bool hover = (g_shell.hover_launcher == (i32)i);
        canvas_fill_round_alpha(c, cx, cy, cell_w, cell_h, 14,
                                hover ? g_theme.surface_hover : g_theme.surface, hover ? 255 : 200);
        if (hover) {
            canvas_outline_round(c, cx, cy, cell_w, cell_h, 14, 1, g_theme.accent, 110);
        }
        desktop_draw_app_icon(c, app, cx + cell_w / 2 - 23, cy + 16, 46);
        font_draw_centered(c, &font_ui_11, cx + cell_w / 2, cy + 72, app->name,
                           g_theme.text, 255);
        shown++;
    }
    if (shown == 0) {
        font_draw_centered(c, &font_ui_13, px + pw / 2, grid_y + 40,
                           "No applications match that search.", g_theme.text_muted, 255);
    }

    canvas_hline(c, px + 28, py + ph - 74, pw - 56, g_theme.border, 140);
    font_draw(c, &font_ui_11, px + 28, py + ph - 56,
              "Enter opens the first match. Super closes the launcher.",
              g_theme.text_dim, 230);
    ui_button(c, px + pw - 340, py + ph - 62, 96, 32, "Lock", 0, false);
    ui_button(c, px + pw - 236, py + ph - 62, 96, 32, "Restart", 0, false);
    ui_button(c, px + pw - 132, py + ph - 62, 108, 32, "Shut down", 2, false);
}

static void launcher_click(i32 x, i32 y)
{
    i32 w = g_screen.width;
    i32 h = g_screen.height;
    i32 pw = 900, ph = 580;
    i32 px = w / 2 - pw / 2;
    i32 py = h / 2 - ph / 2;
    if (x < px || x >= px + pw || y < py || y >= py + ph) {
        g_shell.launcher_open = false;
        return;
    }
    i32 sw = pw - 56;
    i32 sx = px + 28;
    i32 grid_y = py + 132;
    const i32 cols = 5;
    i32 cell_w = (sw - (cols - 1) * 10) / cols;
    i32 shown = 0;
    for (u32 i = 0; i < app_count(); i++) {
        const app_desc_t *app = app_at(i);
        if (!app_matches(app)) {
            continue;
        }
        i32 col = shown % cols;
        i32 row = shown / cols;
        i32 cx = sx + col * (cell_w + 10);
        i32 cy = grid_y + row * 128;
        if (x >= cx && x < cx + cell_w && y >= cy && y < cy + 118) {
            desktop_launch_app(app->id);
            g_shell.launcher_open = false;
            return;
        }
        shown++;
    }
    if (y >= py + ph - 62 && y < py + ph - 30) {
        if (x >= px + pw - 340 && x < px + pw - 244) {
            desktop_lock();
            g_shell.launcher_open = false;
        } else if (x >= px + pw - 236 && x < px + pw - 140) {
            desktop_restart();
        } else if (x >= px + pw - 132 && x < px + pw - 24) {
            desktop_shutdown();
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Quick settings + notifications                                            */
/* ------------------------------------------------------------------------- */

static void draw_toggle(canvas_t *c, i32 x, i32 y, bool on, const char *label)
{
    canvas_fill_round_alpha(c, x, y, 42, 24, 12, on ? g_theme.accent : 0x35363F, 255);
    canvas_circle(c, x + (on ? 30 : 12), y + 12, 9, on ? 0x14151C : 0xE8E4DC, 255);
    font_draw(c, &font_ui_13, x + 54, y + 3, label, g_theme.text, 255);
}

static void draw_quick_settings(canvas_t *c)
{
    i32 w = c->width;
    i32 pw = 360;
    i32 ph = 436;
    i32 px = w - pw - 18;
    i32 py = 50;
    canvas_shadow(c, px, py, pw, ph, 20, 30, 150);
    canvas_fill_round_alpha(c, px, py, pw, ph, 20, g_theme.dark ? 0x111219 : 0xFBF8F2, 246);
    canvas_outline_round(c, px, py, pw, ph, 20, 1, 0xFFFFFF, 28);

    canvas_circle(c, px + 36, py + 36, 16, g_theme.accent, 235);
    font_draw_centered(c, &font_ui_15, px + 36, py + 27, "A", 0x14151C, 255);
    font_draw(c, &font_ui_13, px + 62, py + 22, "aurelia", g_theme.text, 255);
    font_draw(c, &font_ui_11, px + 62, py + 40, "Local session", g_theme.text_dim, 230);
    font_draw_right(c, &font_ui_11, px + pw - 22, py + 30, "QUICK SETTINGS", g_theme.accent, 200);

    draw_toggle(c, px + 22, py + 74, g_theme.dark, "Midnight theme");
    /* Reduced motion */
    canvas_fill_round_alpha(c, px + 22, py + 110, 42, 24, 12,
                            g_theme.reduced_motion ? g_theme.accent : 0x35363F, 255);
    canvas_circle(c, px + (g_theme.reduced_motion ? 52 : 34), py + 122, 9,
                  g_theme.reduced_motion ? 0x14151C : 0xE8E4DC, 255);
    font_draw(c, &font_ui_13, px + 76, py + 113, "Reduced motion", g_theme.text, 255);

    /* Brightness */
    font_draw(c, &font_ui_11, px + 22, py + 150, "BRIGHTNESS", g_theme.text_dim, 235);
    char brightness[8];
    ksnprintf(brightness, sizeof(brightness), "%d%%", g_theme.brightness);
    font_draw_right(c, &font_ui_11, px + pw - 22, py + 150, brightness, g_theme.text_muted, 235);
    canvas_fill_round(c, px + 22, py + 168, pw - 44, 6, 3, 0x30313A);
    canvas_fill_round(c, px + 22, py + 168, (pw - 44) * g_theme.brightness / 100, 6, 3,
                      g_theme.accent);
    canvas_circle(c, px + 22 + (pw - 44) * g_theme.brightness / 100, py + 171, 8,
                  AURELIA_IVORY, 255);

    /* Accent swatches */
    font_draw(c, &font_ui_11, px + 22, py + 192, "ACCENT", g_theme.text_dim, 235);
    static const u32 accents[] = {0xD6B878, 0xC9A27A, 0x9FA8C9, 0x9EC9B4, 0xD69A9A};
    for (i32 i = 0; i < 5; i++) {
        i32 sx = px + 22 + i * 34;
        canvas_circle(c, sx + 9, py + 218, 9, accents[i], 255);
        if (g_theme.accent == accents[i]) {
            canvas_ring(c, sx + 9, py + 218, 12, 1, accents[i], 200);
        }
    }

    /* Wallpapers */
    font_draw(c, &font_ui_11, px + 22, py + 246, "WALLPAPER", g_theme.text_dim, 235);
    for (i32 i = 0; i < 4; i++) {
        i32 wx = px + 22 + i * 82;
        u32 base = (i == 0) ? 0x17130D : (i == 1) ? 0x191A20 : (i == 2) ? 0xE7DECD : 0x10201A;
        canvas_fill_round(c, wx, py + 264, 72, 44, 8, base);
        if (i == 2) {
            canvas_circle(c, wx + 52, py + 286, 14, 0xEFE3C8, 150);
        } else {
            canvas_ring(c, wx + 40, py + 286, 14, 1, AURELIA_GOLD, 120);
        }
        if (g_theme.wallpaper_index == (u32)i) {
            canvas_outline_round(c, wx, py + 264, 72, 44, 8, 2, g_theme.accent, 230);
        }
    }

    canvas_hline(c, px + 22, py + 322, pw - 44, g_theme.border, 140);
    font_draw(c, &font_ui_11, px + 22, py + 334, "NETWORK", g_theme.text_dim, 230);
    font_draw(c, &font_ui_13, px + 110, py + 332, "No device driver", g_theme.text_muted, 245);
    font_draw(c, &font_ui_11, px + 22, py + 356, "AUDIO", g_theme.text_dim, 230);
    font_draw(c, &font_ui_13, px + 110, py + 354, "PC speaker only", g_theme.text_muted, 245);
    font_draw_right(c, &font_ui_11, px + pw - 22, py + 356, "driver pending", g_theme.text_dim, 200);

    /* Session buttons */
    const char *labels[] = {"Lock", "Restart", "Shut down"};
    i32 bx = px + 22;
    for (i32 i = 0; i < 3; i++) {
        i32 bw = font_text_width(&font_ui_11, labels[i]) + 30;
        canvas_fill_round_alpha(c, bx, py + ph - 58, bw, 32, 10,
                                i == 2 ? 0x3A2020 : 0x24252F, 220);
        font_draw_centered(c, &font_ui_11, bx + bw / 2, py + ph - 47, labels[i],
                           i == 2 ? 0xE8A0A0 : g_theme.text, 255);
        bx += bw + 8;
    }
}

static void draw_notifications(canvas_t *c)
{
    i32 w = c->width;
    i32 h = c->height;
    i32 pw = 360;
    i32 px = w - pw - 18;
    i32 py = 50;
    i32 ph = h - py - 20;
    canvas_shadow(c, px, py, pw, ph, 20, 30, 140);
    canvas_fill_round_alpha(c, px, py, pw, ph, 20, g_theme.dark ? 0x111219 : 0xFBF8F2, 246);
    canvas_outline_round(c, px, py, pw, ph, 20, 1, 0xFFFFFF, 26);
    font_draw(c, &font_ui_17, px + 22, py + 20, "Notifications", g_theme.text, 255);
    font_draw_right(c, &font_ui_11, px + pw - 22, py + 24, "CLEAR ALL", g_theme.accent, 220);
    canvas_hline(c, px + 22, py + 54, pw - 44, g_theme.border, 140);
    if (g_notification_count == 0) {
        font_draw_centered(c, &font_ui_13, px + pw / 2, py + 90, "You're all caught up.",
                           g_theme.text_muted, 255);
    }
    i32 y = py + 68;
    for (i32 i = g_notification_count - 1; i >= 0; i--) {
        const notification_t *n = &g_notifications[i];
        canvas_fill_round_alpha(c, px + 18, y, pw - 36, 76, 12, g_theme.surface, 220);
        canvas_circle(c, px + 36, y + 22, 4, g_theme.accent, 255);
        font_draw(c, &font_ui_13, px + 48, y + 14, n->title, g_theme.text, 255);
        font_draw_right(c, &font_ui_11, px + pw - 32, y + 16, n->time, g_theme.text_dim, 220);
        font_draw_wrapped(c, &font_ui_11, px + 48, y + 36, pw - 90, n->body,
                          g_theme.text_muted, 240);
        y += 86;
    }
}

/* ------------------------------------------------------------------------- */
/* Login / lock screen                                                       */
/* ------------------------------------------------------------------------- */

static bool verify_password(const char *password)
{
    char material[128];
    ksnprintf(material, sizeof(material), "%s%s", AURELIA_DEMO_SALT, password);
    char digest[65];
    sha256_hex(material, strlen(material), digest);
    return strcmp(digest, AURELIA_DEMO_HASH) == 0;
}

static void draw_login(canvas_t *c)
{
    i32 w = c->width;
    i32 h = c->height;
    draw_wallpaper(c);
    canvas_fill_alpha(c, 0, 0, w, h, g_theme.dark ? 0x05060A : 0x1A1712, 130);

    i32 cw = 420;
    i32 ch = 404;
    i32 cx = w / 2 - cw / 2;
    i32 cy = h / 2 - ch / 2;
    canvas_shadow(c, cx, cy, cw, ch, 24, 40, 160);
    canvas_fill_round_alpha(c, cx, cy, cw, ch, 24, g_theme.dark ? 0x111219 : 0xFBF8F2, 238);
    canvas_outline_round(c, cx, cy, cw, ch, 24, 1, 0xFFFFFF, 30);

    /* Mark */
    i32 mx = cx + cw / 2;
    i32 my = cy + 74;
    canvas_ring(c, mx, my, 34, 2, g_theme.accent, 255);
    canvas_ring(c, mx, my, 38, 1, 0x6A5A36, 160);
    canvas_line(c, mx - 14, my + 15, mx, my - 17, AURELIA_IVORY, 255);
    canvas_line(c, mx + 14, my + 15, mx, my - 17, AURELIA_IVORY, 255);
    canvas_fill(c, mx - 8, my + 4, 16, 2, g_theme.accent);

    font_draw_centered(c, &font_ui_20, mx, cy + 128, "AURELIA", AURELIA_IVORY, 255);
    font_draw_centered(c, &font_ui_11, mx, cy + 158, "SOLSTICE 1.0", g_theme.text_dim, 230);

    /* Avatar */
    canvas_circle(c, mx, cy + 204, 26, g_theme.accent, 230);
    font_draw_centered(c, &font_ui_28, mx, cy + 186, "A", 0x14151C, 255);
    font_draw_centered(c, &font_ui_15, mx, cy + 238, AURELIA_DEMO_USER, g_theme.text, 255);
    font_draw_centered(c, &font_ui_11, mx, cy + 258, "Enter your password", g_theme.text_dim, 230);

    /* Password field */
    i32 fw = cw - 72;
    i32 fx = cx + 36;
    i32 fy = cy + 282;
    canvas_fill_round(c, fx, fy, fw, 40, 10, g_theme.dark ? 0x1B1C24 : 0xEFE9DD);
    canvas_outline_round(c, fx, fy, fw, 40, 10, 1,
                         g_shell.password_error ? g_theme.danger : g_theme.accent, 150);
    char masked[40];
    u32 count = i32_min((i32)g_shell.password_len, 24);
    for (u32 i = 0; i < count; i++) {
        masked[i] = '*';
    }
    masked[count] = '\0';
    font_draw(c, &font_ui_15, fx + 16, fy + 10, masked, g_theme.text, 255);
    if ((timer_ms() / 500) % 2 == 0) {
        i32 caret = font_text_width(&font_ui_15, masked);
        canvas_fill(c, fx + 16 + caret + 1, fy + 11, 2, 18, g_theme.accent);
    }
    if (g_shell.password_error) {
        font_draw_centered(c, &font_ui_11, mx, cy + 328, "Incorrect password. Try again.",
                           g_theme.danger, 255);
    } else {
        font_draw_centered(c, &font_ui_11, mx, cy + 328,
                           "Demo account password: solstice", g_theme.text_dim, 220);
    }

    font_draw_centered(c, &font_ui_11, mx, cy + ch - 30, "AURELIA OS 1.0.0 alpha - press Enter to sign in",
                       g_theme.text_dim, 200);
}

static void login_submit(void)
{
    char password[64];
    strncpy(password, g_shell.password, sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';
    if (verify_password(password)) {
        g_shell.state = SHELL_ACTIVE;
        g_shell.password_len = 0;
        g_shell.password[0] = '\0';
        g_shell.password_error = false;
        g_shell.first_frame = true;
        g_login_anim_start = timer_ms();
        kprintf("[desktop] authenticated user %s\n", AURELIA_DEMO_USER);
    } else {
        g_shell.password_error = true;
        g_shell.error_until = timer_ms() + 2200;
        g_shell.password_len = 0;
        g_shell.password[0] = '\0';
        kprintf("[desktop] authentication failed for %s\n", AURELIA_DEMO_USER);
    }
}

/* ------------------------------------------------------------------------- */
/* Cursor                                                                    */
/* ------------------------------------------------------------------------- */

static void draw_cursor(canvas_t *c, i32 x, i32 y)
{
    static const char *const mask[] = {
        "X           ", "XX          ", "XXX         ", "XXXX        ",
        "XXXXX       ", "XXXXXX      ", "XXXXXXX     ", "XXXXXXXX    ",
        "XXXXXXXXX   ", "XXXXXXXXXX  ", "XXXXX       ", "XXX XX      ",
        "XX  XX      ", "X   XX      ", "    XX      ", "    XX      ",
        "     X      ",
    };
    const i32 rows = (i32)ARRAY_SIZE(mask);
    const i32 cols = 12;
    /* Dark outline first. */
    for (i32 row = 0; row < rows; row++) {
        for (i32 col = 0; col < cols; col++) {
            if (mask[row][col] == 'X') {
                continue;
            }
            bool near = false;
            for (i32 dy = -1; dy <= 1 && !near; dy++) {
                for (i32 dx = -1; dx <= 1; dx++) {
                    i32 rr = row + dy;
                    i32 cc = col + dx;
                    if (rr >= 0 && rr < rows && cc >= 0 && cc < cols && mask[rr][cc] == 'X') {
                        near = true;
                        break;
                    }
                }
            }
            if (near) {
                canvas_pixel(c, x + col, y + row, 0x0A0B0F);
            }
        }
    }
    for (i32 row = 0; row < rows; row++) {
        for (i32 col = 0; col < cols; col++) {
            if (mask[row][col] == 'X') {
                canvas_pixel(c, x + col, y + row, AURELIA_IVORY);
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Frame                                                                     */
/* ------------------------------------------------------------------------- */

static void draw_desktop_icons(canvas_t *c)
{
    (void)c;
}

static void render_frame(void)
{
    canvas_t *c = g_view_ready ? &g_view : &g_screen;
    canvas_clip_reset(c);

    if (g_shell.state == SHELL_LOGIN || g_shell.state == SHELL_LOCKED) {
        draw_login(c);
        draw_cursor(c, mouse_state()->x, mouse_state()->y);
        if (g_view_ready) {
            canvas_blit(&g_screen, g_view.pixels, g_view.width, g_view.height, 0, 0);
        }
        return;
    }

    draw_wallpaper(c);
    canvas_vgradient(c, 0, 0, c->width, 140, 0x000000, g_theme.dark ? 0x0B0C12 : 0xF2EDE3);
    for (i32 i = 0; i < 18; i++) {
        canvas_fill_alpha(c, 0, 0, c->width, 6, 0x000000, (u8)i32_max(0, 3 - i / 6));
    }
    draw_desktop_icons(c);
    draw_clock_widget(c);

    wm_draw(c);
    canvas_clip_reset(c);
    draw_dock(c);
    draw_topbar(c);

    if (g_shell.launcher_open) {
        draw_launcher(c);
    }
    if (g_shell.quick_open) {
        draw_quick_settings(c);
    }
    if (g_shell.notifications_open) {
        draw_notifications(c);
    }

    if (g_theme.brightness < 100) {
        u8 alpha = (u8)i32_clamp((100 - g_theme.brightness) * 2, 0, 190);
        canvas_fill_alpha(c, 0, 0, c->width, c->height, 0x000000, alpha);
    }
    draw_cursor(c, mouse_state()->x, mouse_state()->y);
    if (g_shell.first_frame) {
        g_shell.first_frame = false;
    }
    if (g_view_ready) {
        canvas_blit(&g_screen, g_view.pixels, g_view.width, g_view.height, 0, 0);
    }
}

/* ------------------------------------------------------------------------- */
/* Input                                                                     */
/* ------------------------------------------------------------------------- */

static void topbar_click(i32 x, i32 y)
{
    i32 w = g_screen.width;
    if (y > TOPBAR_H) {
        return;
    }
    if (x > w - 120 && x < w - 24) {
        g_shell.quick_open = !g_shell.quick_open;
        g_shell.notifications_open = false;
        return;
    }
    if (x > w - 360 && x < w - 120) {
        g_shell.notifications_open = !g_shell.notifications_open;
        g_shell.quick_open = false;
        return;
    }
    /* Workspace pills */
    if (x >= 136 && x < 210 && y > 8 && y < 30) {
        u32 ws = (u32)((x - 136) / 16);
        if (ws < 4) {
            g_shell.workspace = ws;
            wm_set_workspace(ws);
        }
        return;
    }
    /* Launcher button under the top bar */
    if (x < 52 && y > TOPBAR_H) {
        g_shell.launcher_open = !g_shell.launcher_open;
    }
}

static void quick_click(i32 x, i32 y)
{
    i32 w = g_screen.width;
    i32 pw = 360, ph = 436;
    i32 px = w - pw - 18;
    i32 py = 50;
    if (x < px || x >= px + pw || y < py || y >= py + ph) {
        g_shell.quick_open = false;
        return;
    }
    if (y >= py + 70 && y < py + 98 && x < px + 60) {
        theme_set_dark(!g_theme.dark);
        g_wallpaper_dirty = true;
        return;
    }
    if (y >= py + 106 && y < py + 136 && x < px + 200) {
        g_theme.reduced_motion = !g_theme.reduced_motion;
        return;
    }
    if (y >= py + 164 && y < py + 180) {
        g_theme.brightness = i32_clamp((x - (px + 22)) * 100 / (pw - 44), 20, 100);
        return;
    }
    if (y >= py + 204 && y < py + 236) {
        static const u32 accents[] = {0xD6B878, 0xC9A27A, 0x9FA8C9, 0x9EC9B4, 0xD69A9A};
        for (i32 i = 0; i < 5; i++) {
            i32 sx = px + 22 + i * 34 + 9;
            if (i32_abs(x - sx) <= 12) {
                theme_set_accent(accents[i]);
            }
        }
        return;
    }
    if (y >= py + 260 && y < py + 314) {
        for (i32 i = 0; i < 4; i++) {
            i32 wx = px + 22 + i * 82;
            if (x >= wx && x < wx + 72) {
                theme_set_wallpaper((u32)i);
                g_wallpaper_dirty = true;
                if (i == 2) {
                    theme_set_dark(false);
                } else if (i != 2) {
                    theme_set_dark(true);
                }
            }
        }
        return;
    }
    if (y >= py + ph - 70 && y < py + ph - 20) {
        if (x < px + 110) {
            desktop_lock();
        } else if (x < px + 210) {
            desktop_restart();
        } else {
            desktop_shutdown();
        }
    }
}

static void notifications_click(i32 x, i32 y)
{
    i32 w = g_screen.width;
    i32 pw = 360;
    i32 px = w - pw - 18;
    if (y > 60 && y < 90 && x > px + pw - 130) {
        clear_notifications();
        return;
    }
    if (x < px || x >= px + pw) {
        g_shell.notifications_open = false;
    }
}

static void launcher_toggle(void)
{
    g_shell.launcher_open = !g_shell.launcher_open;
    g_shell.query_len = 0;
    g_shell.query[0] = '\0';
    g_shell.quick_open = false;
    g_shell.notifications_open = false;
}

static bool desktop_key(const key_event_t *event)
{
    if (!event->pressed) {
        return false;
    }
    if (g_shell.state == SHELL_LOGIN || g_shell.state == SHELL_LOCKED) {
        if (event->key == KEY_ENTER) {
            login_submit();
            return true;
        }
        if (event->key == KEY_ESCAPE) {
            g_shell.password_len = 0;
            g_shell.password[0] = '\0';
            return true;
        }
        if (event->key == KEY_BACKSPACE) {
            if (g_shell.password_len > 0) {
                g_shell.password[--g_shell.password_len] = '\0';
            }
            return true;
        }
        if (event->key >= 32 && event->key < 127 && g_shell.password_len < 63) {
            g_shell.password[g_shell.password_len++] = (char)event->key;
            g_shell.password[g_shell.password_len] = '\0';
            return true;
        }
        return true;
    }

    bool meta = (event->mods & MOD_META) != 0;
    if (event->key == KEY_LEFT_META || event->key == KEY_RIGHT_META || event->key == KEY_F1) {
        launcher_toggle();
        return true;
    }
    if (meta) {
        if (event->key == 'l' || event->key == 'L') {
            desktop_lock();
            return true;
        }
        if (event->key >= '1' && event->key <= '4') {
            g_shell.workspace = (u32)(event->key - '1');
            wm_set_workspace(g_shell.workspace);
            return true;
        }
        if (event->key == 'd' || event->key == 'D') {
            g_shell.show_widgets = !g_shell.show_widgets;
            return true;
        }
    }
    if ((event->mods & MOD_ALT) && event->key == KEY_TAB) {
        wm_cycle();
        return true;
    }
    if ((event->mods & MOD_ALT) && event->key == KEY_F4) {
        wm_close_focused();
        return true;
    }
    if ((event->mods & MOD_CTRL) && (event->mods & MOD_ALT) &&
        (event->key == 't' || event->key == 'T')) {
        desktop_launch_app("terminal");
        return true;
    }
    if (event->key == KEY_ESCAPE) {
        if (g_shell.launcher_open || g_shell.quick_open || g_shell.notifications_open) {
            g_shell.launcher_open = false;
            g_shell.quick_open = false;
            g_shell.notifications_open = false;
            return true;
        }
    }

    if (g_shell.launcher_open) {
        if (event->key == KEY_BACKSPACE) {
            if (g_shell.query_len > 0) {
                g_shell.query[--g_shell.query_len] = '\0';
            }
            return true;
        }
        if (event->key == KEY_ENTER) {
            const app_desc_t *app = launcher_first_match();
            if (app) {
                desktop_launch_app(app->id);
                g_shell.launcher_open = false;
                g_shell.query_len = 0;
                g_shell.query[0] = '\0';
            }
            return true;
        }
        if (event->key >= 32 && event->key < 127 && g_shell.query_len < sizeof(g_shell.query) - 1) {
            g_shell.query[g_shell.query_len++] = (char)event->key;
            g_shell.query[g_shell.query_len] = '\0';
            return true;
        }
        return true;
    }
    return wm_handle_key(event);
}

static void desktop_mouse(i32 x, i32 y, u8 buttons, bool pressed, bool released,
                          bool double_click)
{
    g_shell.hover_dock = dock_index_at(x, y);

    if (g_shell.state == SHELL_LOGIN || g_shell.state == SHELL_LOCKED) {
        if (pressed) {
            i32 w = g_screen.width, h = g_screen.height;
            i32 cw = 420, ch = 404;
            i32 cx = w / 2 - cw / 2, cy = h / 2 - ch / 2;
            if (x >= cx + 36 && x < cx + cw - 36 && y >= cy + 282 && y < cy + 322) {
                /* focus the password field: it is always focused; just ignore */
            } else if (y >= cy + 330 && y < cy + 396 && x > cx + 100 && x < cx + cw - 100) {
                login_submit();
            }
        }
        return;
    }

    if (g_shell.launcher_open) {
        if (pressed) {
            launcher_click(x, y);
        }
        return;
    }
    if (g_shell.quick_open) {
        if (pressed) {
            quick_click(x, y);
        }
        return;
    }
    if (g_shell.notifications_open && pressed) {
        notifications_click(x, y);
        return;
    }

    if (pressed && y < TOPBAR_H) {
        topbar_click(x, y);
        return;
    }
    if (pressed && g_shell.hover_dock >= 0) {
        i32 index = g_shell.hover_dock;
        if (index == 0) {
            launcher_toggle();
        } else {
            const app_desc_t *app = app_at((u32)index - 1);
            if (app) {
                desktop_launch_app(app->id);
            }
        }
        return;
    }
    wm_handle_mouse(x, y, buttons, pressed, released, double_click);
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void desktop_open_file(const char *path)
{
    if (!path) {
        return;
    }
    strncpy(g_pending_file, path, sizeof(g_pending_file) - 1);
    g_pending_file[sizeof(g_pending_file) - 1] = '\0';
    window_t *existing = wm_find_by_app("editor");
    if (existing) {
        wm_close_window(existing);
    }
    desktop_launch_app("editor");
}

const char *desktop_take_pending_file(void)
{
    static char out[VFS_PATH_MAX];
    strncpy(out, g_pending_file, sizeof(out) - 1);
    out[sizeof(out) - 1] = '\0';
    g_pending_file[0] = '\0';
    return out;
}

bool desktop_launch_app_instance(const char *id)
{
    return desktop_launch_app(id);
}

bool desktop_launch_app(const char *id)
{
    const app_desc_t *app = app_find(id);
    if (!app) {
        return false;
    }
    window_t *existing = wm_find_by_app(id);
    if (existing && existing->workspace == wm_workspace()) {
        wm_focus(existing);
        return true;
    }
    window_t *w = wm_open(app);
    if (!w) {
        return false;
    }
    wm_focus(w);
    return true;
}

void desktop_init(void)
{
    memset(&g_shell, 0, sizeof(g_shell));
    g_shell.state = SHELL_LOGIN;
    g_shell.show_widgets = true;
    g_shell.workspace = 0;
    g_shell.hover_dock = -1;
    g_shell.hover_launcher = -1;
    g_shell.first_frame = true;
    strncpy(g_shell.user, AURELIA_DEMO_USER, sizeof(g_shell.user) - 1);
    g_notification_count = 0;

    vfs_init();
    rtc_init();
    wm_init();
    sound_init();
    g_pending_file[0] = '\0';

    g_view_ready = false;
    if (!g_frame_buffer) {
        g_frame_buffer = kmalloc((size_t)g_screen.width * g_screen.height * sizeof(u32));
    }
    if (g_frame_buffer) {
        canvas_bind(&g_view, g_frame_buffer, (i32)g_screen.width, (i32)g_screen.height,
                    (i32)g_screen.width);
        g_view_ready = true;
    }

    desktop_notify("AURELIA OS ready", "Login with the demo account (password: solstice).");
    desktop_notify("Display initialised", "1920x1080 compositor at 32 bits per pixel.");
    kprintf("[ok] desktop shell initialised\n");
}

void desktop_lock(void)
{
    if (g_shell.state != SHELL_ACTIVE) {
        return;
    }
    g_shell.state = SHELL_LOCKED;
    g_shell.password_len = 0;
    g_shell.password[0] = '\0';
    g_shell.launcher_open = false;
    g_shell.quick_open = false;
    g_shell.notifications_open = false;
    kprintf("[desktop] session locked\n");
}

void desktop_shutdown(void)
{
    kprintf("[desktop] shutdown requested\n");
    for (i32 i = 0; i < 18; i++) {
        canvas_fill_alpha(&g_screen, 0, 0, g_screen.width, g_screen.height, 0x000000, 16);
        timer_sleep_ms(30);
    }
    /* QEMU ACPI power-off (PIIX4 PM1a control register). */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    cli();
    for (;;) {
        hlt();
    }
}

void desktop_restart(void)
{
    kprintf("[desktop] restart requested\n");
    /* Pulse the keyboard controller reset line. */
    outb(0x64, 0xFE);
    for (volatile u32 i = 0; i < 1000000; i++) {
    }
    cli();
    for (;;) {
        hlt();
    }
}

u32 desktop_window_count(void) { return wm_count_on_workspace(wm_workspace()); }
u32 desktop_appearance_index(void) { return g_theme.wallpaper_index; }

void desktop_cycle_appearance(void)
{
    g_theme.wallpaper_index = (g_theme.wallpaper_index + 1) % 4;
    g_wallpaper_dirty = true;
}

void desktop_run(void)
{
    desktop_init();
    kprintf("[ok] entering desktop event loop\n");
    u64 last_second = timer_uptime_seconds();
    u64 last_render = 0;
    bool pending = true;

    for (;;) {
        bool dirty = false;
        key_event_t event;
        while (input_poll_key(&event)) {
            if (desktop_key(&event)) {
                dirty = true;
            }
        }

        const mouse_state_t *mouse = mouse_state();
        bool pressed = (mouse->buttons & 1) && !(g_last_buttons & 1);
        bool released = !(mouse->buttons & 1) && (g_last_buttons & 1);
        bool moved = mouse->x != g_last_mouse_x || mouse->y != g_last_mouse_y;
        bool double_click = false;
        if (pressed) {
            u64 now = timer_ms();
            if (now - g_last_click_ms < 420 &&
                i32_abs(mouse->x - g_last_click_x) < 8 &&
                i32_abs(mouse->y - g_last_click_y) < 8) {
                double_click = true;
            }
            g_last_click_ms = now;
            g_last_click_x = mouse->x;
            g_last_click_y = mouse->y;
        }
        if (moved || pressed || released) {
            desktop_mouse(mouse->x, mouse->y, mouse->buttons, pressed, released, double_click);
            g_last_mouse_x = mouse->x;
            g_last_mouse_y = mouse->y;
            dirty = true;
        }
        g_last_buttons = mouse->buttons;

        u64 now_second = timer_uptime_seconds();
        if (now_second != last_second) {
            last_second = now_second;
            dirty = true;
        }
        if (g_shell.password_error && timer_ms() > g_shell.error_until) {
            g_shell.password_error = false;
            dirty = true;
        }

        if (dirty) {
            pending = true;
        }
        u64 now = timer_ms();
        if (pending && now - last_render >= 20) {
            u64 start = now;
            render_frame();
            last_render = timer_ms();
            g_shell.frame_ms = (u32)(last_render - start);
            pending = false;
        }
        wm_tick_all();
        hlt();
    }
}
