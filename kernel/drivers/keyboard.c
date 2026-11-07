/* AURELIA OS - PS/2 keyboard (scancode set 1) */
#include <aurelia/input.h>
#include <aurelia/irq.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>

#define KBD_DATA   0x60
#define KBD_STATUS 0x64

#define KEY_QUEUE_SIZE 256

static key_event_t g_queue[KEY_QUEUE_SIZE];
static volatile u32 g_head;
static volatile u32 g_tail;
static u8 g_mods;
static bool g_extended;

static const char g_map_lower[128] = {
    [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',
    [0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0D]='=',
    [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',
    [0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1A]='[',[0x1B]=']',
    [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',
    [0x24]='j',[0x25]='k',[0x26]='l',[0x27]=';',[0x28]='\'',[0x29]='`',
    [0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',
    [0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' ',
    [0x4A]='-',[0x4E]='+',
};

static const char g_map_upper[128] = {
    [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',
    [0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',[0x0D]='+',
    [0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',
    [0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',[0x1A]='{',[0x1B]='}',
    [0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',
    [0x24]='J',[0x25]='K',[0x26]='L',[0x27]=':',[0x28]='"',[0x29]='~',
    [0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',
    [0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]='?',[0x39]=' ',
    [0x4A]='-',[0x4E]='+',
};

static void queue_push(u32 key, bool pressed)
{
    u32 next = (g_head + 1) % KEY_QUEUE_SIZE;
    if (next == g_tail) {
        return; /* drop when full rather than block the IRQ */
    }
    g_queue[g_head].key = key;
    g_queue[g_head].mods = g_mods;
    g_queue[g_head].pressed = pressed;
    g_head = next;
}

static u32 named_key(u8 code)
{
    switch (code) {
    case 0x01: return KEY_ESCAPE;
    case 0x0E: return KEY_BACKSPACE;
    case 0x0F: return KEY_TAB;
    case 0x1C: return KEY_ENTER;
    case 0x3B: return KEY_F1;  case 0x3C: return KEY_F2;
    case 0x3D: return KEY_F3;  case 0x3E: return KEY_F4;
    case 0x3F: return KEY_F5;  case 0x40: return KEY_F6;
    case 0x41: return KEY_F7;  case 0x42: return KEY_F8;
    case 0x43: return KEY_F9;  case 0x44: return KEY_F10;
    case 0x57: return KEY_F11; case 0x58: return KEY_F12;
    case 0x1D: return KEY_LEFT_CTRL;
    case 0x2A: return KEY_LEFT_SHIFT;
    case 0x36: return KEY_RIGHT_SHIFT;
    case 0x38: return KEY_LEFT_ALT;
    case 0x3A: return KEY_CAPS_LOCK;
    default: return KEY_NONE;
    }
}

static u32 named_key_extended(u8 code)
{
    switch (code) {
    case 0x48: return KEY_UP;
    case 0x50: return KEY_DOWN;
    case 0x4B: return KEY_LEFT;
    case 0x4D: return KEY_RIGHT;
    case 0x47: return KEY_HOME;
    case 0x4F: return KEY_END;
    case 0x49: return KEY_PAGE_UP;
    case 0x51: return KEY_PAGE_DOWN;
    case 0x53: return KEY_DELETE;
    case 0x52: return KEY_INSERT;
    case 0x1D: return KEY_RIGHT_CTRL;
    case 0x38: return KEY_RIGHT_ALT;
    case 0x5B: return KEY_LEFT_META;
    case 0x5C: return KEY_RIGHT_META;
    default: return KEY_NONE;
    }
}

static void keyboard_irq(u8 irq, void *context)
{
    (void)irq;
    (void)context;
    u8 status = inb(KBD_STATUS);
    if (!(status & 0x01)) {
        return;
    }
    u8 scancode = inb(KBD_DATA);

    if (scancode == 0xE0) {
        g_extended = true;
        return;
    }

    bool pressed = (scancode & 0x80) == 0;
    u8 code = (u8)(scancode & 0x7F);

    if (g_extended) {
        g_extended = false;
        u32 key = named_key_extended(code);
        if (key != KEY_NONE) {
            queue_push(key, pressed);
        }
        return;
    }

    switch (code) {
    case 0x2A: g_mods = pressed ? (u8)(g_mods | MOD_SHIFT) : (u8)(g_mods & ~MOD_SHIFT); break;
    case 0x36: g_mods = pressed ? (u8)(g_mods | MOD_SHIFT) : (u8)(g_mods & ~MOD_SHIFT); break;
    case 0x1D: g_mods = pressed ? (u8)(g_mods | MOD_CTRL) : (u8)(g_mods & ~MOD_CTRL); break;
    case 0x38: g_mods = pressed ? (u8)(g_mods | MOD_ALT) : (u8)(g_mods & ~MOD_ALT); break;
    case 0x3A:
        if (pressed) {
            g_mods ^= MOD_CAPS;
        }
        break;
    default:
        break;
    }

    u32 key = named_key(code);
    if (key != KEY_NONE) {
        queue_push(key, pressed);
        return;
    }

    bool shifted = (g_mods & MOD_SHIFT) != 0;
    bool caps = (g_mods & MOD_CAPS) != 0;
    char ch = shifted ? g_map_upper[code] : g_map_lower[code];
    if (caps && ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    } else if (caps && ch >= 'A' && ch <= 'Z') {
        ch = (char)(ch - 'A' + 'a');
    }
    if (ch && pressed) {
        queue_push((u32)(u8)ch, pressed);
    }
}

bool input_poll_key(key_event_t *event)
{
    if (g_tail == g_head) {
        return false;
    }
    if (event) {
        *event = g_queue[g_tail];
    }
    g_tail = (g_tail + 1) % KEY_QUEUE_SIZE;
    return true;
}

bool input_key_pending(void) { return g_tail != g_head; }

void input_flush_keys(void) { g_tail = g_head; }

u8 input_modifiers(void) { return g_mods; }

void keyboard_init(void)
{
    /* Drain anything the firmware left in the controller buffer. */
    for (int i = 0; i < 16 && (inb(KBD_STATUS) & 0x01); i++) {
        (void)inb(KBD_DATA);
    }
    g_head = g_tail = 0;
    g_mods = 0;
    g_extended = false;
    irq_register(1, keyboard_irq, NULLPTR);
    kprintf("[ok] PS/2 keyboard ready (IRQ1)\n");
}
