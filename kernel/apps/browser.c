/* AURELIA OS - AURELIA Browser
 *
 * No network stack exists in this build, so the browser is an offline document
 * reader for the operating system's own pages. It performs real navigation,
 * history, bookmarks and search over those pages. It does not claim to render
 * the modern web.
 */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>
#include <aurelia/vfs.h>
#include <aurelia/timer.h>

#define BR_PAGES 7
#define BR_HISTORY 12

typedef struct {
    int index;
    int history[BR_HISTORY];
    int history_len;
    int history_pos;
} br_tab_t;

typedef struct {
    br_tab_t tabs[2];
    i32 tab_count;
    i32 active;
    bool editing;
    char address[96];
    u32 address_len;
    bool bookmarks_open;
    bool is_bookmark[BR_PAGES];
    char query[48];
    u32 query_len;
    bool show_search;
    i32 mx, my;
} br_state_t;

typedef struct {
    const char *uri;
    const char *title;
    const char *body;
} br_page_t;

static const br_page_t g_pages[BR_PAGES] = {
    {
        "aurelia:home",
        "Welcome to AURELIA OS",
        "AURELIA is a real x86_64 operating system built from scratch: its own boot path, "
        "memory management, interrupt handling, drivers, compositor, window manager and "
        "applications.\n\nThis browser is an offline reader. There is no network device "
        "driver or TCP/IP stack in this build, so it cannot open websites. What it can do "
        "is navigate real pages bundled with the system, keep per-tab history and "
        "bookmarks, and search the documentation.\n\nTry the pages listed below, or type "
        "a word such as kernel, security or design into the address bar and press Enter "
        "to search the offline documentation.\n\n- design\n- architecture\n- filesystem\n"
        "- security\n- roadmap\n- legal",
    },
    {
        "aurelia:design",
        "The AURELIA design language",
        "Midnight #090A0E is the canvas. Champagne #D6B878 is the single accent: it marks "
        "focus, selection and the things worth looking at. Ivory #F4F0E7 carries text. "
        "Graphite #202127 and warm gray #85838A build depth without noise.\n\nSurfaces "
        "are rounded between 10 and 24 pixels. Shadows are soft and directional. Type is "
        "set in Inter for interface text, Cormorant and Playfair for display moments, and "
        "JetBrains Mono in the terminal and editor.\n\nMotion is short and purposeful: 120 "
        "to 220 milliseconds, ease-out, and never in the way of input. Reduced motion is a "
        "first-class setting, not an afterthought.\n\nThe goal is calm confidence rather "
        "than decoration. One accent per surface. Generous space. Nothing blinking for "
        "attention that does not deserve it.",
    },
    {
        "aurelia:architecture",
        "Kernel architecture",
        "AURELIA boots through Limine into a higher-half x86_64 kernel linked at "
        "0xffffffff80000000. The bootloader supplies a higher-half direct map, the memory "
        "map and a linear framebuffer; the kernel takes ownership from there.\n\n"
        "Implemented today: GDT with a 64-bit TSS, a 256-entry IDT, exception and IRQ "
        "stubs, 8259 PIC remapping, the 8254 PIT at 1000 Hz, a bitmap physical allocator, "
        "a boundary-tag heap, PS/2 keyboard and mouse drivers, the CMOS RTC, a serial "
        "console and a kernel log ring.\n\nAlso implemented: kernel-owned 4-level page "
        "tables with user/supervisor separation, a minimal ring-3 test process, a SYSCALL "
        "entry path for write/getpid/exit, and a cooperative kernel-thread context-switch "
        "self-test. The window manager and all applications still run in ring 0; there is "
        "no general process table or preemptive scheduler yet. That is an honest limitation "
        "of this alpha, not a design goal.",
    },
    {
        "aurelia:filesystem",
        "The RAM filesystem",
        "The AURELIA alpha ships a real in-memory filesystem: a tree of nodes with names, "
        "permission bits, timestamps, file contents and directory children. It supports "
        "creation, reading, writing, appending, renaming and deletion, and it backs the "
        "Terminal, Explorer, Notes and the editor.\n\nThe tree is seeded at boot with "
        "System, Applications, Users, Library, Drivers, Configuration, Temporary and "
        "Recovery. Files are capped at 256 KiB and the tree at 192 nodes.\n\nThere is no "
        "block device driver yet, so nothing survives a reboot. The VFS API is deliberately "
        "backend-agnostic: an ATA or NVMe driver can register a second backend without "
        "changing applications.",
    },
    {
        "aurelia:security",
        "Accounts and session security",
        "AURELIA has a single local account in this image. The password is never stored: "
        "the image contains a salted SHA-256 digest, and the login screen hashes the "
        "entered password with the same salt before comparing.\n\nThe session locks on "
        "demand and returns to a full-screen lock screen. There is no privilege separation "
        "yet: every application still executes in ring 0, so the account boundary is a "
        "session boundary, not a security boundary against malicious code. A boot-time "
        "ring-3 test verifies user/supervisor pages and syscall entry, but desktop apps do "
        "not use that path yet.\n\nDo not treat this alpha as hardened. A preemptive "
        "scheduler, a full process table and per-application address spaces are the next "
        "architectural step.",
    },
    {
        "aurelia:roadmap",
        "Roadmap",
        "Completed: boot, framebuffer and serial; GDT, IDT, interrupts, physical memory, "
        "heap, keyboard, mouse, timer; filesystem; compositor, window manager and desktop "
        "shell; the built-in application suite; kernel-owned paging and a ring-3 syscall "
        "foundation.\n\nNext: a preemptive scheduler and full process table, then persistent "
        "FAT32 storage, networking, package management, updates, recovery and the SDK. "
        "After that, desktop applications move out of ring 0 onto the process and syscall "
        "ABI.\n\nEach milestone is built and booted in QEMU before the next one begins, and "
        "limitations are documented rather than hidden.",
    },
    {
        "aurelia:legal",
        "Notices",
        "AURELIA OS is an original operating system project. Its kernel, compositor, "
        "window manager, shell and applications are written for this project.\n\nThe "
        "interface fonts are Inter, Cormorant Garamond, Playfair Display and JetBrains "
        "Mono, used under the SIL Open Font License. The Limine bootloader is used as the "
        "boot path and is distributed under its own licence. QEMU is used for testing.\n\n"
        "No third-party desktop environment, compositor toolkit or web engine is linked "
        "into this image.",
    },
};

static br_state_t *state_of(window_t *w) { return (br_state_t *)w->state; }

static const char *page_uri(int index)
{
    if (index < 0 || index >= BR_PAGES) {
        return "(none)";
    }
    return g_pages[index].uri;
}

static void browser_navigate(br_state_t *s, int index)
{
    br_tab_t *tab = &s->tabs[s->active];
    if (index < 0 || index >= BR_PAGES) {
        return;
    }
    if (tab->history_pos + 1 < tab->history_len) {
        tab->history_len = tab->history_pos + 1;
    }
    if (tab->history_len < BR_HISTORY) {
        tab->history[tab->history_len++] = index;
        tab->history_pos = tab->history_len - 1;
    } else {
        memmove(tab->history, tab->history + 1, sizeof(int) * (BR_HISTORY - 1));
        tab->history[BR_HISTORY - 1] = index;
        tab->history_pos = BR_HISTORY - 1;
    }
    tab->index = index;
    s->show_search = false;
    s->editing = false;
    strncpy(s->address, g_pages[index].uri, sizeof(s->address) - 1);
    s->address_len = (u32)strlen(s->address);
}

static void browser_go(br_state_t *s, const char *input)
{
    for (i32 i = 0; i < BR_PAGES; i++) {
        if (strcmp(input, g_pages[i].uri) == 0) {
            browser_navigate(s, i);
            return;
        }
    }
    strncpy(s->query, input, sizeof(s->query) - 1);
    s->query[sizeof(s->query) - 1] = '\0';
    s->query_len = (u32)strlen(s->query);
    s->show_search = true;
    s->editing = false;
}

static void browser_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    br_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    br_tab_t *tab = &s->tabs[s->active];

    if (y >= 10 && y < 40) {
        if (x >= 12 && x < 12 + s->tab_count * 150) {
            s->active = (x - 12) / 150;
            s->address_len = (u32)strlen(page_uri(s->tabs[s->active].index));
            strncpy(s->address, page_uri(s->tabs[s->active].index), sizeof(s->address) - 1);
            return;
        }
        if (s->tab_count < 2 && x >= 320 && x < 352) {
            s->tabs[s->tab_count] = s->tabs[0];
            s->tab_count++;
            s->active = s->tab_count - 1;
            return;
        }
    }
    if (y >= 48 && y < 82) {
        if (x >= 14 && x < 46) {
            if (tab->history_pos > 0) {
                tab->history_pos--;
                tab->index = tab->history[tab->history_pos];
            }
            return;
        }
        if (x >= 50 && x < 82) {
            if (tab->history_pos + 1 < tab->history_len) {
                tab->history_pos++;
                tab->index = tab->history[tab->history_pos];
            }
            return;
        }
        if (x >= 86 && x < 118) {
            browser_navigate(s, 0);
            return;
        }
        if (x >= w->w - 130 && x < w->w - 88) {
            s->bookmarks_open = !s->bookmarks_open;
            return;
        }
        if (x >= w->w - 80 && x < w->w - 24) {
            s->is_bookmark[tab->index] = !s->is_bookmark[tab->index];
            return;
        }
        if (x >= 132 && x < w->w - 136) {
            s->editing = true;
            s->address_len = (u32)strlen(s->address);
            return;
        }
    }
    if (s->bookmarks_open && x > w->w - 260) {
        i32 row = 0;
        for (i32 i = 0; i < BR_PAGES; i++) {
            if (!s->is_bookmark[i]) {
                continue;
            }
            i32 ry = 96 + row * 34;
            if (y >= ry && y < ry + 30) {
                browser_navigate(s, i);
                return;
            }
            row++;
        }
        return;
    }
    if (s->show_search) {
        i32 ry = 132;
        for (i32 i = 0; i < BR_PAGES; i++) {
            if (!strstr(g_pages[i].title, s->query) && !strstr(g_pages[i].body, s->query)) {
                continue;
            }
            if (y >= ry && y < ry + 40) {
                browser_navigate(s, i);
                return;
            }
            ry += 48;
        }
    }
}

static void browser_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    br_state_t *s = state_of(w);
    br_tab_t *tab = &s->tabs[s->active];
    if (s->editing) {
        if (event->key == KEY_ENTER) {
            char input[96];
            strncpy(input, s->address, sizeof(input) - 1);
            input[sizeof(input) - 1] = '\0';
            browser_go(s, input);
            return;
        }
        if (event->key == KEY_ESCAPE) {
            s->editing = false;
            return;
        }
        if (event->key == KEY_BACKSPACE) {
            if (s->address_len > 0) {
                s->address[--s->address_len] = '\0';
            }
            return;
        }
        if (event->key >= 32 && event->key < 127 && s->address_len < sizeof(s->address) - 1) {
            s->address[s->address_len++] = (char)event->key;
            s->address[s->address_len] = '\0';
        }
        return;
    }
    if (event->key == KEY_LEFT && tab->history_pos > 0) {
        tab->history_pos--;
        tab->index = tab->history[tab->history_pos];
    } else if (event->key == KEY_RIGHT && tab->history_pos + 1 < tab->history_len) {
        tab->history_pos++;
        tab->index = tab->history[tab->history_pos];
    } else if (event->key == 'l' && (event->mods & MOD_CTRL)) {
        s->editing = true;
    } else if (event->key == KEY_ESCAPE) {
        s->bookmarks_open = false;
        s->show_search = false;
    }
}

static void browser_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    br_state_t *s = state_of(w);
    br_tab_t *tab = &s->tabs[s->active];
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);

    /* Tabs */
    for (i32 i = 0; i < s->tab_count; i++) {
        i32 tx = x + 12 + i * 150;
        bool active = i == s->active;
        canvas_fill_round_alpha(c, tx, y + 10, 142, 32, 8,
                                active ? g_theme.surface_hover : g_theme.surface, active ? 255 : 160);
        font_draw_ellipsized(c, &font_ui_11, tx + 12, y + 19, 110,
                             g_pages[s->tabs[i].index].title,
                             active ? g_theme.text : g_theme.text_dim, 255);
        if (active) {
            canvas_fill(c, tx + 8, y + 40, 126, 2, g_theme.accent);
        }
    }
    if (s->tab_count < 2) {
        font_draw(c, &font_ui_15, x + 320, y + 14, "+", g_theme.text_muted, 200);
    }

    /* Toolbar */
    ui_button(c, x + 14, y + 50, 30, 30, "<", 0, ui_hit(s->mx, s->my, 14, y + 50, 30, 30));
    ui_button(c, x + 50, y + 50, 30, 30, ">", 0, ui_hit(s->mx, s->my, 50, y + 50, 30, 30));
    ui_button(c, x + 86, y + 50, 30, 30, "H", 0, ui_hit(s->mx, s->my, 86, y + 50, 30, 30));
    ui_field(c, x + 132, y + 50, cw - 270, 30, s->address, "Enter a page or search", s->editing);
    if (s->editing && (timer_ms() / 500) % 2 == 0) {
        i32 caret = font_text_width(&font_ui_13, s->address);
        canvas_fill(c, x + 146 + caret, y + 58, 2, 14, g_theme.accent);
    }
    ui_button(c, x + cw - 130, y + 50, 42, 30, "BM", s->bookmarks_open ? 1 : 0,
              ui_hit(s->mx, s->my, cw - 130, y + 50, 42, 30));
    ui_button(c, x + cw - 80, y + 50, 56, 30, s->is_bookmark[tab->index] ? "Saved" : "Save",
              0, ui_hit(s->mx, s->my, cw - 80, y + 50, 56, 30));

    /* Offline banner */
    canvas_fill_alpha(c, x, y + 90, cw, 26, 0x3A2A20, 90);
    font_draw(c, &font_ui_11, x + 16, y + 96,
              "Offline build: no network driver. These pages ship inside AURELIA.",
              g_theme.warning, 235);

    i32 content_y = y + 128;
    i32 content_x = x + 36;
    i32 content_w = cw - 72;

    if (s->show_search) {
        font_draw(c, &font_ui_20, content_x, content_y, "Search results", g_theme.text, 255);
        char info[128];
        ksnprintf(info, sizeof(info), "Offline pages matching \"%s\"", s->query);
        font_draw(c, &font_ui_11, content_x, content_y + 28, info, g_theme.text_muted, 235);
        i32 ry = content_y + 60;
        i32 found = 0;
        for (i32 i = 0; i < BR_PAGES; i++) {
            if (!strstr(g_pages[i].title, s->query) && !strstr(g_pages[i].body, s->query)) {
                continue;
            }
            canvas_fill_round_alpha(c, content_x, ry, content_w, 36, 8, g_theme.surface, 200);
            font_draw(c, &font_ui_13, content_x + 14, ry + 8, g_pages[i].title, g_theme.text, 255);
            font_draw_right(c, &font_ui_11, content_x + content_w - 14, ry + 10,
                            g_pages[i].uri, g_theme.accent, 200);
            ry += 48;
            found++;
        }
        if (found == 0) {
            font_draw(c, &font_ui_13, content_x, ry,
                      "Nothing found. This build cannot reach the web.", g_theme.text_dim, 255);
        }
        return;
    }

    const br_page_t *page = &g_pages[tab->index];
    font_draw(c, &font_ui_28, content_x, content_y, page->title, g_theme.text, 255);
    char uri[96];
    ksnprintf(uri, sizeof(uri), "%s  -  local document", page->uri);
    font_draw(c, &font_ui_11, content_x, content_y + 38, uri, g_theme.accent, 225);
    canvas_hline(c, content_x, content_y + 58, content_w, g_theme.border, 130);
    font_draw_wrapped(c, &font_ui_13, content_x, content_y + 74, content_w, page->body,
                      g_theme.text_muted, 250);

    if (s->bookmarks_open) {
        i32 px = x + cw - 260;
        canvas_fill_round_alpha(c, px, y + 90, 248, ch - 110, 12, g_theme.surface, 245);
        canvas_outline_round(c, px, y + 90, 248, ch - 110, 12, 1, 0xFFFFFF, 24);
        font_draw(c, &font_ui_13, px + 16, y + 104, "Bookmarks", g_theme.text, 255);
        i32 ry = y + 132;
        i32 shown = 0;
        for (i32 i = 0; i < BR_PAGES; i++) {
            if (!s->is_bookmark[i]) {
                continue;
            }
            font_draw_ellipsized(c, &font_ui_11, px + 16, ry, 210, g_pages[i].title,
                                 g_theme.text_muted, 255);
            ry += 34;
            shown++;
        }
        if (shown == 0) {
            font_draw_wrapped(c, &font_ui_11, px + 16, ry, 210,
                              "No bookmarks. Use Save on a page.", g_theme.text_dim, 230);
        }
    }

    canvas_hline(c, x, y + ch - 26, cw, g_theme.border, 120);
    font_draw(c, &font_ui_11, x + 16, y + ch - 19,
              "AURELIA Browser 1.0  -  offline document reader  -  no web engine",
              g_theme.text_dim, 220);
}

static void *browser_create(void)
{
    br_state_t *s = kzalloc(sizeof(br_state_t));
    if (!s) {
        return NULLPTR;
    }
    s->tab_count = 1;
    s->active = 0;
    s->tabs[0].index = 0;
    s->tabs[0].history[0] = 0;
    s->tabs[0].history_len = 1;
    s->tabs[0].history_pos = 0;
    s->is_bookmark[0] = true;
    s->is_bookmark[1] = true;
    strncpy(s->address, g_pages[0].uri, sizeof(s->address) - 1);
    s->address_len = (u32)strlen(s->address);
    return s;
}

static const app_vtable_t browser_vt = {
    .draw = browser_draw,
    .key = browser_key,
    .mouse = browser_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_browser = {
    .id = "browser",
    .name = "Browser",
    .tagline = "Offline documentation reader",
    .width = 1080,
    .height = 700,
    .tint = 0x2F4358,
    .vt = &browser_vt,
    .create = browser_create,
};
