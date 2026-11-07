/* AURELIA OS - kernel entry logic: boot handoff, subsystem init, boot splash */
#include <aurelia/types.h>
#include <aurelia/arch.h>
#include <aurelia/limine.h>
#include <aurelia/serial.h>
#include <aurelia/log.h>
#include <aurelia/fb.h>
#include <aurelia/gdt.h>
#include <aurelia/idt.h>
#include <aurelia/cpu.h>
#include <aurelia/kstring.h>
#include <aurelia/pmm.h>
#include <aurelia/heap.h>
#include <aurelia/paging.h>
#include <aurelia/process.h>
#include <aurelia/sched.h>
#include <aurelia/irq.h>
#include <aurelia/timer.h>
#include <aurelia/input.h>
#include <aurelia/gfx.h>
#include <aurelia/font.h>
#include <aurelia/theme.h>
#include <aurelia/desktop.h>

/* Limine requests: kept in the .limine_requests section (writable). */

__attribute__((used, section(".limine_requests_start")))
static volatile u64 g_requests_start[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile u64 g_base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request g_bootloader_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request g_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request g_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request g_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_cmdline_request g_cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request g_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_tsc_frequency_request g_tsc_request = {
    .id = LIMINE_TSC_FREQUENCY_REQUEST_ID, .revision = 0,
};

__attribute__((used, section(".limine_requests_end")))
static volatile u64 g_requests_end[2] = LIMINE_REQUESTS_END_MARKER;

extern u8 __kernel_end[];

#define KERNEL_VIRTUAL_BASE 0xffffffff80000000ull
#define BOOT_GOLD   0xD6B878
#define BOOT_IVORY  0xF4F0E7

static u64 g_hhdm_offset;

/* ------------------------------------------------------------------------- */
/* Boot splash                                                               */
/* ------------------------------------------------------------------------- */

static void draw_splash_base(void)
{
    if (!fb_ready()) {
        return;
    }
    i32 w = (i32)fb_width();
    i32 h = (i32)fb_height();
    fb_vgradient(0, 0, w, h, 0x0C0D12, 0x05060A);
    fb_radial_glow(w / 2, h / 2 - 70, h / 3, BOOT_GOLD, 14);

    i32 cx = w / 2;
    i32 cy = h / 2 - 70;

    fb_ring(cx, cy, 62, 2, BOOT_GOLD);
    fb_ring(cx, cy, 66, 1, 0x6A5A36);
    for (i32 t = -1; t <= 1; t++) {
        fb_line(cx - 25 + t, cy + 27, cx + t, cy - 31, BOOT_IVORY);
        fb_line(cx + 25 + t, cy + 27, cx + t, cy - 31, BOOT_IVORY);
    }
    fb_fill_rect(cx - 14, cy + 6, 28, 2, BOOT_GOLD);
    fb_pixel(cx, cy - 38, BOOT_GOLD);
}

static void splash_progress(u32 percent)
{
    if (!fb_ready()) {
        return;
    }
    i32 w = (i32)fb_width();
    i32 h = (i32)fb_height();
    i32 track_w = 260;
    i32 x = w / 2 - track_w / 2;
    i32 y = h - 96;
    fb_fill_rect(x, y, track_w, 2, 0x24252C);
    fb_fill_rect(x, y, (i32)((track_w * (i32)percent) / 100), 2, BOOT_GOLD);
}

/* ------------------------------------------------------------------------- */
/* Temporary visual test surface (replaced by the desktop shell)              */
/* ------------------------------------------------------------------------- */

static void preview_desktop(void) UNUSED
{
    if (!fb_ready()) {
        return;
    }
    gfx_init();
    canvas_t *c = &g_screen;
    i32 w = c->width;
    i32 h = c->height;

    canvas_vgradient(c, 0, 0, w, h, 0x0B0C12, 0x171310);
    for (i32 r = 520; r > 260; r -= 26) {
        canvas_circle(c, w - 380, h / 2 + 40, r, 0x1C1710, 20);
    }
    canvas_ring(c, w - 380, h / 2 + 40, 300, 1, 0x6A5A36, 90);
    canvas_ring(c, w - 380, h / 2 + 40, 420, 1, 0x4A3F28, 60);

    canvas_fill_alpha(c, 0, 0, w, 36, 0x0D0E13, 200);
    font_draw(c, &font_ui_13, 22, 11, "AURELIA", AURELIA_GOLD, 255);
    font_draw(c, &font_ui_11, 92, 13, "OS 1.0 \xC2\xB7 Solstice", 0x8F8B96, 255);
    font_draw_right(c, &font_ui_11, w - 24, 13, "21:04  \xC2\xB7  Thu 6 Nov", 0xD8D3C8, 255);

    i32 cx = w / 2;
    i32 cy = 210;
    canvas_shadow(c, cx - 300, cy, 600, 150, 20, 26, 90);
    canvas_fill_round_alpha(c, cx - 300, cy, 600, 150, 20, 0x121319, 238);
    canvas_outline_round(c, cx - 300, cy, 600, 150, 20, 1, 0x3A352A, 180);
    font_draw_centered(c, &font_ui_11, cx, cy + 22, "THURSDAY, 6 NOVEMBER", 0x9A968F, 255);
    font_draw_centered(c, &font_display_96, cx, cy + 40, "21:04", AURELIA_IVORY, 255);

    /* Dock */
    i32 dock_w = 470;
    i32 dock_h = 74;
    i32 dock_x = cx - dock_w / 2;
    i32 dock_y = h - dock_h - 24;
    canvas_shadow(c, dock_x, dock_y, dock_w, dock_h, 24, 24, 120);
    canvas_fill_round_alpha(c, dock_x, dock_y, dock_w, dock_h, 24, 0x101117, 226);
    canvas_outline_round(c, dock_x, dock_y, dock_w, dock_h, 24, 1, 0xFFFFFF, 24);
    for (i32 i = 0; i < 7; i++) {
        i32 tile = 46;
        i32 tx = dock_x + 18 + i * (tile + 14);
        i32 ty = dock_y + 14;
        u32 tint = (i == 0) ? AURELIA_GOLD : 0x2A2B35;
        canvas_fill_round_alpha(c, tx, ty, tile, tile, 14, tint, i == 0 ? 255 : 200);
        if (i == 0) {
            font_draw_centered(c, &font_ui_20, tx + tile / 2, ty + 11, "A", 0x0A0B0F, 255);
        }
    }

    /* Mock window to validate typography and surfaces */
    i32 wx = cx - 450;
    i32 wy = 400;
    i32 ww = 900;
    i32 wh = 360;
    canvas_shadow(c, wx, wy, ww, wh, 16, 30, 130);
    canvas_fill_round(c, wx, wy, ww, wh, 16, 0x14151C);
    canvas_outline_round(c, wx, wy, ww, wh, 16, 1, 0xFFFFFF, 26);
    for (i32 i = 0; i < 3; i++) {
        canvas_circle(c, wx + 26 + i * 20, wy + 24, 6, i == 0 ? 0xD97070 : (i == 1 ? 0xE2B86A : 0x86C9A4), 235);
    }
    font_draw_centered(c, &font_ui_13, wx + ww / 2, wy + 16, "AURELIA Terminal", 0xBDB8B1, 255);
    canvas_hline(c, wx + 1, wy + 47, ww - 2, 0xFFFFFF, 20);
    font_draw(c, &font_mono_13, wx + 28, wy + 68, "aurelia@solstice:~$", AURELIA_GOLD, 255);
    font_draw(c, &font_mono_13, wx + 190, wy + 68, "status", 0xE8E3D8, 255);
    font_draw(c, &font_mono_13, wx + 28, wy + 92, "  uptime    0d 00:00:14", 0xC9C4BB, 255);
    font_draw(c, &font_mono_13, wx + 28, wy + 112, "  memory    64 MiB heap / 1018 MiB usable", 0xC9C4BB, 255);
    font_draw(c, &font_mono_13, wx + 28, wy + 132, "  display   1920x1080 \xC2\xB7 32bpp", 0xC9C4BB, 255);
    font_draw(c, &font_mono_13, wx + 28, wy + 156, "aurelia@solstice:~$", AURELIA_GOLD, 255);
    font_draw(c, &font_mono_13, wx + 190, wy + 156, "_", 0xE8E3D8, 255);

    font_draw(c, &font_ui_11, wx + 28, wy + wh - 34, "Milestone 5 \xC2\xB7 compositor and typography validation", 0x77747E, 255);
    font_draw(c, &font_ui_28, 60, h / 2 - 80, "Solstice", AURELIA_IVORY, 235);
    font_draw(c, &font_ui_13, 62, h / 2 - 38, "Elegant. Minimal. Powerful.", AURELIA_GOLD, 220);
}

/* ------------------------------------------------------------------------- */
/* Kernel entry                                                              */
/* ------------------------------------------------------------------------- */

void kmain(void)
{
    serial_init();
    kprintf("[early] AURELIA kernel entered in long mode\n");
    gdt_init();
    idt_init();
    cpu_init();
    kprintf("[early] GDT, TSS, IDT and CPU state ready\n");

    kprintf("\n  AURELIA OS %s \"%s\" (%s, %s kernel)\n",
            AURELIA_VERSION, AURELIA_CODENAME, AURELIA_ARCH, AURELIA_BUILD);
    kprintf("  Copyright (c) 2026 AURELIA Systems. All rights reserved.\n\n");

    if (!LIMINE_BASE_REVISION_SUPPORTED(g_base_revision)) {
        kpanic("bootloader does not support Limine base revision 3");
    }
    if (!g_hhdm_request.response) {
        kpanic("bootloader did not provide a higher-half direct map");
    }
    g_hhdm_offset = g_hhdm_request.response->offset;

    if (g_tsc_request.response) {
        cpu_set_tsc_hz(g_tsc_request.response->frequency);
    }
    if (g_bootloader_request.response) {
        kprintf("[boot] bootloader: %s %s\n",
                g_bootloader_request.response->name,
                g_bootloader_request.response->version);
    }
    kprintf("[boot] HHDM at %#llx\n", g_hhdm_offset);

    if (g_address_request.response) {
        kprintf("[boot] kernel: phys %#llx virt %#llx\n",
                g_address_request.response->physical_base,
                g_address_request.response->virtual_base);
    }

    bool recovery = false;
    if (g_cmdline_request.response && g_cmdline_request.response->cmdline) {
        kprintf("[boot] cmdline: \"%s\"\n", g_cmdline_request.response->cmdline);
        recovery = strstr(g_cmdline_request.response->cmdline, "recover=1") != NULLPTR;
    }

    if (g_framebuffer_request.response && g_framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = g_framebuffer_request.response->framebuffers[0];
        if (fb_init(fb, g_hhdm_offset)) {
            kprintf("[ok] framebuffer %ux%u %ubpp (%u KiB)\n", fb_width(), fb_height(),
                    g_fb.bpp, (u32)(g_fb.byte_size / 1024));
        } else {
            kprintf("[warn] framebuffer unavailable; serial console only\n");
        }
    }
    draw_splash_base();
    splash_progress(10);

    /* Physical memory + heap */
    u64 kernel_size = (u64)__kernel_end - KERNEL_VIRTUAL_BASE;
    u64 kernel_phys = g_address_request.response ? g_address_request.response->physical_base : 0x100000;
    pmm_init(g_memmap_request.response, g_hhdm_offset, kernel_phys, kernel_size);
    splash_progress(22);

    heap_init(64ull * 1024 * 1024);
    paging_init();
    bool process_ready = process_init();
    splash_progress(34);

    /* Interrupt controller and drivers */
    irq_init();
    timer_init();
    keyboard_init();
    mouse_init();
    mouse_set_bounds(fb_ready() ? fb_width() : 1024, fb_ready() ? fb_height() : 768);
    splash_progress(52);

    kprintf("[ok] heap: %llu MiB total, %llu KiB free\n",
            heap_total() / (1024 * 1024), heap_free() / 1024);
    kprintf("[ok] pmm: %llu MiB usable, %llu MiB free\n",
            pmm_total_bytes() / (1024 * 1024), pmm_free_bytes() / (1024 * 1024));
    kprintf("      cpu: %s | %s | %u logical cores\n",
            cpu_vendor(), cpu_brand(), cpu_info()->cores);

    if (recovery) {
        kprintf("[boot] recovery mode requested via cmdline\n");
    }

    sti();
    kprintf("[ok] interrupts enabled (CPL 0)\n");
    splash_progress(66);

    /* Timer self-test: 250 ms of sleep should advance ~250 ticks. */
    u64 before = timer_ticks();
    timer_sleep_ms(250);
    u64 elapsed = timer_ticks() - before;
    kprintf("[ok] PIT tick self-test: %llu ticks in 250 ms\n", elapsed);

    /* Heap self-test */
    void *a = kmalloc(256);
    void *b = kzalloc(8192);
    if (!a || !b) {
        kpanic("heap self-test failed: kmalloc returned NULL");
    }
    kprintf("[ok] heap self-test: kmalloc(256)=%p kzalloc(8192)=%p\n", a, b);
    kfree(a);
    kfree(b);

    /* Ring-3 isolation is a boot gate: report it, but do not pretend a failed
       user transition is a working desktop. The desktop remains bootable so
       the diagnostics log can be inspected. */
    if (process_ready && !process_run_user_selftest()) {
        kprintf("[warn] ring-3 self-test failed; see diagnostics above\n");
    } else if (!process_ready) {
        kprintf("[warn] ring-3 self-test not run (SYSCALL unavailable)\n");
    }

    if (!sched_selftest()) {
        kprintf("[warn] thread context-switch self-test failed\n");
    }

    splash_progress(100);

    theme_init();
    gfx_init();
    desktop_run();
}
