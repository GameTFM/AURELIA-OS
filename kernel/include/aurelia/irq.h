/* AURELIA OS - hardware IRQ dispatch */
#pragma once

#include <aurelia/types.h>

typedef void (*irq_handler_t)(u8 irq, void *context);

void irq_init(void);
bool irq_register(u8 irq, irq_handler_t handler, void *context);
void irq_dispatch(u8 irq);
void irq_enable(u8 irq);
void irq_disable(u8 irq);
u64  irq_count(u8 irq);
