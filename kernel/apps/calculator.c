/* AURELIA OS - Calculator
 *
 * Integer fixed-point arithmetic (milli-units) so the kernel can stay compiled
 * with SSE disabled. Supports + - * / % and parentheses.
 */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/heap.h>

#define CALC_EXPR 64
#define CALC_SCALE 1000

typedef struct {
    char expr[CALC_EXPR];
    u32 length;
    char result[40];
    char history[3][CALC_EXPR];
    bool error;
    i32 mx, my;
} calc_state_t;

typedef struct {
    const char *p;
    bool error;
} calc_parser_t;

static calc_state_t *state_of(window_t *w) { return (calc_state_t *)w->state; }

static i64 parse_expression(calc_parser_t *ps);

static void skip_spaces(calc_parser_t *ps)
{
    while (*ps->p == ' ') {
        ps->p++;
    }
}

static i64 parse_number(calc_parser_t *ps)
{
    skip_spaces(ps);
    if (*ps->p == '-') {
        ps->p++;
        return -parse_number(ps);
    }
    if (*ps->p == '(') {
        ps->p++;
        i64 value = parse_expression(ps);
        skip_spaces(ps);
        if (*ps->p == ')') {
            ps->p++;
        } else {
            ps->error = true;
        }
        return value;
    }
    i64 whole = 0;
    bool any = false;
    while (*ps->p >= '0' && *ps->p <= '9') {
        whole = whole * 10 + (*ps->p - '0');
        ps->p++;
        any = true;
    }
    i64 frac = 0;
    i64 scale = 100;
    if (*ps->p == '.') {
        ps->p++;
        while (*ps->p >= '0' && *ps->p <= '9' && scale > 0) {
            frac += (*ps->p - '0') * scale;
            scale /= 10;
            ps->p++;
            any = true;
        }
    }
    if (!any) {
        ps->error = true;
    }
    return whole * CALC_SCALE + frac;
}

static i64 parse_term(calc_parser_t *ps)
{
    i64 value = parse_number(ps);
    for (;;) {
        skip_spaces(ps);
        char op = *ps->p;
        if (op != '*' && op != '/' && op != '%') {
            break;
        }
        ps->p++;
        i64 rhs = parse_number(ps);
        if ((op == '/' || op == '%') && rhs == 0) {
            ps->error = true;
            return 0;
        }
        if (op == '*') {
            value = (value * rhs) / CALC_SCALE;
        } else if (op == '/') {
            value = (value * CALC_SCALE) / rhs;
        } else {
            value = ((value / CALC_SCALE) % (rhs / CALC_SCALE)) * CALC_SCALE;
        }
    }
    return value;
}

static i64 parse_expression(calc_parser_t *ps)
{
    i64 value = parse_term(ps);
    for (;;) {
        skip_spaces(ps);
        char op = *ps->p;
        if (op != '+' && op != '-') {
            break;
        }
        ps->p++;
        i64 rhs = parse_term(ps);
        value = (op == '+') ? value + rhs : value - rhs;
    }
    return value;
}

static void format_number(i64 value, char *out, size_t size)
{
    bool negative = value < 0;
    u64 magnitude = negative ? (u64)(-value) : (u64)value;
    u64 whole = magnitude / CALC_SCALE;
    u64 frac = magnitude % CALC_SCALE;
    if (frac == 0) {
        ksnprintf(out, size, "%s%llu", negative ? "-" : "", whole);
        return;
    }
    char digits[4];
    digits[0] = (char)('0' + (frac / 100) % 10);
    digits[1] = (char)('0' + (frac / 10) % 10);
    digits[2] = (char)('0' + frac % 10);
    digits[3] = '\0';
    int len = 3;
    while (len > 1 && digits[len - 1] == '0') {
        digits[--len] = '\0';
    }
    ksnprintf(out, size, "%s%llu.%s", negative ? "-" : "", whole, digits);
}

static void calc_evaluate(calc_state_t *s)
{
    if (s->length == 0) {
        return;
    }
    calc_parser_t ps = { .p = s->expr, .error = false };
    i64 value = parse_expression(&ps);
    skip_spaces(&ps);
    if (ps.error || *ps.p != '\0') {
        ksnprintf(s->result, sizeof(s->result), "Syntax error");
        s->error = true;
        return;
    }
    for (i32 i = 2; i > 0; i--) {
        strncpy(s->history[i], s->history[i - 1], CALC_EXPR - 1);
    }
    char formatted[40];
    format_number(value, formatted, sizeof(formatted));
    ksnprintf(s->history[0], sizeof(s->history[0]), "%s = %s", s->expr, formatted);
    ksnprintf(s->result, sizeof(s->result), "%s", formatted);
    s->error = false;
}

static void calc_input(calc_state_t *s, char ch)
{
    if (s->length >= CALC_EXPR - 1) {
        return;
    }
    s->expr[s->length++] = ch;
    s->expr[s->length] = '\0';
}

static void calc_clear(calc_state_t *s)
{
    s->expr[0] = '\0';
    s->length = 0;
    s->result[0] = '\0';
    s->error = false;
}

static const char *const k_keys[5][4] = {
    {"C", "(", ")", "/"},
    {"7", "8", "9", "*"},
    {"4", "5", "6", "-"},
    {"1", "2", "3", "+"},
    {"0", ".", "%", "="},
};

static void calc_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    calc_state_t *s = state_of(w);
    if (event->key == KEY_ENTER) {
        calc_evaluate(s);
        return;
    }
    if (event->key == KEY_BACKSPACE) {
        if (s->length > 0) {
            s->expr[--s->length] = '\0';
        }
        return;
    }
    if (event->key == KEY_ESCAPE) {
        calc_clear(s);
        return;
    }
    if (event->key >= 32 && event->key < 127) {
        calc_input(s, (char)event->key);
    }
}

static void calc_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    calc_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    if (y >= 150 && y < 400) {
        i32 col = (x - 24) / 80;
        i32 row = (y - 150) / 50;
        if (col >= 0 && col < 4 && row >= 0 && row < 5) {
            const char *key = k_keys[row][col];
            if (strcmp(key, "C") == 0) {
                calc_clear(s);
            } else if (strcmp(key, "=") == 0) {
                calc_evaluate(s);
            } else {
                calc_input(s, key[0]);
            }
        }
    }
}

static void calc_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    calc_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);

    font_draw(c, &font_ui_11, x + 24, y + 14, "AURELIA CALCULATOR", g_theme.text_dim, 235);
    for (i32 i = 2; i >= 0; i--) {
        if (s->history[i][0]) {
            font_draw(c, &font_ui_11, x + 24, y + 34 + (2 - i) * 16, s->history[i],
                      g_theme.text_dim, 150);
        }
    }

    canvas_fill_round(c, x + 18, y + 84, cw - 36, 56, 10, g_theme.dark ? 0x0B0C10 : 0xEFE9DD);
    font_draw_right(c, &font_ui_28, x + cw - 30, y + 92,
                    s->length ? s->expr : "0", g_theme.text, 255);
    if (s->result[0]) {
        font_draw_right(c, &font_ui_13, x + cw - 30, y + 122, s->result,
                        s->error ? g_theme.danger : g_theme.accent, 245);
    }

    for (i32 row = 0; row < 5; row++) {
        for (i32 col = 0; col < 4; col++) {
            i32 bx = x + 24 + col * 80;
            i32 by = y + 150 + row * 50;
            const char *label = k_keys[row][col];
            bool hover = ui_hit(s->mx, s->my, bx, by, 72, 42);
            i32 style = (strcmp(label, "=") == 0) ? 1 : 0;
            if (strcmp(label, "C") == 0) {
                style = 2;
            }
            ui_button(c, bx, by, 72, 42, label, style, hover);
        }
    }
    font_draw(c, &font_ui_11, x + 24, y + ch - 22,
              "Type an expression or use the keypad. Enter evaluates.", g_theme.text_dim, 220);
}

static void *calc_create(void)
{
    calc_state_t *s = kzalloc(sizeof(calc_state_t));
    if (s) {
        strncpy(s->result, "0", sizeof(s->result) - 1);
    }
    return s;
}

static const app_vtable_t calc_vt = {
    .draw = calc_draw,
    .key = calc_key,
    .mouse = calc_mouse,
    .close = NULLPTR,
    .tick = NULLPTR,
};

const app_desc_t app_calculator = {
    .id = "calculator",
    .name = "Calculator",
    .tagline = "Arithmetic with real expression parsing",
    .width = 380,
    .height = 500,
    .tint = 0x5A4A2C,
    .vt = &calc_vt,
    .create = calc_create,
};
