/* AURELIA OS - keyboard and mouse input */
#pragma once

#include <aurelia/types.h>

/* Modifier bits */
#define MOD_SHIFT 0x01
#define MOD_CTRL  0x02
#define MOD_ALT   0x04
#define MOD_META  0x08
#define MOD_CAPS  0x10

/* Named keys start at 0x100; printable keys carry their codepoint. */
enum {
    KEY_NONE = 0,
    KEY_ESCAPE = 0x100, KEY_BACKSPACE, KEY_TAB, KEY_ENTER, KEY_DELETE,
    KEY_INSERT, KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_CTRL, KEY_RIGHT_CTRL,
    KEY_LEFT_ALT, KEY_RIGHT_ALT, KEY_LEFT_META, KEY_RIGHT_META,
    KEY_CAPS_LOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_PRINT_SCREEN, KEY_SCROLL_LOCK, KEY_PAUSE
};

typedef struct {
    u32 key;        /* codepoint for printable keys, or KEY_* */
    u8  mods;
    bool pressed;
} key_event_t;

typedef struct {
    i32 x;
    i32 y;
    i32 delta_x;
    i32 delta_y;
    u8  buttons;    /* bit 0 left, bit 1 right, bit 2 middle */
    bool moved;
} mouse_state_t;

void keyboard_init(void);
void mouse_init(void);

bool input_poll_key(key_event_t *event);
bool input_key_pending(void);
void input_flush_keys(void);
u8   input_modifiers(void);

const mouse_state_t *mouse_state(void);
void mouse_set_bounds(u32 width, u32 height);
void mouse_warp(i32 x, i32 y);
bool mouse_take_wheel(i32 *delta);
