/* AURELIA OS - AURELIA Settings */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/vfs.h>
#include <aurelia/kstring.h>
#include <aurelia/heap.h>
#include <aurelia/pmm.h>
#include <aurelia/cpu.h>
#include <aurelia/timer.h>
#include <aurelia/fb.h>
#include <aurelia/log.h>

typedef enum {
    PANE_PERSONALIZATION = 0,
    PANE_DISPLAY,
    PANE_SOUND,
    PANE_NETWORK,
    PANE_STORAGE,
    PANE_ACCESSIBILITY,
    PANE_ABOUT,
    PANE_COUNT,
} settings_pane_t;

typedef struct {
    i32 pane;
    i32 mx, my;
} settings_state_t;

static const char *const k_panes[] = {
    "Personalization", "Display", "Sound", "Network", "Storage", "Accessibility", "About",
};

static settings_state_t *state_of(window_t *w) { return (settings_state_t *)w->state; }

static void settings_save(void)
{
    char text[160];
    ksnprintf(text, sizeof(text),
              "mode=%s\naccent=%06X\nwallpaper=%u\ndensity=%d\nbrightness=%d\nreduced_motion=%d\n",
              g_theme.dark ? "dark" : "light", g_theme.accent, g_theme.wallpaper_index,
              g_theme.density, g_theme.brightness, g_theme.reduced_motion ? 1 : 0);
    vfs_node_t *node = vfs_lookup("/Configuration/theme.conf");
    if (node) {
        vfs_write(node, text, (u32)strlen(text));
    }
}

static void settings_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)released;
    settings_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    if (x >= 10 && x < 210 && y >= 66 && y < 66 + PANE_COUNT * 42) {
        i32 index = (y - 66) / 42;
        if (index >= 0 && index < PANE_COUNT) {
            s->pane = index;
        }
        return;
    }
    /* These offsets mirror settings_draw(): cx = x + 250, cy = y + 26. */
    i32 cx = 250;
    i32 cy = 26;
    bool changed = false;
    if (s->pane == PANE_PERSONALIZATION) {
        if (y >= cy + 80 && y < cy + 112) {
            if (x >= cx && x < cx + 100) {
                theme_set_dark(true);
                changed = true;
            } else if (x >= cx + 108 && x < cx + 208) {
                theme_set_dark(false);
                changed = true;
            }
        }
        if (y >= cy + 150 && y < cy + 184) {
            static const u32 accents[] = {0xD6B878, 0xC9A27A, 0x9FA8C9, 0x9EC9B4, 0xD69A9A};
            for (i32 i = 0; i < 5; i++) {
                if (i32_abs(x - (cx + 18 + i * 40)) < 16) {
                    theme_set_accent(accents[i]);
                    changed = true;
                }
            }
        }
        if (y >= cy + 214 && y < cy + 292) {
            for (i32 i = 0; i < 4; i++) {
                i32 wx = cx + i * 132;
                if (x >= wx && x < wx + 116) {
                    theme_set_wallpaper((u32)i);
                    theme_set_dark(i != 2);
                    changed = true;
                }
            }
        }
        if (y >= cy + 336 && y < cy + 368) {
            if (x >= cx && x < cx + 110) {
                g_theme.density = 0;
                changed = true;
            } else if (x >= cx + 120 && x < cx + 230) {
                g_theme.density = 1;
                changed = true;
            } else if (x >= cx + 240 && x < cx + 350) {
                g_theme.density = 2;
                changed = true;
            }
        }
    } else if (s->pane == PANE_DISPLAY) {
        if (y >= cy + 126 && y < cy + 150) {
            i32 value = i32_clamp((x - cx) * 100 / 300, 20, 100);
            g_theme.brightness = value;
            changed = true;
        }
    } else if (s->pane == PANE_ACCESSIBILITY) {
        if (y >= cy + 50 && y < cy + 92) {
            g_theme.reduced_motion = !g_theme.reduced_motion;
            changed = true;
        }
        if (y >= cy + 164 && y < cy + 196) {
            if (x >= cx && x < cx + 110) {
                g_theme.density = 0;
                changed = true;
            } else if (x >= cx + 120 && x < cx + 230) {
                g_theme.density = 1;
                changed = true;
            } else if (x >= cx + 240 && x < cx + 350) {
                g_theme.density = 2;
                changed = true;
            }
        }
        if (y >= cy + 320 && y < cy + 352) {
            if (x >= cx && x < cx + 100) {
                theme_set_dark(true);
                changed = true;
            } else if (x >= cx + 108 && x < cx + 208) {
                theme_set_dark(false);
                changed = true;
            }
        }
    }
    if (changed) {
        settings_save();
    }
}

static void settings_segmented(canvas_t *c, i32 x, i32 y, const char *const *labels, i32 count,
                               i32 selected)
{
    for (i32 i = 0; i < count; i++) {
        bool active = i == selected;
        canvas_fill_round(c, x + i * 110, y, 102, 32, 8,
                          active ? g_theme.accent : (g_theme.dark ? 0x24252F : 0xE8E1D4));
        font_draw_centered(c, &font_ui_11, x + i * 110 + 51, y + 8, labels[i],
                           active ? 0x14151C : g_theme.text_muted, 255);
    }
}

static void settings_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    settings_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);
    canvas_fill(c, x, y, 220, ch, g_theme.dark ? 0x14151C : 0xF1EBDF);

    font_draw(c, &font_ui_20, x + 20, y + 20, "Settings", g_theme.text, 255);
    for (i32 i = 0; i < PANE_COUNT; i++) {
        i32 iy = y + 66 + i * 42;
        ui_sidebar_item(c, x + 10, iy, 200, 34, k_panes[i], s->pane == i,
                        ui_hit(s->mx, s->my, 10, iy, 200, 34));
    }

    i32 cx = x + 250;
    i32 cy = y + 26;

    if (s->pane == PANE_PERSONALIZATION) {
        ui_header(c, cx, cy, cw - 280, "Personalization",
                  "Theme, accent and desktop appearance.");
        ui_section(c, cx, cy + 60, "MODE");
        static const char *const modes[] = {"Midnight", "Ivory"};
        settings_segmented(c, cx, cy + 80, modes, 2, g_theme.dark ? 0 : 1);

        ui_section(c, cx, cy + 132, "ACCENT");
        static const u32 accents[] = {0xD6B878, 0xC9A27A, 0x9FA8C9, 0x9EC9B4, 0xD69A9A};
        for (i32 i = 0; i < 5; i++) {
            ui_swatch(c, cx + 18 + i * 40, cy + 166, 13, accents[i], g_theme.accent == accents[i]);
        }

        ui_section(c, cx, cy + 196, "WALLPAPER");
        const char *names[] = {"Solstice", "Monolith", "Dune", "Aurora"};
        for (i32 i = 0; i < 4; i++) {
            i32 wx = cx + i * 132;
            u32 base = (i == 0) ? 0x17130D : (i == 1) ? 0x191A20 : (i == 2) ? 0xE7DECD : 0x10201A;
            canvas_fill_round(c, wx, cy + 214, 116, 66, 10, base);
            if (i == 2) {
                canvas_circle(c, wx + 84, cy + 250, 22, 0xEFE3C8, 160);
            } else {
                canvas_ring(c, wx + 60, cy + 248, 22, 1, AURELIA_GOLD, 140);
            }
            if (g_theme.wallpaper_index == (u32)i) {
                canvas_outline_round(c, wx, cy + 214, 116, 66, 10, 2, g_theme.accent, 230);
            }
            font_draw(c, &font_ui_11, wx + 4, cy + 286, names[i],
                      g_theme.wallpaper_index == (u32)i ? g_theme.text : g_theme.text_dim, 235);
        }

        ui_section(c, cx, cy + 318, "INTERFACE DENSITY");
        static const char *const density[] = {"Compact", "Comfortable", "Spacious"};
        settings_segmented(c, cx, cy + 336, density, 3, g_theme.density);

        ui_section(c, cx, cy + 390, "SUMMARY");
        char summary[128];
        ksnprintf(summary, sizeof(summary), "%s theme  /  accent #%06X  /  %s density",
                  g_theme.dark ? "Midnight" : "Ivory", g_theme.accent,
                  g_theme.density == 0 ? "compact" : (g_theme.density == 2 ? "spacious" : "comfortable"));
        font_draw(c, &font_ui_13, cx, cy + 410, summary, g_theme.text_muted, 255);
        font_draw(c, &font_ui_11, cx, cy + 434,
                  "Saved to /Configuration/theme.conf in the RAM filesystem.",
                  g_theme.text_dim, 220);
    } else if (s->pane == PANE_DISPLAY) {
        ui_header(c, cx, cy, cw - 280, "Display", "Panel output and compositor settings.");
        char mode[64];
        ksnprintf(mode, sizeof(mode), "%ux%u  /  32 bpp  /  %u KiB framebuffer",
                  fb_width(), fb_height(), (u32)(g_fb.byte_size / 1024));
        font_draw(c, &font_ui_13, cx, cy + 64, mode, g_theme.text_muted, 255);
        ui_divider(c, cx, cy + 88, cw - 280);

        font_draw(c, &font_ui_13, cx, cy + 110, "Brightness", g_theme.text, 255);
        font_draw_right(c, &font_ui_11, cx + 300, cy + 112, "80-100% recommended", g_theme.text_dim, 200);
        ui_progress(c, cx, cy + 136, 300, 6, g_theme.brightness, g_theme.accent);
        canvas_circle(c, cx + 300 * g_theme.brightness / 100, cy + 139, 8, AURELIA_IVORY, 255);
        char pct[8];
        ksnprintf(pct, sizeof(pct), "%d%%", g_theme.brightness);
        font_draw(c, &font_ui_11, cx, cy + 152, pct, g_theme.accent, 255);

        ui_section(c, cx, cy + 190, "SCALING");
        font_draw(c, &font_ui_13, cx, cy + 210, "100% (native) - additional scale factors are not implemented",
                  g_theme.text_muted, 255);

        ui_section(c, cx, cy + 250, "REFRESH");
        font_draw(c, &font_ui_13, cx, cy + 270, "Compositor redraws on input at up to 50 Hz",
                  g_theme.text_muted, 255);
        ui_section(c, cx, cy + 306, "GPU");
        font_draw(c, &font_ui_13, cx, cy + 326,
                  "Linear framebuffer only - no GPU acceleration driver yet",
                  g_theme.text_muted, 255);
    } else if (s->pane == PANE_SOUND) {
        ui_header(c, cx, cy, cw - 280, "Sound", "Audio devices and output.");
        canvas_fill_round_alpha(c, cx, cy + 70, cw - 300, 96, 12, g_theme.surface, 220);
        font_draw(c, &font_ui_13, cx + 20, cy + 86, "No audio controller initialised",
                  g_theme.text, 255);
        font_draw_wrapped(c, &font_ui_11, cx + 20, cy + 108, cw - 350,
                          "This build has no AC'97/HDA driver, so system audio is unavailable. "
                          "The internal PC speaker can produce tones for the Tones player.",
                          g_theme.text_muted, 235);
        ui_section(c, cx, cy + 190, "DEVICES");
        font_draw(c, &font_ui_13, cx, cy + 210, "HD Audio  -  not detected (no PCI enumeration)",
                  g_theme.text_dim, 255);
        font_draw(c, &font_ui_13, cx, cy + 234, "PC speaker  -  available via PIT channel 2",
                  g_theme.text_muted, 255);
        ui_section(c, cx, cy + 274, "VOLUME");
        ui_progress(c, cx, cy + 294, 300, 6, 0, g_theme.accent);
        font_draw(c, &font_ui_11, cx, cy + 308, "Unavailable: requires an audio driver",
                  g_theme.danger, 230);
    } else if (s->pane == PANE_NETWORK) {
        ui_header(c, cx, cy, cw - 280, "Network", "Interfaces, addressing and connectivity.");
        canvas_fill_round_alpha(c, cx, cy + 70, cw - 300, 96, 12, g_theme.surface, 220);
        font_draw(c, &font_ui_13, cx + 20, cy + 86, "No network device", g_theme.text, 255);
        font_draw_wrapped(c, &font_ui_11, cx + 20, cy + 108, cw - 350,
                          "The AURELIA alpha has no Ethernet or Wi-Fi driver and no PCI bus "
                          "enumeration yet, so there is no link to configure. DHCP, IPv4, DNS "
                          "and sockets remain on the roadmap.",
                          g_theme.text_muted, 235);
        ui_section(c, cx, cy + 190, "INTERFACE");
        font_draw(c, &font_ui_13, cx, cy + 210, "eth0     down    no driver", g_theme.text_dim, 255);
        font_draw(c, &font_ui_13, cx, cy + 234, "wlan0    absent  no driver", g_theme.text_dim, 255);
        ui_section(c, cx, cy + 274, "DRIVER ARCHITECTURE");
        font_draw(c, &font_ui_13, cx, cy + 294,
                  "net_device registration + IRQ-driven RX/TX queues are designed but unimplemented.",
                  g_theme.text_muted, 255);
    } else if (s->pane == PANE_STORAGE) {
        ui_header(c, cx, cy, cw - 280, "Storage", "Volumes and filesystem usage.");
        u32 used = vfs_used_bytes();
        ui_section(c, cx, cy + 64, "RAM FILESYSTEM");
        ui_progress(c, cx, cy + 84, 420, 8, (i32)(used / (1024 * 1024) * 100 / 256) + 1,
                    g_theme.accent);
        char info[96];
        ksnprintf(info, sizeof(info), "%u KiB used  /  192 nodes  /  max 256 KiB per file",
                  used / 1024);
        font_draw(c, &font_ui_11, cx, cy + 102, info, g_theme.text_dim, 235);
        ui_section(c, cx, cy + 140, "PHYSICAL MEMORY");
        u64 total = pmm_total_bytes();
        u64 freeb = pmm_free_bytes();
        ui_progress(c, cx, cy + 160, 420, 8, (i32)((total - freeb) * 100 / (total ? total : 1)),
                    g_theme.accent);
        char mem[96];
        ksnprintf(mem, sizeof(mem), "%llu MiB used  /  %llu MiB usable",
                  (total - freeb) / (1024 * 1024), total / (1024 * 1024));
        font_draw(c, &font_ui_11, cx, cy + 178, mem, g_theme.text_dim, 235);
        ui_section(c, cx, cy + 216, "VOLUMES");
        font_draw(c, &font_ui_13, cx, cy + 236, "RAMFS (/)         mounted         no persistence",
                  g_theme.text_muted, 255);
        font_draw(c, &font_ui_13, cx, cy + 260, "ATA / NVMe        not implemented",
                  g_theme.text_dim, 255);
        font_draw(c, &font_ui_13, cx, cy + 284, "ISO9660 (boot)    read by firmware only",
                  g_theme.text_dim, 255);
        font_draw_wrapped(c, &font_ui_11, cx, cy + 320, cw - 320,
                          "Files are real and can be created, edited and deleted, but the tree "
                          "lives in kernel memory and is rebuilt at every boot.",
                          g_theme.text_dim, 220);
    } else if (s->pane == PANE_ACCESSIBILITY) {
        ui_header(c, cx, cy, cw - 280, "Accessibility", "Comfort and motion preferences.");
        font_draw(c, &font_ui_13, cx, cy + 66, "Reduced motion", g_theme.text, 255);
        font_draw(c, &font_ui_11, cx, cy + 86,
                  "Skip window and shell animations (applies immediately).", g_theme.text_dim, 230);
        ui_toggle(c, cx + 420, cy + 62, g_theme.reduced_motion);
        ui_divider(c, cx, cy + 122, cw - 300);
        font_draw(c, &font_ui_13, cx, cy + 140, "Interface density", g_theme.text, 255);
        static const char *const density[] = {"Compact", "Comfortable", "Spacious"};
        settings_segmented(c, cx, cy + 164, density, 3, g_theme.density);
        ui_divider(c, cx, cy + 220, cw - 300);
        font_draw(c, &font_ui_13, cx, cy + 238, "High contrast accents", g_theme.text, 255);
        font_draw(c, &font_ui_11, cx, cy + 258, "Gold on midnight exceeds 7:1 contrast for body text.",
                  g_theme.text_dim, 230);
        font_draw(c, &font_ui_13, cx, cy + 300, "Theme", g_theme.text, 255);
        static const char *const modes[] = {"Midnight", "Ivory"};
        settings_segmented(c, cx, cy + 320, modes, 2, g_theme.dark ? 0 : 1);
        font_draw(c, &font_ui_11, cx, cy + 372,
                  "Saving writes these preferences to /Configuration/theme.conf.",
                  g_theme.text_dim, 220);
    } else {
        ui_header(c, cx, cy, cw - 280, "About AURELIA OS",
                  "System information reported by the kernel.");
        const cpu_info_t *cpu = cpu_info();
        struct {
            const char *label;
            char value[96];
        } rows[8];
        ksnprintf(rows[0].value, sizeof(rows[0].value), "AURELIA OS %s \"%s\" %s",
                  AURELIA_VERSION, AURELIA_CODENAME, AURELIA_BUILD);
        rows[0].label = "Version";
        ksnprintf(rows[1].value, sizeof(rows[1].value), "AURELIA 1.0.0  /  x86_64 higher-half");
        rows[1].label = "Kernel";
        ksnprintf(rows[2].value, sizeof(rows[2].value), "%s", cpu_brand());
        rows[2].label = "Processor";
        ksnprintf(rows[3].value, sizeof(rows[3].value), "%s  /  %u core(s)", cpu_vendor(), cpu->cores);
        rows[3].label = "Vendor";
        ksnprintf(rows[4].value, sizeof(rows[4].value), "%llu MiB usable  /  %llu MiB free",
                  pmm_total_bytes() / (1024 * 1024), pmm_free_bytes() / (1024 * 1024));
        rows[4].label = "Memory";
        ksnprintf(rows[5].value, sizeof(rows[5].value), "%ux%u at 32 bpp", fb_width(), fb_height());
        rows[5].label = "Display";
        ksnprintf(rows[6].value, sizeof(rows[6].value), "%llu s  /  %llu timer ticks",
                  timer_uptime_seconds(), timer_ticks());
        rows[6].label = "Uptime";
        ksnprintf(rows[7].value, sizeof(rows[7].value), "Limine 12.9.6  /  BIOS + UEFI CD");
        rows[7].label = "Bootloader";
        for (i32 i = 0; i < 8; i++) {
            i32 ry = cy + 64 + i * 34;
            canvas_fill_round_alpha(c, cx, ry, cw - 300, 30, 7,
                                    i % 2 ? g_theme.surface : g_theme.bg_alt, 200);
            font_draw(c, &font_ui_11, cx + 14, ry + 9, rows[i].label, g_theme.text_dim, 235);
            font_draw(c, &font_ui_13, cx + 120, ry + 6, rows[i].value, g_theme.text_muted, 255);
        }
        font_draw(c, &font_ui_11, cx, cy + 356,
                  "Build: freestanding C11, Zig/clang toolchain, no libc, no external UI toolkit.",
                  g_theme.text_dim, 220);
    }
}

static void *settings_create(void)
{
    settings_state_t *s = kzalloc(sizeof(settings_state_t));
    if (s) {
        s->pane = PANE_PERSONALIZATION;
    }
    return s;
}

static const app_vtable_t settings_vt = {
    .draw = settings_draw,
    .key = NULLPTR,
    .mouse = settings_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_settings = {
    .id = "settings",
    .name = "Settings",
    .tagline = "Personalise and inspect the system",
    .width = 980,
    .height = 640,
    .tint = 0x3E4149,
    .vt = &settings_vt,
    .create = settings_create,
};
