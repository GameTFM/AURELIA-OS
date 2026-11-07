/* AURELIA OS - Programmable Interval Timer (channel 0, 1000 Hz) */
#include <aurelia/timer.h>
#include <aurelia/irq.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>

#define PIT_BASE_FREQUENCY 1193182u
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

static volatile u64 g_ticks;

static void pit_irq(u8 irq, void *context)
{
    (void)irq;
    (void)context;
    g_ticks++;
}

void timer_init(void)
{
    u32 divisor = PIT_BASE_FREQUENCY / TIMER_HZ;
    if (divisor == 0) {
        divisor = 1;
    }
    outb(PIT_COMMAND, 0x36); /* channel 0, lobyte/hibyte, mode 3, binary */
    outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));

    irq_register(0, pit_irq, NULLPTR);
    kprintf("[ok] PIT configured at %u Hz (divisor %u)\n", TIMER_HZ, divisor);
}

u64 timer_ticks(void) { return g_ticks; }
u64 timer_ms(void) { return g_ticks; }
u64 timer_uptime_seconds(void) { return g_ticks / TIMER_HZ; }

void timer_sleep_ticks(u64 ticks)
{
    u64 target = g_ticks + ticks;
    while (g_ticks < target) {
        hlt();
    }
}

void timer_sleep_ms(u64 ms) { timer_sleep_ticks(ms); }
