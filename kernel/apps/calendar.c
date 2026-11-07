/* AURELIA OS - Calendar */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/rtc.h>

typedef struct {
    i32 month_offset;
    i32 mx, my;
} calendar_state_t;

static calendar_state_t *state_of(window_t *w) { return (calendar_state_t *)w->state; }

static i32 days_in_month(i32 month, i32 year)
{
    static const i32 days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while (month < 1) {
        month += 12;
        year--;
    }
    while (month > 12) {
        month -= 12;
        year++;
    }
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

static i32 first_weekday(i32 year, i32 month)
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
    return (h + 6) % 7;
}

static void calendar_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    calendar_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    if (y >= 78 && y < 112) {
        if (x >= 28 && x < 68) {
            s->month_offset--;
        } else if (x >= w->w - 76 && x < w->w - 36) {
            s->month_offset++;
        } else if (x >= w->w - 160 && x < w->w - 84) {
            s->month_offset = 0;
        }
    }
}

static void calendar_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    calendar_state_t *s = state_of(w);
    if (event->key == KEY_LEFT) {
        s->month_offset--;
    } else if (event->key == KEY_RIGHT) {
        s->month_offset++;
    } else if (event->key == KEY_HOME) {
        s->month_offset = 0;
    }
}

static void calendar_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    calendar_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    rtc_time_t now;
    rtc_read(&now);

    i32 month = (i32)now.month + s->month_offset;
    i32 year = (i32)now.year;
    while (month < 1) {
        month += 12;
        year--;
    }
    while (month > 12) {
        month -= 12;
        year++;
    }

    font_draw(c, &font_ui_28, x + 28, y + 26, rtc_month_name((u8)month), g_theme.text, 255);
    char year_text[8];
    ksnprintf(year_text, sizeof(year_text), "%d", year);
    font_draw(c, &font_ui_20, x + 28 + font_text_width(&font_ui_28, rtc_month_name((u8)month)) + 12,
              y + 36, year_text, g_theme.accent, 255);

    ui_button(c, x + 28, y + 78, 40, 32, "<", 0, ui_hit(s->mx, s->my, 28, y + 78, 40, 32));
    ui_button(c, x + cw - 68, y + 78, 40, 32, ">", 0,
              ui_hit(s->mx, s->my, cw - 68, y + 78, 40, 32));
    ui_button(c, x + cw - 160, y + 78, 76, 32, "Today", 1,
              ui_hit(s->mx, s->my, cw - 160, y + 78, 76, 32));

    i32 grid_x = x + 28;
    i32 grid_y = y + 138;
    i32 cell_w = (cw - 56) / 7;
    i32 cell_h = (ch - 190) / 6;
    static const char *const headers[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    for (i32 i = 0; i < 7; i++) {
        font_draw_centered(c, &font_ui_11, grid_x + i * cell_w + cell_w / 2, grid_y - 18,
                           headers[i], g_theme.text_dim, 235);
    }
    for (i32 i = 0; i < 7; i++) {
        for (i32 row = 0; row < 6; row++) {
            canvas_outline_round(c, grid_x + i * cell_w + 2, grid_y + row * cell_h + 2,
                                 cell_w - 4, cell_h - 4, 6, 1, g_theme.border, 70);
        }
    }

    i32 first = first_weekday(year, month);
    i32 days = days_in_month(month, year);
    for (i32 day = 1; day <= days; day++) {
        i32 cell = first + day - 1;
        i32 col = cell % 7;
        i32 row = cell / 7;
        i32 cx = grid_x + col * cell_w + cell_w / 2;
        i32 cy = grid_y + row * cell_h + 14;
        bool today = (s->month_offset == 0) && day == (i32)now.day;
        if (today) {
            canvas_fill_round(c, cx - 16, cy - 8, 32, 32, 8, g_theme.accent);
        }
        char text[4];
        ksnprintf(text, sizeof(text), "%d", day);
        font_draw_centered(c, &font_ui_13, cx, cy, text, today ? 0x14151C : g_theme.text_muted,
                           255);
    }

    char footer[96];
    ksnprintf(footer, sizeof(footer), "Today is %s, %u %s %u  -  %s",
              rtc_weekday_name(now.weekday), now.day, rtc_month_name(now.month), now.year,
              "dates come from the CMOS RTC");
    font_draw(c, &font_ui_11, x + 28, y + ch - 28, footer, g_theme.text_dim, 225);
}

static void *calendar_create(void)
{
    return kzalloc(sizeof(calendar_state_t));
}

static const app_vtable_t calendar_vt = {
    .draw = calendar_draw,
    .key = calendar_key,
    .mouse = calendar_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_calendar = {
    .id = "calendar",
    .name = "Calendar",
    .tagline = "Month view driven by the real-time clock",
    .width = 780,
    .height = 600,
    .tint = 0x6B4A3A,
    .vt = &calendar_vt,
    .create = calendar_create,
};
