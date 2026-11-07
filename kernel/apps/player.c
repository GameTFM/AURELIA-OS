/* AURELIA OS - Tones
 *
 * A real media player for the only audio device this kernel drives: the PC
 * speaker on PIT channel 2. It plays an original short melody and shows honest
 * progress. No audio codec, mixing or HD Audio driver exists yet.
 */
#include <aurelia/app.h>
#include <aurelia/wm.h>
#include <aurelia/ui.h>
#include <aurelia/theme.h>
#include <aurelia/font.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/sound.h>
#include <aurelia/heap.h>
#include <aurelia/timer.h>

typedef struct {
    u16 hz;
    u16 ms;
} note_t;

static const note_t k_melody[] = {
    {440, 280}, {523, 280}, {659, 420}, {587, 280}, {523, 280}, {440, 420},
    {392, 280}, {440, 280}, {523, 560}, {0, 160},
    {494, 280}, {587, 280}, {698, 420}, {659, 280}, {587, 280}, {494, 420},
    {440, 280}, {494, 280}, {440, 700},
};

typedef struct {
    i32 note;
    bool playing;
    u64 note_start;
    i32 mx, my;
} player_state_t;

static player_state_t *state_of(window_t *w) { return (player_state_t *)w->state; }

static u32 total_ms(void)
{
    u32 total = 0;
    for (u32 i = 0; i < ARRAY_SIZE(k_melody); i++) {
        total += k_melody[i].ms;
    }
    return total;
}

static void player_play(player_state_t *s)
{
    s->note = 0;
    s->playing = true;
    s->note_start = timer_ms();
    if (k_melody[0].hz) {
        sound_tone(k_melody[0].hz);
    }
}

static void player_stop(player_state_t *s)
{
    s->playing = false;
    sound_off();
}

static void player_tick(window_t *w)
{
    player_state_t *s = state_of(w);
    if (!s->playing) {
        return;
    }
    u64 elapsed = timer_ms() - s->note_start;
    if (elapsed < k_melody[s->note].ms) {
        return;
    }
    s->note++;
    s->note_start = timer_ms();
    if (s->note >= (i32)ARRAY_SIZE(k_melody)) {
        player_stop(s);
        return;
    }
    if (k_melody[s->note].hz) {
        sound_tone(k_melody[s->note].hz);
    } else {
        sound_off();
    }
}

static void player_mouse(window_t *w, i32 x, i32 y, u8 buttons, bool pressed, bool released)
{
    (void)buttons;
    (void)released;
    player_state_t *s = state_of(w);
    s->mx = x;
    s->my = y;
    if (!pressed) {
        return;
    }
    if (y >= w->h - 78) {
        if (x >= 24 && x < 96) {
            if (s->playing) {
                player_stop(s);
            } else {
                player_play(s);
            }
        } else if (x >= 106 && x < 178) {
            player_stop(s);
            s->note = 0;
        }
    }
}

static void player_key(window_t *w, const key_event_t *event)
{
    if (!event->pressed) {
        return;
    }
    player_state_t *s = state_of(w);
    if (event->key == KEY_ENTER) {
        if (s->playing) {
            player_stop(s);
        } else {
            player_play(s);
        }
    } else if (event->key == KEY_ESCAPE) {
        player_stop(s);
    }
}

static void player_draw(window_t *w, canvas_t *c, i32 x, i32 y, i32 cw, i32 ch)
{
    player_state_t *s = state_of(w);
    canvas_fill(c, x, y, cw, ch, g_theme.dark ? 0x101116 : 0xFBF8F2);

    canvas_fill_round(c, x + 24, y + 22, cw - 48, 130, 16, 0x15110B);
    canvas_ring(c, x + 84, y + 87, 40, 1, 0x6A5A36, 160);
    canvas_ring(c, x + 84, y + 87, 30, 2, g_theme.accent, 200);
    canvas_circle(c, x + 84, y + 87, 6, g_theme.accent, 255);
    font_draw(c, &font_ui_17, x + 142, y + 48, "Solstice Theme", g_theme.text, 255);
    font_draw(c, &font_ui_11, x + 142, y + 74, "AURELIA Tones  /  original PC speaker melody",
              g_theme.text_dim, 230);
    font_draw(c, &font_ui_11, x + 142, y + 96, "3/4  -  72 BPM  -  mono  -  1.9 kHz speaker",
              g_theme.text_dim, 200);

    u32 elapsed = 0;
    for (i32 i = 0; i < s->note && i < (i32)ARRAY_SIZE(k_melody); i++) {
        elapsed += k_melody[i].ms;
    }
    if (s->playing && s->note < (i32)ARRAY_SIZE(k_melody)) {
        u64 in_note = timer_ms() - s->note_start;
        if (in_note < k_melody[s->note].ms) {
            elapsed += (u32)in_note;
        }
    }
    u32 total = total_ms();
    i32 pct = total ? (i32)(elapsed * 100 / total) : 0;
    ui_progress(c, x + 24, y + 172, cw - 48, 6, pct, g_theme.accent);
    char times[48];
    ksnprintf(times, sizeof(times), "%u:%02u / %u:%02u", elapsed / 60000, (elapsed / 1000) % 60,
              total / 60000, (total / 1000) % 60);
    font_draw(c, &font_ui_11, x + 24, y + 186, times, g_theme.text_dim, 235);

    font_draw(c, &font_ui_11, x + 24, y + 214, "MELODY", g_theme.text_dim, 235);
    for (u32 i = 0; i < ARRAY_SIZE(k_melody); i++) {
        i32 col = (i32)i % 10;
        i32 row = (i32)i / 10;
        i32 bx = x + 24 + col * ((cw - 48) / 10);
        i32 by = y + 234 + row * 46;
        bool current = s->playing && (i32)i == s->note;
        i32 height = k_melody[i].hz ? 8 + (k_melody[i].hz - 380) / 40 : 3;
        canvas_fill_round(c, bx, by + 28 - height, (cw - 48) / 10 - 8, height, 2,
                          current ? g_theme.accent : (k_melody[i].hz ? 0x5C5A63 : 0x33343D));
    }

    ui_button(c, x + 24, y + ch - 78, 72, 34, s->playing ? "Pause" : "Play", 1,
              ui_hit(s->mx, s->my, 24, ch - 78, 72, 34));
    ui_button(c, x + 106, y + ch - 78, 72, 34, "Stop", 0,
              ui_hit(s->mx, s->my, 106, ch - 78, 72, 34));
    font_draw(c, &font_ui_11, x + 200, y + ch - 68,
              sound_available() ? "Playing through the PC speaker (PIT channel 2)."
                                : "No audio device available.",
              g_theme.text_dim, 235);
    font_draw(c, &font_ui_11, x + 24, y + ch - 30,
              "No HDA/AC'97 driver yet: system audio, mixing and codecs are unimplemented.",
              g_theme.text_dim, 205);
}

static void *player_create(void)
{
    player_state_t *s = kzalloc(sizeof(player_state_t));
    if (s) {
        s->note = 0;
    }
    return s;
}

static void player_close(window_t *w)
{
    player_state_t *s = state_of(w);
    if (s) {
        player_stop(s);
    }
}

static const app_vtable_t player_vt = {
    .draw = player_draw,
    .key = player_key,
    .mouse = player_mouse,
    .close = player_close,
    .tick = player_tick,
};

const app_desc_t app_player = {
    .id = "player",
    .name = "Tones",
    .tagline = "PC speaker melody player",
    .width = 620,
    .height = 520,
    .tint = 0x5A3A3A,
    .vt = &player_vt,
    .create = player_create,
};
