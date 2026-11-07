/* AURELIA OS - PC speaker driver (PIT channel 2 + port 0x61) */
#include <aurelia/sound.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>

#define PIT_FREQUENCY 1193182u

void sound_init(void)
{
    sound_off();
    kprintf("[ok] PC speaker available (PIT channel 2)\n");
}

void sound_tone(u32 hz)
{
    if (hz < 20) {
        sound_off();
        return;
    }
    u32 divisor = PIT_FREQUENCY / hz;
    if (divisor == 0 || divisor > 0xFFFF) {
        sound_off();
        return;
    }
    outb(0x43, 0xB6);              /* channel 2, low/high byte, mode 3, binary */
    outb(0x42, (u8)(divisor & 0xFF));
    outb(0x42, (u8)(divisor >> 8));
    u8 control = inb(0x61);
    if ((control & 0x03) != 0x03) {
        outb(0x61, (u8)(control | 0x03));
    }
}

void sound_off(void)
{
    u8 control = inb(0x61);
    outb(0x61, (u8)(control & 0xFC));
}

bool sound_available(void) { return true; }
