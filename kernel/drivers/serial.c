/* AURELIA OS - 16550 UART serial driver (COM1) */
#include <aurelia/serial.h>
#include <aurelia/arch.h>

#define COM1_PORT 0x3F8

static bool g_serial_ready = false;

void serial_init(void)
{
    outb(COM1_PORT + 1, 0x00); /* disable interrupts */
    outb(COM1_PORT + 3, 0x80); /* enable DLAB */
    outb(COM1_PORT + 0, 0x01); /* divisor 1 -> 115200 baud */
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03); /* 8N1 */
    outb(COM1_PORT + 2, 0xC7); /* FIFO enable, clear, 14 byte threshold */
    outb(COM1_PORT + 4, 0x0B); /* IRQs off, RTS/DSR set */
    outb(COM1_PORT + 4, 0x1E); /* loopback test */
    outb(COM1_PORT + 0, 0xAE);
    for (u32 spin = 0; spin < 100000u; spin++) {
        if (inb(COM1_PORT + 5) & 0x01) {
            break;
        }
    }
    (void)inb(COM1_PORT + 0);
    outb(COM1_PORT + 4, 0x0F);
    g_serial_ready = true;
}

bool serial_ready(void) { return g_serial_ready; }

static bool tx_empty(void) { return (inb(COM1_PORT + 5) & 0x20) != 0; }

void serial_putc(char c)
{
    if (!g_serial_ready) {
        return;
    }
    if (c == '\n') {
        serial_putc('\r');
    }
    u32 spin = 0;
    while (!tx_empty()) {
        if (++spin > 1000000u) {
            return;
        }
    }
    outb(COM1_PORT, (u8)c);
}

void serial_write(const char *s)
{
    while (*s) {
        serial_putc(*s++);
    }
}
