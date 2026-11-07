/* AURELIA OS - 16550 UART serial console */
#pragma once

#include <aurelia/types.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
bool serial_ready(void);
