/* AURELIA OS - PC speaker (PIT channel 2) */
#pragma once

#include <aurelia/types.h>

void sound_init(void);
void sound_tone(u32 hz);
void sound_off(void);
bool sound_available(void);
