/* AURELIA OS - AURELIA Terminal
 *
 * A real command interpreter over the in-memory filesystem, kernel memory
 * statistics, the window manager and the theme engine. No external processes
 * exist yet: commands run synchronously inside the kernel.
 */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/vfs.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/timer.h>
#include <aurelia/heap.h>
#include <aurelia/pmm.h>
#include <aurelia/cpu.h>
#include <aurelia/rtc.h>
#include <aurelia/fb.h>
#include <aurelia/desktop.h>

#define TERM_LINES 240
#define TERM_COLS  110
#define TERM_HISTORY 24

typedef struct {
    char lines[TERM_LINES][TERM_COLS];
    u32 count;
    char input[256];
    u32 input_len;
    vfs_node_t *cwd;
    char history[TERM_HISTORY][256];
    u32 history_count;
    i32 history_index;
    u32 scroll; /* 0 = bottom */
} terminal_state_t;

static terminal_state_t *state_of(window_t *w) { return (terminal_state_t *)w->state; }

static void term_clear(terminal_state_t *s)
{
    s->count = 0;
    s->scroll = 0;
}

static void term_print(terminal_state_t *s, const char *text)
{
    if (!text) {
        return;
    }
    const char *cursor = text;
    while (*cursor) {
        if (s->count >= TERM_LINES) {
            memmove(s->lines[0], s->lines[1], sizeof(s->lines[0]) * (TERM_LINES - 1));
            s->count = TERM_LINES - 1;
        }
        char *line = s->lines[s->count];
        u32 len = 0;
        while (*cursor && *cursor != '\n' && len < TERM_COLS - 1) {
            line[len++] = *cursor++;
        }
        line[len] = '\0';
        s->count++;
        if (*cursor == '\n') {
            cursor++;
        }
    }
    s->scroll = 0;
}

static void term_println(terminal_state_t *s, const char *text)
{
    term_print(s, text);
    term_print(s, "\n");
}

static void term_printf(terminal_state_t *s, const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    term_print(s, buffer);
}

static void prompt_path(terminal_state_t *s, char *out, size_t size)
{
    vfs_absolute(s->cwd, out, size);
    if (strcmp(out, "/Users/aurelia") == 0) {
        strncpy(out, "~", size - 1);
        out[size - 1] = '\0';
    }
}

static void print_prompt(terminal_state_t *s)
{
    char path[VFS_PATH_MAX];
    prompt_path(s, path, sizeof(path));
    term_printf(s, "aurelia@solstice:%s$ ", path);
}

static void cmd_ls(terminal_state_t *s, const char *arg)
{
    vfs_node_t *dir;
    if (arg && *arg) {
        dir = vfs_resolve(s->cwd, arg);
    } else {
        dir = s->cwd;
    }
    if (!dir) {
        term_println(s, "ls: no such file or directory");
        return;
    }
    if (!dir->is_dir) {
        term_printf(s, "%-24s %8u bytes\n", dir->name, dir->size);
        return;
    }
    u32 count = 0;
    for (vfs_node_t *child = dir->children; child; child = child->next) {
        term_printf(s, "%s%-24s %8u  %s\n", child->is_dir ? "d " : "- ",
                    child->name, child->is_dir ? 0 : child->size,
                    child->is_dir ? "dir" : "file");
        count++;
    }
    if (count == 0) {
        term_println(s, "(empty)");
    }
}

static void cmd_cat(terminal_state_t *s, const char *arg)
{
    if (!arg || !*arg) {
        term_println(s, "usage: cat <file>");
        return;
    }
    vfs_node_t *node = vfs_resolve(s->cwd, arg);
    if (!node || node->is_dir) {
        term_println(s, "cat: not a readable file");
        return;
    }
    if (!node->data) {
        term_println(s, "(empty file)");
        return;
    }
    term_print(s, node->data);
    if (node->size && node->data[node->size - 1] != '\n') {
        term_print(s, "\n");
    }
}

static void cmd_mkdir(terminal_state_t *s, const char *arg, bool dir)
{
    if (!arg || !*arg) {
        term_printf(s, "usage: %s <name>\n", dir ? "mkdir" : "touch");
        return;
    }
    char path[VFS_PATH_MAX];
    vfs_join(s->cwd, arg, path, sizeof(path));
    if (vfs_lookup(path)) {
        term_printf(s, "%s: already exists\n", arg);
        return;
    }
    if (!vfs_create(path, dir)) {
        term_printf(s, "%s: cannot create\n", arg);
        return;
    }
    term_printf(s, "created %s %s\n", dir ? "directory" : "file", path);
}

static void cmd_rm(terminal_state_t *s, const char *arg)
{
    if (!arg || !*arg) {
        term_println(s, "usage: rm <path>");
        return;
    }
    vfs_node_t *node = vfs_resolve(s->cwd, arg);
    if (!node || node == vfs_root()) {
        term_printf(s, "rm: %s: not found\n", arg);
        return;
    }
    u32 children = vfs_child_count(node);
    if (children > 0) {
        term_printf(s, "rm: %s: directory not empty (%u entries)\n", arg, children);
        return;
    }
    vfs_delete(node);
    term_printf(s, "removed %s\n", arg);
}

static void cmd_cp(terminal_state_t *s, const char *args)
{
    char src_name[VFS_PATH_MAX];
    char dst_name[VFS_PATH_MAX];
    src_name[0] = dst_name[0] = '\0';
    const char *space = strchr(args, ' ');
    if (!space) {
        term_println(s, "usage: cp <source> <destination>");
        return;
    }
    size_t len = (size_t)(space - args);
    if (len >= sizeof(src_name)) {
        len = sizeof(src_name) - 1;
    }
    memcpy(src_name, args, len);
    src_name[len] = '\0';
    strncpy(dst_name, space + 1, sizeof(dst_name) - 1);
    vfs_node_t *src = vfs_resolve(s->cwd, src_name);
    if (!src || src->is_dir) {
        term_println(s, "cp: source is not a file");
        return;
    }
    vfs_node_t *dst = vfs_resolve(s->cwd, dst_name);
    char path[VFS_PATH_MAX];
    if (dst && dst->is_dir) {
        vfs_join(dst, src->name, path, sizeof(path));
        dst = vfs_lookup(path);
    }
    if (!dst) {
        vfs_join(s->cwd, dst_name, path, sizeof(path));
        dst = vfs_create(path, false);
    }
    if (!dst) {
        term_println(s, "cp: cannot create destination");
        return;
    }
    vfs_write(dst, src->data ? src->data : "", src->size);
    term_printf(s, "copied %u bytes\n", src->size);
}

static void cmd_mv(terminal_state_t *s, const char *args)
{
    const char *space = strchr(args, ' ');
    if (!space) {
        term_println(s, "usage: mv <source> <new-name>");
        return;
    }
    char src_name[VFS_PATH_MAX];
    size_t len = (size_t)(space - args);
    if (len >= sizeof(src_name)) {
        len = sizeof(src_name) - 1;
    }
    memcpy(src_name, args, len);
    src_name[len] = '\0';
    vfs_node_t *src = vfs_resolve(s->cwd, src_name);
    if (!src) {
        term_println(s, "mv: source not found");
        return;
    }
    const char *dst = space + 1;
    const char *slash = strrchr(dst, '/');
    const char *name = slash ? slash + 1 : dst;
    if (vfs_rename(src, name)) {
        term_printf(s, "renamed to %s\n", name);
    } else {
        term_println(s, "mv: cannot rename");
    }
}

static void cmd_tree(terminal_state_t *s, vfs_node_t *node, i32 depth)
{
    if (depth > 2) {
        return;
    }
    for (vfs_node_t *child = node->children; child; child = child->next) {
        char indent[16];
        i32 count = i32_min(depth * 2, 14);
        for (i32 i = 0; i < count; i++) {
            indent[i] = ' ';
        }
        indent[count] = '\0';
        term_printf(s, "%s%s%s\n", indent, child->name, child->is_dir ? "/" : "");
        if (child->is_dir) {
            cmd_tree(s, child, depth + 1);
        }
    }
}

static void cmd_neofetch(terminal_state_t *s)
{
    term_println(s, "");
    term_println(s, "        +@@+        aurelia@solstice");
    term_println(s, "      +@@@@@@+      ----------------");
    term_println(s, "    +@@+    +@@+    OS      AURELIA OS 1.0.0 Solstice (alpha)");
    term_println(s, "   +@@+  @@  +@@+   Kernel  AURELIA x86_64 (higher-half)");
    term_println(s, "   +@@+      +@@+   Shell   aurelia-sh 1.0");
    term_println(s, "    +@@+    +@@+    WM      AURELIA compositor");
    term_println(s, "      +@@@@@@+      CPU     ");
    term_println(s, "        +@@+        Memory  ");
    term_println(s, "");
    /* Replace the last two lines with real values. */
    if (s->count >= 3) {
        ksnprintf(s->lines[s->count - 3], TERM_COLS, "    +@@+    +@@+    CPU     %s", cpu_brand());
        ksnprintf(s->lines[s->count - 2], TERM_COLS, "      +@@@@@@+      Memory  %llu MiB / %llu MiB",
                  heap_used() / (1024 * 1024), pmm_total_bytes() / (1024 * 1024));
    }
}

static const struct {
    const char *name;
    const char *id;
} k_app_aliases[] = {
    {"terminal", "terminal"}, {"explorer", "explorer"}, {"files", "explorer"},
    {"settings", "settings"}, {"notes", "notes"}, {"calculator", "calculator"},
    {"calc", "calculator"}, {"monitor", "monitor"}, {"clock", "clock"},
    {"store", "store"}, {"browser", "browser"}, {"about", "about"},
    {"sysinfo", "about"}, {"editor", "editor"}, {"calendar", "calendar"},
    {"imageviewer", "imageviewer"}, {"player", "player"}, {"logs", "logs"},
};

static void cmd_open(terminal_state_t *s, const char *arg)
{
    if (!arg || !*arg) {
        term_println(s, "usage: open <app>");
        return;
    }
    for (u32 i = 0; i < ARRAY_SIZE(k_app_aliases); i++) {
        if (strcmp(k_app_aliases[i].name, arg) == 0) {
            if (desktop_launch_app(k_app_aliases[i].id)) {
                term_printf(s, "launched %s\n", k_app_aliases[i].id);
            } else {
                term_printf(s, "open: %s is unavailable\n", arg);
            }
            return;
        }
    }
    term_printf(s, "open: unknown app '%s'\n", arg);
}

static void cmd_help(terminal_state_t *s)
{
    term_println(s, "AURELIA shell commands");
    term_println(s, "  help about status neofetch uname whoami date uptime");
    term_println(s, "  ls cd pwd cat mkdir touch rm mv cp echo tree df");
    term_println(s, "  mem cpu ps apps open history theme accent clear");
    term_println(s, "  reboot shutdown exit");
    term_println(s, "");
    term_println(s, "Filesystem is a RAM tree: changes last until reboot.");
}

static void shell_execute(terminal_state_t *s, const char *line);

static void cmd_echo(terminal_state_t *s, const char *args)
{
    const char *redirect = strstr(args, " > ");
    if (redirect) {
        char path[VFS_PATH_MAX];
        strncpy(path, redirect + 3, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        char text[512];
        size_t len = (size_t)(redirect - args);
        if (len >= sizeof(text)) {
            len = sizeof(text) - 1;
        }
        memcpy(text, args, len);
        text[len] = '\0';
        char full[VFS_PATH_MAX];
        vfs_join(s->cwd, path, full, sizeof(full));
        vfs_node_t *node = vfs_lookup(full);
        if (!node) {
            node = vfs_create(full, false);
        }
        if (!node || !vfs_write(node, text, (u32)strlen(text))) {
            term_println(s, "echo: cannot write");
            return;
        }
        term_printf(s, "wrote %u bytes to %s\n", (u32)strlen(text), full);
        return;
    }
    term_println(s, args);
}

static void shell_execute(terminal_state_t *s, const char *line)
{
    while (*line == ' ') {
        line++;
    }
    if (!*line) {
        return;
    }
    char command[256];
    strncpy(command, line, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';
    char *args = strchr(command, ' ');
    if (args) {
        *args++ = '\0';
        while (*args == ' ') {
            args++;
        }
    } else {
        args = command + strlen(command);
    }

    if (strcmp(command, "help") == 0) {
        cmd_help(s);
    } else if (strcmp(command, "clear") == 0) {
        term_clear(s);
    } else if (strcmp(command, "about") == 0) {
        term_println(s, "AURELIA OS 1.0.0 \"Solstice\" alpha");
        term_println(s, "An original x86_64 operating system: custom kernel, compositor,");
        term_println(s, "window manager and applications written from scratch in C.");
        term_println(s, "No libc, no X11, no Wayland, no external UI toolkit.");
    } else if (strcmp(command, "status") == 0) {
        term_printf(s, "uptime      %llu:%02llu:%02llu\n", timer_uptime_seconds() / 3600,
                    (timer_uptime_seconds() / 60) % 60, timer_uptime_seconds() % 60);
        term_printf(s, "timer       %llu ticks at %u Hz\n", timer_ticks(), TIMER_HZ);
        term_printf(s, "memory      heap %llu KiB used of %llu KiB\n",
                    heap_used() / 1024, heap_total() / 1024);
        term_printf(s, "pmm         %llu MiB free of %llu MiB\n",
                    pmm_free_bytes() / (1024 * 1024), pmm_total_bytes() / (1024 * 1024));
        term_printf(s, "filesystem  %u bytes in RAM\n", vfs_used_bytes());
        term_printf(s, "windows     %u open\n", wm_count_on_workspace(wm_workspace()));
        term_printf(s, "display     %ux%u %ubpp\n", g_screen.width, g_screen.height, g_fb.bpp);
    } else if (strcmp(command, "neofetch") == 0) {
        cmd_neofetch(s);
    } else if (strcmp(command, "uname") == 0) {
        term_println(s, "AURELIA 1.0.0 Solstice x86_64");
    } else if (strcmp(command, "whoami") == 0) {
        term_println(s, "aurelia");
    } else if (strcmp(command, "date") == 0) {
        char date[64], time[16];
        rtc_format_date(date, sizeof(date));
        rtc_format_time(time, sizeof(time));
        term_printf(s, "%s %s\n", date, time);
    } else if (strcmp(command, "uptime") == 0) {
        term_printf(s, "up %llu seconds\n", timer_uptime_seconds());
    } else if (strcmp(command, "mem") == 0) {
        term_printf(s, "heap   %llu KiB used / %llu KiB total / %llu KiB free\n",
                    heap_used() / 1024, heap_total() / 1024, heap_free() / 1024);
        term_printf(s, "pmm    %llu MiB used / %llu MiB usable\n",
                    pmm_used_bytes() / (1024 * 1024), pmm_total_bytes() / (1024 * 1024));
    } else if (strcmp(command, "cpu") == 0) {
        term_printf(s, "%s | %s | %u core(s)\n", cpu_vendor(), cpu_brand(), cpu_info()->cores);
        term_printf(s, "APIC %s, x2APIC %s, SSE2 %s, AVX %s\n",
                    cpu_info()->has_apic ? "yes" : "no",
                    cpu_info()->has_x2apic ? "yes" : "no",
                    cpu_info()->has_sse2 ? "yes" : "no",
                    cpu_info()->has_avx ? "yes" : "no");
    } else if (strcmp(command, "ls") == 0) {
        cmd_ls(s, args);
    } else if (strcmp(command, "cd") == 0) {
        if (!*args) {
            s->cwd = vfs_lookup("/Users/aurelia");
        } else {
            vfs_node_t *target = vfs_resolve(s->cwd, args);
            if (target && target->is_dir) {
                s->cwd = target;
            } else {
                term_printf(s, "cd: %s: not a directory\n", args);
            }
        }
    } else if (strcmp(command, "pwd") == 0) {
        char path[VFS_PATH_MAX];
        vfs_absolute(s->cwd, path, sizeof(path));
        term_println(s, path);
    } else if (strcmp(command, "cat") == 0) {
        cmd_cat(s, args);
    } else if (strcmp(command, "mkdir") == 0) {
        cmd_mkdir(s, args, true);
    } else if (strcmp(command, "touch") == 0) {
        cmd_mkdir(s, args, false);
    } else if (strcmp(command, "rm") == 0) {
        cmd_rm(s, args);
    } else if (strcmp(command, "mv") == 0) {
        cmd_mv(s, args);
    } else if (strcmp(command, "cp") == 0) {
        cmd_cp(s, args);
    } else if (strcmp(command, "echo") == 0) {
        cmd_echo(s, args);
    } else if (strcmp(command, "tree") == 0) {
        vfs_node_t *base = *args ? vfs_resolve(s->cwd, args) : s->cwd;
        if (!base) {
            term_println(s, "tree: not found");
        } else {
            char path[VFS_PATH_MAX];
            vfs_absolute(base, path, sizeof(path));
            term_printf(s, "%s\n", path);
            cmd_tree(s, base, 1);
        }
    } else if (strcmp(command, "df") == 0) {
        term_printf(s, "RAMFS   %u/%u KiB used  100%% in RAM\n",
                    vfs_used_bytes() / 1024, (4u * 1024 * 1024));
    } else if (strcmp(command, "ps") == 0) {
        term_println(s, " PID  NAME              STATE     WORKSPACE");
        for (u32 i = 0; i < wm_count(); i++) {
            window_t *w = wm_window_at(i);
            if (!w->used) {
                continue;
            }
            term_printf(s, " %4u %-17s %-9s %u\n", w->id, w->app->name,
                        w->minimized ? "minimized" : (wm_is_focused(w) ? "focused" : "open"),
                        w->workspace);
        }
    } else if (strcmp(command, "apps") == 0) {
        term_println(s, "Installed applications:");
        for (u32 i = 0; i < app_count(); i++) {
            const app_desc_t *app = app_at(i);
            term_printf(s, "  %-12s %s\n", app->id, app->tagline);
        }
    } else if (strcmp(command, "open") == 0) {
        cmd_open(s, args);
    } else if (strcmp(command, "history") == 0) {
        for (u32 i = 0; i < s->history_count; i++) {
            term_printf(s, "%3u  %s\n", i + 1, s->history[i]);
        }
    } else if (strcmp(command, "theme") == 0) {
        if (strcmp(args, "dark") == 0) {
            theme_set_dark(true);
            term_println(s, "theme: midnight");
        } else if (strcmp(args, "light") == 0) {
            theme_set_dark(false);
            term_println(s, "theme: ivory");
        } else {
            term_printf(s, "theme: %s (use dark or light)\n", g_theme.dark ? "dark" : "light");
        }
    } else if (strcmp(command, "accent") == 0) {
        if (*args) {
            u32 value = (u32)strtou64(args, 16, NULLPTR);
            theme_set_accent(value & 0xFFFFFF);
            term_printf(s, "accent set to #%06X\n", value & 0xFFFFFF);
        } else {
            term_printf(s, "accent #%06X\n", g_theme.accent);
        }
    } else if (strcmp(command, "reboot") == 0) {
        term_println(s, "restarting...");
        desktop_restart();
    } else if (strcmp(command, "shutdown") == 0 || strcmp(command, "poweroff") == 0) {
        term_println(s, "shutting down...");
        desktop_shutdown();
    } else if (strcmp(command, "exit") == 0) {
        wm_close_window(wm_focused());
    } else if (strcmp(command, "sudo") == 0) {
        term_println(s, "aurelia is already the system owner; sudo is not needed.");
    } else {
        term_printf(s, "aurelia-sh: %s: command not found (try 'help')\n", command);
    }
}

static void terminal_submit(terminal_state_t *s)
{
    if (s->input_len == 0) {
        print_prompt(s);
        return;
    }
    term_printf(s, "aurelia@solstice$ %s\n", s->input);
    if (s->history_count < TERM_HISTORY) {
        strncpy(s->history[s->history_count++], s->input, 255);
        s->history[s->history_count - 1][255] = '\0';
    } else {
        memmove(s->history[0], s->history[1], sizeof(s->history[0]) * (TERM_HISTORY - 1));
        strncpy(s->history[TERM_HISTORY - 1], s->input, 255);
    }
    s->history_index = -1;
    char line[256];
    strncpy(line, s->input, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    s->input[0] = '\0';
    s->input_len = 0;
    shell_execute(s, line);
    print_prompt(s);
}

static void terminal_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    terminal_state_t *s = state_of(w);
    if (event->mods & MOD_CTRL) {
        if (event->key == 'l' || event->key == 'L') {
            term_clear(s);
            print_prompt(s);
            return;
        }
        if (event->key == 'c' || event->key == 'C') {
            term_println(s, "^C");
            s->input[0] = '\0';
            s->input_len = 0;
            print_prompt(s);
            return;
        }
    }
    if (event->key == KEY_ENTER) {
        terminal_submit(s);
        return;
    }
    if (event->key == KEY_BACKSPACE) {
        if (s->input_len > 0) {
            s->input[--s->input_len] = '\0';
        }
        return;
    }
    if (event->key == KEY_UP) {
        if (s->history_count > 0) {
            if (s->history_index < 0) {
                s->history_index = (i32)s->history_count - 1;
            } else if (s->history_index > 0) {
                s->history_index--;
            }
            strncpy(s->input, s->history[s->history_index], sizeof(s->input) - 1);
            s->input_len = (u32)strlen(s->input);
        }
        return;
    }
    if (event->key == KEY_DOWN) {
        if (s->history_index >= 0) {
            if ((u32)(s->history_index + 1) < s->history_count) {
                s->history_index++;
                strncpy(s->input, s->history[s->history_index], sizeof(s->input) - 1);
            } else {
                s->history_index = -1;
                s->input[0] = '\0';
            }
            s->input_len = (u32)strlen(s->input);
        }
        return;
    }
    if (event->key >= 32 && event->key < 127 && s->input_len < sizeof(s->input) - 1) {
        s->input[s->input_len++] = (char)event->key;
        s->input[s->input_len] = '\0';
    }
}

static void terminal_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    terminal_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, 0x0D0E13);

    i32 pad = 18;
    i32 lh = font_line_height(&font_mono_13);
    i32 available = (ch - pad * 2 - lh) / lh;
    if (available < 1) {
        available = 1;
    }
    i32 first = (i32)s->count - available;
    if (first < 0) {
        first = 0;
    }
    i32 row = y + pad;
    for (i32 i = first; i < (i32)s->count; i++) {
        const char *line = s->lines[i];
        u32 color = 0xCFCBC2;
        if (strncmp(line, "aurelia@solstice", 16) == 0) {
            color = AURELIA_GOLD;
        } else if (strncmp(line, "aurelia-sh:", 11) == 0 ||
                   strncmp(line, "usage:", 6) == 0) {
            color = 0xE29B9B;
        }
        font_draw(c, &font_mono_13, x + pad, row, line, color, 255);
        row += lh;
    }

    /* Input line */
    char path[VFS_PATH_MAX];
    prompt_path(s, path, sizeof(path));
    char prompt[VFS_PATH_MAX + 32];
    ksnprintf(prompt, sizeof(prompt), "aurelia@solstice:%s$ ", path);
    i32 prompt_w = font_draw(c, &font_mono_13, x + pad, row, prompt, AURELIA_GOLD, 255);
    i32 cursor_x = x + pad + prompt_w + font_draw(c, &font_mono_13, x + pad + prompt_w, row,
                                                  s->input, 0xF2EEE5, 255);
    if ((timer_ms() / 500) % 2 == 0) {
        canvas_fill(c, cursor_x + 1, row + 2, 2, lh - 4, AURELIA_GOLD);
    }
}

static void *terminal_create(void)
{
    terminal_state_t *s = kzalloc(sizeof(terminal_state_t));
    if (!s) {
        return NULLPTR;
    }
    s->cwd = vfs_lookup("/Users/aurelia");
    if (!s->cwd) {
        s->cwd = vfs_root();
    }
    s->history_index = -1;
    term_println(s, "AURELIA Terminal 1.0 - aurelia-sh");
    term_println(s, "Type 'help' for commands, 'neofetch' for system details.");
    term_println(s, "");
    print_prompt(s);
    return s;
}

static void terminal_close(window_t *w) { (void)w; }

static const app_vtable_t terminal_vt = {
    .draw = terminal_draw,
    .key = terminal_key,
    .mouse = NULLPTR,
    .close = terminal_close,
    .tick = NULLPTR,
};

const app_desc_t app_terminal = {
    .id = "terminal",
    .name = "Terminal",
    .tagline = "Command line and system diagnostics",
    .width = 860,
    .height = 520,
    .tint = 0x2A2B35,
    .vt = &terminal_vt,
    .create = terminal_create,
};
