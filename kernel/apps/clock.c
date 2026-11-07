/* AURELIA OS - Clock and calendar */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/rtc.h>
#include <aurelia/timer.h>
#include <aurelia/heap.h>

typedef struct {
    i32 mx, my;
} clock_state_t;

static i32 days_in_month(u32 month, u32 year)
{
    static const i32 days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return (month >= 1 && month <= 12) ? days[month - 1] : 30;
}

static i32 weekday_of_first(u32 year, u32 month)
{
    /* Zeller's congruence, returning 0 = Sunday. */
    i32 y = (i32)year;
    i32 m = (i32)month;
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    i32 k = y % 100;
    i32 j = y / 100;
    i32 h = (1 + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 6) % 7;
}

static void clock_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    (void)w;
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    rtc_time_t now;
    rtc_read(&now);

    static const i16 sin60[61] = {
        0, 105, 208, 309, 407, 500, 588, 669, 743, 809, 866, 914, 951, 978, 995, 1000,
        995, 978, 951, 914, 866, 809, 743, 669, 588, 500, 407, 309, 208, 105,
        0, -105, -208, -309, -407, -500, -588, -669, -743, -809, -866, -914,
        -951, -978, -995, -1000, -995, -978, -951, -914, -866, -809, -743, -669,
        -588, -500, -407, -309, -208, -105, 0,
    };
    i32 cx = x + cw / 2;
    i32 cy = y + 150;
    i32 radius = 96;
    canvas_circle(c, cx, cy, radius + 14, g_theme.dark ? 0x14151C : 0xEFE9DD, 255);
    canvas_circle(c, cx, cy, radius, g_theme.dark ? 0x0C0D12 : 0xFFFFFF, 255);
    canvas_ring(c, cx, cy, radius, 1, g_theme.accent, 90);

    for (i32 i = 0; i < 60; i++) {
        bool major = (i % 5) == 0;
        i32 len = major ? 9 : 4;
        i32 angle = i * 6;
        i32 s = sin60[angle / 6];
        i32 co = sin60[(angle / 6 + 15) % 60];
        i32 ox = cx + s * (radius - 6) / 1000;
        i32 oy = cy - co * (radius - 6) / 1000;
        i32 ix = cx + s * (radius - 6 - len) / 1000;
        i32 iy = cy - co * (radius - 6 - len) / 1000;
        canvas_line(c, ox, oy, ix, iy, major ? g_theme.text : g_theme.text_dim, major ? 220 : 120);
    }

    i32 second = now.second % 60;
    i32 minute = now.minute % 60;
    i32 hour = now.hour % 12;
    i32 hour_angle = (hour * 5 + minute / 12) % 60;
    i32 hour_x = cx + sin60[hour_angle] * (radius - 56) / 1000;
    i32 hour_y = cy - sin60[(hour_angle + 15) % 60] * (radius - 56) / 1000;
    canvas_line(c, cx, cy, hour_x, hour_y, g_theme.text, 255);
    canvas_line(c, cx + 2, cy, hour_x + 2, hour_y, g_theme.text, 180);
    i32 min_x = cx + sin60[minute] * (radius - 24) / 1000;
    i32 min_y = cy - sin60[(minute + 15) % 60] * (radius - 24) / 1000;
    canvas_line(c, cx, cy, min_x, min_y, g_theme.text, 255);
    i32 sec_x = cx + sin60[second] * (radius - 14) / 1000;
    i32 sec_y = cy - sin60[(second + 15) % 60] * (radius - 14) / 1000;
    canvas_line(c, cx, cy, sec_x, sec_y, g_theme.accent, 255);
    canvas_circle(c, cx, cy, 5, g_theme.accent, 255);

    char time[8];
    ksnprintf(time, sizeof(time), "%02u:%02u", now.hour, now.minute);
    font_draw_centered(c, &font_ui_28, cx, y + 262, time, g_theme.text, 255);
    char date[64];
    ksnprintf(date, sizeof(date), "%s, %u %s %u", rtc_weekday_name(now.weekday), now.day,
              rtc_month_name(now.month), now.year);
    font_draw_centered(c, &font_ui_11, cx, y + 302, date, g_theme.text_muted, 235);

    /* Month view */
    i32 mx = x + 28;
    i32 my = y + 336;
    font_draw(c, &font_ui_13, mx, my, rtc_month_name(now.month), g_theme.text, 255);
    font_draw_right(c, &font_ui_11, x + cw - 28, my + 2, "AURELIA CALENDAR", g_theme.text_dim, 200);
    static const char *const names[] = {"S", "M", "T", "W", "T", "F", "S"};
    for (i32 i = 0; i < 7; i++) {
        font_draw_centered(c, &font_ui_11, mx + 18 + i * 38, my + 26, names[i],
                           g_theme.text_dim, 235);
    }
    i32 first = weekday_of_first(now.year, now.month);
    i32 days = days_in_month(now.month, now.year);
    for (i32 day = 1; day <= days; day++) {
        i32 cell = first + day - 1;
        i32 col = cell % 7;
        i32 row = cell / 7;
        i32 dx = mx + 18 + col * 38;
        i32 dy = my + 50 + row * 32;
        bool today = day == (i32)now.day;
        if (today) {
            canvas_fill_round(c, dx - 12, dy - 4, 24, 24, 6, g_theme.accent);
        }
        char day_text[4];
        ksnprintf(day_text, sizeof(day_text), "%d", day);
        font_draw_centered(c, &font_ui_11, dx, dy + 1, day_text,
                           today ? 0x14151C : g_theme.text_muted, 255);
    }
    (void)ch;
}

static void *clock_create(void)
{
    return kzalloc(sizeof(clock_state_t));
}

static const app_vtable_t clock_vt = {
    .draw = clock_draw,
    .key = NULLPTR,
    .mouse = NULLPTR,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_clock = {
    .id = "clock",
    .name = "Clock",
    .tagline = "Analogue time and today's date",
    .width = 460,
    .height = 620,
    .tint = 0x4A4436,
    .vt = &clock_vt,
    .create = clock_create,
};
