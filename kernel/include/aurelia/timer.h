/* AURELIA OS - 8254 PIT system timer */
#pragma once

#include <aurelia/types.h>

#define TIMER_HZ 1000

void timer_init(void);
u64  timer_ticks(void);
u64  timer_ms(void);
u64  timer_uptime_seconds(void);
void timer_sleep_ms(u64 ms);
void timer_sleep_ticks(u64 ticks);
