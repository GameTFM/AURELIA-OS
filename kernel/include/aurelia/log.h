/* AURELIA OS - kernel logging: serial console, in-memory ring, formatted output */
#pragma once

#include <aurelia/types.h>

void kputc(char c);
void kputs(const char *s);
void kvprintf(const char *fmt, va_list args);
void kprintf(const char *fmt, ...);

/* Kernel log ring buffer, consumed by the desktop "Boot & Diagnostics" viewer. */
size_t klog_size(void);
size_t klog_read(char *out, size_t max);

NORETURN void kpanic(const char *fmt, ...);
NORETURN void kpanic_exception(void *frame);

/* Formatted printing into a fixed size buffer. Returns characters written. */
int ksnprintf(char *out, size_t size, const char *fmt, ...);
int kvsnprintf(char *out, size_t size, const char *fmt, va_list args);
