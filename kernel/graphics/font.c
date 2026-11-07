/* AURELIA OS - font rendering (UTF-8, antialiased glyph coverage) */
#include <aurelia/font.h>
#include <aurelia/kstring.h>

static i32 utf8_next(const char **cursor, u32 *codepoint)
{
    const u8 *s = (const u8 *)*cursor;
    u32 cp;
    if (s[0] < 0x80) {
        cp = s[0];
        *cursor += 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = ((u32)(s[0] & 0x1F) << 6) | (u32)(s[1] & 0x3F);
        *cursor += 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = ((u32)(s[0] & 0x0F) << 12) | ((u32)(s[1] & 0x3F) << 6) | (u32)(s[2] & 0x3F);
        *cursor += 3;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = ((u32)(s[0] & 0x07) << 18) | ((u32)(s[1] & 0x3F) << 12) |
             ((u32)(s[2] & 0x3F) << 6) | (u32)(s[3] & 0x3F);
        *cursor += 4;
    } else {
        cp = '?';
        *cursor += 1;
    }
    *codepoint = cp;
    return 1;
}

const glyph_t *font_lookup(const font_t *font, u32 codepoint)
{
    if (!font) {
        return NULLPTR;
    }
    const glyph_t *fallback = NULLPTR;
    for (u16 i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].codepoint == codepoint) {
            return &font->glyphs[i];
        }
        if (font->glyphs[i].codepoint == '?') {
            fallback = &font->glyphs[i];
        }
    }
    return fallback;
}

i32 font_char_width(const font_t *font, u32 codepoint)
{
    const glyph_t *g = font_lookup(font, codepoint);
    return g ? (i32)g->advance : 0;
}

i32 font_text_width(const font_t *font, const char *text)
{
    if (!font || !text) {
        return 0;
    }
    i32 width = 0;
    i32 best = 0;
    const char *cursor = text;
    while (*cursor) {
        u32 cp;
        utf8_next(&cursor, &cp);
        if (cp == '\n') {
            best = i32_max(best, width);
            width = 0;
            continue;
        }
        width += font_char_width(font, cp);
    }
    return i32_max(best, width);
}

i32 font_height(const font_t *font) { return font ? font->pixel_size : 0; }
i32 font_line_height(const font_t *font) { return font ? font->line_height : 0; }

i32 font_draw(canvas_t *c, const font_t *font, i32 x, i32 y, const char *text,
              u32 color, u8 alpha)
{
    if (!font || !text) {
        return 0;
    }
    i32 pen = x;
    i32 line_y = y;
    const char *cursor = text;
    while (*cursor) {
        u32 cp;
        utf8_next(&cursor, &cp);
        if (cp == '\n') {
            pen = x;
            line_y += font->line_height;
            continue;
        }
        const glyph_t *g = font_lookup(font, cp);
        if (!g) {
            continue;
        }
        if (g->w && g->h) {
            i32 gx = pen + g->bearing_x;
            i32 gy = line_y + g->bearing_y;
            for (u16 row = 0; row < g->h; row++) {
                i32 py = gy + row;
                if (py < c->clip_y || py >= c->clip_y + c->clip_h) {
                    continue;
                }
                const u8 *src = font->bitmap +
                                (u64)(g->atlas_y + row) * font->atlas_width + g->atlas_x;
                for (u16 col = 0; col < g->w; col++) {
                    u8 coverage = src[col];
                    if (!coverage) {
                        continue;
                    }
                    u32 combined = (u32)coverage * alpha / 255;
                    canvas_blend_pixel(c, gx + col, py, color, (u8)combined);
                }
            }
        }
        pen += g->advance;
    }
    return pen - x;
}

i32 font_draw_wrapped(canvas_t *c, const font_t *font, i32 x, i32 y, i32 max_width,
                      const char *text, u32 color, u8 alpha)
{
    if (!font || !text) {
        return 0;
    }
    i32 line_y = y;
    i32 pen = 0;
    const char *cursor = text;
    const char *line_start = text;
    while (*cursor) {
        /* Find the longest run that fits. */
        const char *word_end = cursor;
        while (*word_end && *word_end != ' ' && *word_end != '\n') {
            word_end++;
        }
        i32 word_width = 0;
        const char *scan = cursor;
        while (scan < word_end) {
            u32 cp;
            utf8_next(&scan, &cp);
            word_width += font_char_width(font, cp);
        }
        if (pen > 0 && pen + word_width > max_width) {
            /* Emit the completed line, then restart from the current word. */
            char buffer[512];
            size_t len = (size_t)(cursor - line_start);
            if (len >= sizeof(buffer)) {
                len = sizeof(buffer) - 1;
            }
            memcpy(buffer, line_start, len);
            buffer[len] = '\0';
            font_draw(c, font, x, line_y, buffer, color, alpha);
            line_y += font->line_height;
            pen = 0;
            line_start = cursor;
            while (*line_start == ' ') {
                line_start++;
            }
            cursor = line_start;
            continue;
        }
        if (*word_end == '\n') {
            char buffer[512];
            size_t len = (size_t)(word_end - line_start);
            if (len >= sizeof(buffer)) {
                len = sizeof(buffer) - 1;
            }
            memcpy(buffer, line_start, len);
            buffer[len] = '\0';
            font_draw(c, font, x, line_y, buffer, color, alpha);
            line_y += font->line_height;
            cursor = word_end + 1;
            line_start = cursor;
            pen = 0;
            continue;
        }
        pen += word_width;
        if (*word_end == ' ') {
            pen += font_char_width(font, ' ');
            cursor = word_end + 1;
        } else {
            cursor = word_end;
        }
    }
    return font_draw(c, font, x, line_y, line_start, color, alpha);
}

i32 font_draw_centered(canvas_t *c, const font_t *font, i32 cx, i32 y,
                       const char *text, u32 color, u8 alpha)
{
    i32 width = font_text_width(font, text);
    return font_draw(c, font, cx - width / 2, y, text, color, alpha);
}

i32 font_draw_right(canvas_t *c, const font_t *font, i32 right_x, i32 y,
                    const char *text, u32 color, u8 alpha)
{
    i32 width = font_text_width(font, text);
    return font_draw(c, font, right_x - width, y, text, color, alpha);
}

i32 font_draw_ellipsized(canvas_t *c, const font_t *font, i32 x, i32 y, i32 max_width,
                         const char *text, u32 color, u8 alpha)
{
    if (font_text_width(font, text) <= max_width) {
        return font_draw(c, font, x, y, text, color, alpha);
    }
    char buffer[256];
    size_t len = strnlen(text, sizeof(buffer) - 1);
    while (len > 0) {
        memcpy(buffer, text, len);
        buffer[len] = '\0';
        strcpy(buffer + len, "...");
        if (font_text_width(font, buffer) <= max_width) {
            break;
        }
        len--;
    }
    return font_draw(c, font, x, y, buffer, color, alpha);
}
