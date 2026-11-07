/* AURELIA OS - kernel log implementation */
#include <aurelia/log.h>
#include <aurelia/arch.h>
#include <aurelia/serial.h>
#include <aurelia/kstring.h>
#include <aurelia/idt.h>

#define KLOG_RING_SIZE 32768

static char g_klog[KLOG_RING_SIZE];
static size_t g_klog_len = 0;
static bool g_klog_dropped = false;
static volatile bool g_panicking = false;

size_t klog_size(void) { return g_klog_len; }

size_t klog_read(char *out, size_t max)
{
    size_t n = g_klog_len < max ? g_klog_len : max;
    memcpy(out, g_klog, n);
    return n;
}

static void klog_append(char c)
{
    if (g_klog_len < KLOG_RING_SIZE - 1) {
        g_klog[g_klog_len++] = c;
    } else if (!g_klog_dropped) {
        const char *msg = "\n[log] ring buffer full; older entries retained\n";
        for (const char *p = msg; *p && g_klog_len < KLOG_RING_SIZE - 1; p++) {
            g_klog[g_klog_len++] = *p;
        }
        g_klog_dropped = true;
    }
}

void kputc(char c)
{
    serial_putc(c);
    klog_append(c);
}

void kputs(const char *s)
{
    while (*s) {
        kputc(*s++);
    }
}

typedef struct {
    char *buffer;
    size_t size;
    size_t written;
    bool to_console;
} print_sink_t;

static void sink_char(print_sink_t *sink, char c)
{
    if (sink->to_console) {
        serial_putc(c);
    } else if (sink->written + 1 < sink->size) {
        sink->buffer[sink->written] = c;
    }
    sink->written++;
}

static void sink_str(print_sink_t *sink, const char *s)
{
    while (*s) {
        sink_char(sink, *s++);
    }
}

static void sink_uint(print_sink_t *sink, u64 value, u32 base, bool upper,
                      u32 width, char pad)
{
    static const char digits_lower[] = "0123456789abcdef";
    static const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = upper ? digits_upper : digits_lower;
    char tmp[66];
    int len = 0;
    if (value == 0) {
        tmp[len++] = '0';
    }
    while (value) {
        tmp[len++] = digits[value % base];
        value /= base;
    }
    for (u32 i = (u32)len; i < width; i++) {
        sink_char(sink, pad);
    }
    while (len > 0) {
        sink_char(sink, tmp[--len]);
    }
}

static void sink_int(print_sink_t *sink, i64 value, u32 width, char pad, bool plus)
{
    if (value < 0) {
        sink_char(sink, '-');
        sink_uint(sink, (u64)(-(value + 1)) + 1, 10, false, width, pad);
    } else {
        if (plus) {
            sink_char(sink, '+');
        }
        sink_uint(sink, (u64)value, 10, false, width, pad);
    }
}

static void format_into(print_sink_t *sink, const char *fmt, va_list args)
{
    while (*fmt) {
        if (*fmt != '%') {
            sink_char(sink, *fmt++);
            continue;
        }
        fmt++;

        bool zero_pad = false;
        bool alternate = false;
        bool plus = false;
        for (;;) {
            if (*fmt == '0') {
                zero_pad = true;
                fmt++;
            } else if (*fmt == '#') {
                alternate = true;
                fmt++;
            } else if (*fmt == '+') {
                plus = true;
                fmt++;
            } else if (*fmt == '-' || *fmt == ' ') {
                fmt++;
            } else {
                break;
            }
        }

        const char *width_start = fmt;
        u32 width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (u32)(*fmt - '0');
            fmt++;
        }
        if (fmt == width_start) {
            width = 0;
        }

        bool is_long = false;
        bool is_longlong = false;
        while (*fmt == 'l') {
            if (is_long) {
                is_longlong = true;
            }
            is_long = true;
            fmt++;
        }
        bool is_size = false;
        if (*fmt == 'z') {
            is_size = true;
            fmt++;
        }

        char pad = zero_pad ? '0' : ' ';

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(args, const char *);
            sink_str(sink, s ? s : "(null)");
            break;
        }
        case 'c': {
            int c = va_arg(args, int);
            sink_char(sink, (char)c);
            break;
        }
        case 'd':
        case 'i': {
            i64 v;
            if (is_longlong) {
                v = va_arg(args, i64);
            } else if (is_long) {
                v = (i64)va_arg(args, long);
            } else if (is_size) {
                v = (i64)va_arg(args, size_t);
            } else {
                v = (i64)va_arg(args, int);
            }
            sink_int(sink, v, width, pad, plus);
            break;
        }
        case 'u':
        case 'x':
        case 'X':
        case 'o':
        case 'p': {
            u64 v;
            if (*fmt == 'p') {
                v = (u64)(uptr)va_arg(args, void *);
            } else if (is_longlong) {
                v = va_arg(args, u64);
            } else if (is_long) {
                v = (u64)va_arg(args, unsigned long);
            } else if (is_size) {
                v = (u64)va_arg(args, size_t);
            } else {
                v = (u64)va_arg(args, unsigned int);
            }
            if (*fmt == 'u') {
                sink_uint(sink, v, 10, false, width, pad);
            } else if (*fmt == 'x') {
                if (alternate) {
                    sink_str(sink, "0x");
                }
                sink_uint(sink, v, 16, false, width, pad);
            } else if (*fmt == 'X') {
                if (alternate) {
                    sink_str(sink, "0x");
                }
                sink_uint(sink, v, 16, true, width, pad);
            } else if (*fmt == 'o') {
                sink_uint(sink, v, 8, false, width, pad);
            } else {
                sink_str(sink, "0x");
                sink_uint(sink, v, 16, false, width ? width : 16, '0');
            }
            break;
        }
        case '%':
            sink_char(sink, '%');
            break;
        case '\0':
            return;
        default:
            sink_char(sink, '%');
            sink_char(sink, *fmt);
            break;
        }
        fmt++;
    }
}

void kvprintf(const char *fmt, va_list args)
{
    print_sink_t sink = { .buffer = NULLPTR, .size = 0, .written = 0, .to_console = true };
    format_into(&sink, fmt, args);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

int kvsnprintf(char *out, size_t size, const char *fmt, va_list args)
{
    print_sink_t sink = { .buffer = out, .size = size, .written = 0, .to_console = false };
    format_into(&sink, fmt, args);
    if (size > 0) {
        size_t term = sink.written < size ? sink.written : size - 1;
        out[term] = '\0';
    }
    return (int)sink.written;
}

int ksnprintf(char *out, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int written = kvsnprintf(out, size, fmt, args);
    va_end(args);
    return written;
}

NORETURN void kpanic(const char *fmt, ...)
{
    cli();
    if (g_panicking) {
        /* A fault inside the panic path: stop touching shared state entirely. */
        for (;;) {
            hlt();
        }
    }
    g_panicking = true;

    char message[512];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    serial_write("\n\n*** AURELIA KERNEL PANIC ***\n");
    serial_write(message);
    serial_write("\nSystem halted. Power cycle to reboot.\n");

    /* Deliberately no framebuffer or heap use from here on. */
    for (;;) {
        hlt();
    }
}

/* Exception entry points (assembly) */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

static const char *const g_exception_names[32] = {
    "Divide Error", "Debug", "Non-maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point", "Alignment Check", "Machine Check", "SIMD Floating-Point",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
};

NORETURN void kpanic_exception(void *frame_ptr)
{
    interrupt_frame_t *f = (interrupt_frame_t *)frame_ptr;
    cli();
    kpanic("CPU exception %u: %s\n  error=%#llx rip=%#llx cs=%#llx rflags=%#llx\n"
           "  rsp=%#llx ss=%#llx cr2=%#llx cr3=%#llx\n"
           "  rax=%#llx rbx=%#llx rcx=%#llx rdx=%#llx\n"
           "  rsi=%#llx rdi=%#llx rbp=%#llx",
           (u64)f->vector,
           (f->vector < 32) ? g_exception_names[f->vector] : "Unknown",
           (u64)f->error, (u64)f->rip, (u64)f->cs, (u64)f->rflags,
           (u64)f->rsp, (u64)f->ss, (u64)read_cr2(), (u64)read_cr3(),
           (u64)f->rax, (u64)f->rbx, (u64)f->rcx, (u64)f->rdx,
           (u64)f->rsi, (u64)f->rdi, (u64)f->rbp);
}
