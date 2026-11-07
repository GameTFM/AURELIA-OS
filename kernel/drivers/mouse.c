/* AURELIA OS - PS/2 mouse (IRQ12, 3-byte packets) */
#include <aurelia/input.h>
#include <aurelia/irq.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>

#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_DATA 0x60

static mouse_state_t g_mouse;
static u32 g_screen_w = 1920;
static u32 g_screen_h = 1080;
static u8 g_packet[3];
static u32 g_packet_index;

static bool wait_input_clear(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) {
            return true;
        }
    }
    return false;
}

static bool wait_output_full(void)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) {
            return true;
        }
    }
    return false;
}

static void mouse_write(u8 value)
{
    wait_input_clear();
    outb(PS2_COMMAND, 0xD4);
    wait_input_clear();
    outb(PS2_DATA, value);
    wait_output_full();
    (void)inb(PS2_DATA);
}

static void mouse_irq(u8 irq, void *context)
{
    (void)irq;
    (void)context;
    u8 status = inb(PS2_STATUS);
    if (!(status & 0x01) || !(status & 0x20)) {
        return;
    }
    u8 byte = inb(PS2_DATA);
    g_packet[g_packet_index++] = byte;
    if (g_packet_index < 3) {
        return;
    }
    g_packet_index = 0;

    if (!(g_packet[0] & 0x08)) {
        return; /* out of sync */
    }

    i32 dx = (i32)(i8)g_packet[1];
    i32 dy = (i32)(i8)g_packet[2];
    if (g_packet[0] & 0x40) {
        dx = 0; /* overflow */
    }
    if (g_packet[0] & 0x80) {
        dy = 0;
    }
    g_mouse.delta_x = dx;
    g_mouse.delta_y = -dy;
    g_mouse.x = i32_clamp(g_mouse.x + dx, 0, (i32)g_screen_w - 1);
    g_mouse.y = i32_clamp(g_mouse.y - dy, 0, (i32)g_screen_h - 1);
    g_mouse.buttons = (u8)(g_packet[0] & 0x07);
    g_mouse.moved = dx != 0 || dy != 0;
    /* This driver uses the 3-byte PS/2 protocol: buttons and movement only.
       Wheel support needs the IntelliMouse 4-byte protocol (roadmap). */
}

void mouse_init(void)
{
    g_mouse.x = (i32)g_screen_w / 2;
    g_mouse.y = (i32)g_screen_h / 2;
    g_mouse.buttons = 0;
    g_packet_index = 0;

    wait_input_clear();
    outb(PS2_COMMAND, 0xA8); /* enable auxiliary (mouse) port */

    /* Enable IRQ12 in the controller command byte. */
    wait_input_clear();
    outb(PS2_COMMAND, 0x20);
    wait_output_full();
    u8 status = (u8)(inb(PS2_DATA) | 0x02);
    wait_input_clear();
    outb(PS2_COMMAND, 0x60);
    wait_input_clear();
    outb(PS2_DATA, status);

    mouse_write(0xF6); /* set defaults */
    mouse_write(0xF4); /* enable data reporting */

    irq_register(12, mouse_irq, NULLPTR);
    kprintf("[ok] PS/2 mouse ready (IRQ12, 3-packet protocol)\n");
}

const mouse_state_t *mouse_state(void) { return &g_mouse; }

void mouse_set_bounds(u32 width, u32 height)
{
    g_screen_w = width;
    g_screen_h = height;
    g_mouse.x = i32_clamp(g_mouse.x, 0, (i32)width - 1);
    g_mouse.y = i32_clamp(g_mouse.y, 0, (i32)height - 1);
}

void mouse_warp(i32 x, i32 y)
{
    g_mouse.x = i32_clamp(x, 0, (i32)g_screen_w - 1);
    g_mouse.y = i32_clamp(y, 0, (i32)g_screen_h - 1);
}

bool mouse_take_wheel(i32 *delta)
{
    (void)delta;
    return false; /* not implemented with the 3-byte protocol */
}
