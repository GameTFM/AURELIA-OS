/* AURELIA OS - System Monitor (real kernel telemetry only) */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/heap.h>
#include <aurelia/pmm.h>
#include <aurelia/cpu.h>
#include <aurelia/timer.h>
#include <aurelia/vfs.h>
#include <aurelia/fb.h>
#include <aurelia/log.h>

#define MON_SAMPLES 80

typedef struct {
    u32 heap_kb[MON_SAMPLES];
    u32 free_mb[MON_SAMPLES];
    i32 sample_count;
    i32 head;
    u64 last_sample;
    i32 mx, my;
    u32 frame_ms;
} monitor_state_t;

static monitor_state_t *state_of(window_t *w) { return (monitor_state_t *)w->state; }

static void monitor_sample(monitor_state_t *s)
{
    s->heap_kb[s->head] = (u32)(heap_used() / 1024);
    s->free_mb[s->head] = (u32)(pmm_free_bytes() / (1024 * 1024));
    s->head = (s->head + 1) % MON_SAMPLES;
    if (s->sample_count < MON_SAMPLES) {
        s->sample_count++;
    }
    s->last_sample = timer_ms();
}

static void monitor_tick(window_t *w)
{
    monitor_state_t *s = state_of(w);
    if (timer_ms() - s->last_sample >= 500) {
        monitor_sample(s);
    }
}

static void draw_spark(canvas_t *c, i32 x, i32 y, i32 w, i32 h, const u32 *values, i32 count,
                       u32 max_value, u32 color)
{
    canvas_fill_round(c, x, y, w, h, 8, g_theme.dark ? 0x0D0E13 : 0xFFFFFF);
    for (i32 i = 0; i < 3; i++) {
        canvas_hline(c, x, y + h * i / 3, w, g_theme.border, 60);
    }
    if (count < 2) {
        return;
    }
    i32 prev_x = x;
    i32 prev_y = y + h - (i32)(values[0] * (u32)h / (max_value ? max_value : 1));
    for (i32 i = 1; i < count; i++) {
        i32 px = x + i * w / (count - 1);
        i32 py = y + h - (i32)(values[i] * (u32)h / (max_value ? max_value : 1));
        py = i32_clamp(py, y, y + h);
        canvas_line(c, prev_x, prev_y, px, py, color, 235);
        prev_x = px;
        prev_y = py;
    }
}

static void monitor_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    monitor_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    font_draw(c, &font_ui_20, x + 24, y + 18, "System Monitor", g_theme.text, 255);
    font_draw(c, &font_ui_11, x + cw - 200, y + 26, "Live kernel telemetry", g_theme.text_dim, 235);

    u32 heap_used_kb = (u32)(heap_used() / 1024);
    u32 heap_total_kb = (u32)(heap_total() / 1024);
    u32 heap_pct = heap_total_kb ? heap_used_kb * 100 / heap_total_kb : 0;
    u32 pmm_pct = (u32)((pmm_total_bytes() - pmm_free_bytes()) * 100 /
                        (pmm_total_bytes() ? pmm_total_bytes() : 1));

    i32 card_w = (cw - 72) / 3;
    const char *labels[3] = {"KERNEL HEAP", "PHYSICAL MEMORY", "STORAGE (RAMFS)"};
    u32 pcts[3] = {heap_pct, pmm_pct, (vfs_used_bytes() * 100) / (256 * 1024)};
    char detail[3][64];
    ksnprintf(detail[0], sizeof(detail[0]), "%u / %u KiB", heap_used_kb, heap_total_kb);
    ksnprintf(detail[1], sizeof(detail[1]), "%llu / %llu MiB",
              (pmm_total_bytes() - pmm_free_bytes()) / (1024 * 1024),
              pmm_total_bytes() / (1024 * 1024));
    ksnprintf(detail[2], sizeof(detail[2]), "%u KiB in %u nodes", vfs_used_bytes() / 1024, 192);
    for (i32 i = 0; i < 3; i++) {
        i32 cx = x + 24 + i * (card_w + 12);
        canvas_fill_round_alpha(c, cx, y + 62, card_w, 92, 12, g_theme.surface, 220);
        font_draw(c, &font_ui_11, cx + 16, y + 76, labels[i], g_theme.text_dim, 235);
        char pct[8];
        ksnprintf(pct, sizeof(pct), "%u%%", pcts[i]);
        font_draw(c, &font_ui_20, cx + 16, y + 94, pct, g_theme.text, 255);
        ui_progress(c, cx + 16, y + 126, card_w - 32, 6, (i32)pcts[i], g_theme.accent);
        font_draw(c, &font_ui_11, cx + 16, y + 138, detail[i], g_theme.text_dim, 220);
    }

    /* Heap history */
    ui_section(c, x + 24, y + 174, "HEAP USAGE - 500 ms SAMPLES");
    u32 hist[MON_SAMPLES];
    u32 max_heap = 1;
    for (i32 i = 0; i < s->sample_count; i++) {
        i32 index = (s->head - s->sample_count + i + MON_SAMPLES) % MON_SAMPLES;
        hist[i] = s->heap_kb[index];
        if (hist[i] > max_heap) {
            max_heap = hist[i];
        }
    }
    draw_spark(c, x + 24, y + 194, cw - 48, 110, hist, s->sample_count,
               max_heap + 512, g_theme.accent);

    /* CPU + uptime */
    i32 info_y = y + 322;
    ui_section(c, x + 24, info_y, "PROCESSOR");
    const cpu_info_t *cpu = cpu_info();
    font_draw(c, &font_ui_13, x + 24, info_y + 20, cpu_brand(), g_theme.text, 255);
    char cpu_line[128];
    ksnprintf(cpu_line, sizeof(cpu_line), "%s  /  %u cores  /  APIC %s  /  SSE2 %s  /  AVX %s",
              cpu_vendor(), cpu->cores, cpu->has_apic ? "yes" : "no",
              cpu->has_sse2 ? "yes" : "no", cpu->has_avx ? "yes" : "no");
    font_draw(c, &font_ui_11, x + 24, info_y + 40, cpu_line, g_theme.text_muted, 235);

    ui_section(c, x + 24, info_y + 66, "SYSTEM");
    char line[160];
    u64 secs = timer_uptime_seconds();
    ksnprintf(line, sizeof(line), "Uptime %llu:%02llu:%02llu   /   %llu PIT ticks at %u Hz   /   %ux%u display",
              secs / 3600, (secs / 60) % 60, secs % 60, timer_ticks(), TIMER_HZ,
              fb_width(), fb_height());
    font_draw(c, &font_ui_11, x + 24, info_y + 84, line, g_theme.text_muted, 235);

    ui_section(c, x + 24, info_y + 112, "WINDOWS");
    font_draw(c, &font_ui_11, x + 24, info_y + 132, "ID    APPLICATION        STATE        WORKSPACE",
              g_theme.text_dim, 235);
    i32 row = 0;
    for (u32 i = 0; i < wm_count() && row < 5; i++) {
        window_t *win = wm_window_at(i);
        if (!win->used) {
            continue;
        }
        char wline[128];
        ksnprintf(wline, sizeof(wline), "%-3u   %-18s %-12s %u", win->id, win->app->name,
                  win->minimized ? "minimized" : (wm_is_focused(win) ? "focused" : "open"),
                  win->workspace);
        font_draw(c, &font_ui_11, x + 24, info_y + 152 + row * 18, wline,
                  win->minimized ? g_theme.text_dim : g_theme.text_muted, 235);
        row++;
    }
}

static void *monitor_create(void)
{
    monitor_state_t *s = kzalloc(sizeof(monitor_state_t));
    if (!s) {
        return NULLPTR;
    }
    s->last_sample = 0;
    monitor_sample(s);
    monitor_sample(s);
    return s;
}

static const app_vtable_t monitor_vt = {
    .draw = monitor_draw,
    .key = NULLPTR,
    .mouse = NULLPTR,
    .close = NULLPTR,
    .tick = monitor_tick,
};

const app_desc_t app_monitor = {
    .id = "monitor",
    .name = "System Monitor",
    .tagline = "Live memory, CPU and window telemetry",
    .width = 940,
    .height = 620,
    .tint = 0x2C4A3E,
    .vt = &monitor_vt,
    .create = monitor_create,
};
