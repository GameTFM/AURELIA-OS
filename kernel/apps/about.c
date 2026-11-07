/* AURELIA OS - System Information */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/pmm.h>
#include <aurelia/cpu.h>
#include <aurelia/timer.h>
#include <aurelia/vfs.h>
#include <aurelia/fb.h>

static void about_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    (void)w;
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    canvas_fill_round_alpha(c, x + 24, y + 24, cw - 48, 118, 16, g_theme.surface, 235);
    canvas_ring(c, x + 76, y + 82, 30, 2, g_theme.accent, 255);
    canvas_ring(c, x + 76, y + 82, 34, 1, 0x6A5A36, 160);
    canvas_line(c, x + 64, y + 97, x + 76, y + 66, AURELIA_IVORY, 255);
    canvas_line(c, x + 88, y + 97, x + 76, y + 66, AURELIA_IVORY, 255);
    canvas_fill(c, x + 68, y + 84, 16, 2, g_theme.accent);

    font_draw(c, &font_ui_20, x + 126, y + 48, "AURELIA OS 1.0.0", g_theme.text, 255);
    font_draw(c, &font_ui_11, x + 126, y + 74, "SOLSTICE  -  ALPHA  -  X86_64", g_theme.accent, 230);
    font_draw(c, &font_ui_11, x + 126, y + 96,
              "Custom kernel, compositor, window manager and applications built from scratch.",
              g_theme.text_muted, 235);
    font_draw(c, &font_ui_11, x + 126, y + 114,
              "No libc, no X11, no Wayland, no external UI toolkit.",
              g_theme.text_dim, 220);

    i32 col_w = (cw - 72) / 2;
    i32 ly = y + 166;
    const cpu_info_t *cpu = cpu_info();
    struct {
        const char *label;
        char value[80];
    } left[6];
    ksnprintf(left[0].value, sizeof(left[0].value), "AURELIA 1.0.0 (x86_64)");
    left[0].label = "Kernel";
    ksnprintf(left[1].value, sizeof(left[1].value), "%s", cpu_vendor());
    left[1].label = "CPU vendor";
    ksnprintf(left[2].value, sizeof(left[2].value), "%s", cpu_brand());
    left[2].label = "Processor";
    ksnprintf(left[3].value, sizeof(left[3].value), "%u core(s)", cpu->cores);
    left[3].label = "Cores";
    ksnprintf(left[4].value, sizeof(left[4].value), "%llu MiB (%llu free)",
              pmm_total_bytes() / (1024 * 1024), pmm_free_bytes() / (1024 * 1024));
    left[4].label = "Physical memory";
    ksnprintf(left[5].value, sizeof(left[5].value), "%u KiB / 64 MiB", (u32)(heap_used() / 1024));
    left[5].label = "Heap used";
    (void)left;
    for (i32 i = 0; i < 6; i++) {
        i32 ry = ly + i * 40;
        font_draw(c, &font_ui_11, x + 24, ry, left[i].label, g_theme.text_dim, 235);
        font_draw(c, &font_ui_13, x + 24, ry + 15, left[i].value, g_theme.text_muted, 255);
    }

    struct {
        const char *label;
        char value[80];
    } right[6];
    ksnprintf(right[0].value, sizeof(right[0].value), "%ux%u, 32 bpp", fb_width(), fb_height());
    right[0].label = "Display";
    ksnprintf(right[1].value, sizeof(right[1].value), "%llu s", timer_uptime_seconds());
    right[1].label = "Uptime";
    ksnprintf(right[2].value, sizeof(right[2].value), "%u KiB in RAM", vfs_used_bytes() / 1024);
    right[2].label = "Filesystem";
    ksnprintf(right[3].value, sizeof(right[3].value), "Limine 12.9.6");
    right[3].label = "Bootloader";
    ksnprintf(right[4].value, sizeof(right[4].value), "1000 Hz (8254 PIT)");
    right[4].label = "Timer";
    ksnprintf(right[5].value, sizeof(right[5].value), "%llu ticks", timer_ticks());
    right[5].label = "Ticks";
    (void)right;
    for (i32 i = 0; i < 6; i++) {
        i32 ry = ly + i * 40;
        font_draw(c, &font_ui_11, x + 24 + col_w, ry, right[i].label, g_theme.text_dim, 235);
        font_draw(c, &font_ui_13, x + 24 + col_w, ry + 15, right[i].value, g_theme.text_muted, 255);
    }

    i32 my = ly + 256;
    canvas_hline(c, x + 24, my, cw - 48, g_theme.border, 140);
    font_draw(c, &font_ui_11, x + 24, my + 10, "CAPABILITY STATUS", g_theme.text_dim, 235);
    const struct {
        const char *name;
        bool done;
        const char *note;
    } features[] = {
        {"Framebuffer and compositor", true, "linear framebuffer, software renderer"},
        {"Window manager", true, "move, resize, snap, z-order, workspaces"},
        {"Physical memory", true, "bitmap allocator over the Limine memory map"},
        {"Kernel heap", true, "64 MiB boundary-tag allocator"},
        {"Paging", true, "kernel page tables + user mappings"},
        {"Ring 3 and syscalls", true, "boot-test only; apps still ring 0"},
        {"Kernel threads", true, "cooperative context-switch test"},
        {"Block storage", false, "RAM filesystem only; no ATA/NVMe"},
        {"Networking", false, "no NIC driver, no TCP/IP"},
        {"Audio controller", false, "PC speaker tones only"},
    };
    for (u32 i = 0; i < ARRAY_SIZE(features); i++) {
        i32 ry = my + 32 + (i32)i * 18;
        canvas_circle(c, x + 30, ry + 6, 3, features[i].done ? g_theme.success : g_theme.text_dim, 255);
        font_draw(c, &font_ui_11, x + 42, ry, features[i].name, g_theme.text_muted, 245);
        font_draw(c, &font_ui_11, x + 240, ry, features[i].note, g_theme.text_dim, 225);
    }
    (void)ch;
}

static void *about_create(void) { return NULLPTR; }

static const app_vtable_t about_vt = {
    .draw = about_draw,
    .key = NULLPTR,
    .mouse = NULLPTR,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_about = {
    .id = "about",
    .name = "System Information",
    .tagline = "Version, hardware and capability matrix",
    .width = 820,
    .height = 700,
    .tint = 0x3A3428,
    .vt = &about_vt,
    .create = about_create,
};
