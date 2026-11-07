/* AURELIA OS - 8259 PIC initialisation and IRQ dispatch */
#include <aurelia/irq.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

typedef struct {
    irq_handler_t handler;
    void *context;
    u64 count;
} irq_slot_t;

static irq_slot_t g_irq_table[16];
static u16 g_irq_mask = 0xFFFF;

static void pic_remap(void)
{
    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    outb(PIC1_DATA, 0x20); /* master vector offset 32 */
    io_wait();
    outb(PIC2_DATA, 0x28); /* slave vector offset 40 */
    io_wait();
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void irq_init(void)
{
    memset(g_irq_table, 0, sizeof(g_irq_table));
    pic_remap();
    /* Mask everything until a driver registers for a line. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    g_irq_mask = 0xFFFF;
    kprintf("[ok] 8259 PIC remapped (IRQs 0-15 -> vectors 32-47)\n");
}

void irq_enable(u8 irq)
{
    if (irq > 15) {
        return;
    }
    g_irq_mask &= (u16)~(1u << irq);
    if (irq >= 8) {
        /* Slave PIC interrupts arrive through the master's cascade line (IRQ2). */
        g_irq_mask &= (u16)~(1u << 2);
    }
    if (irq < 8) {
        outb(PIC1_DATA, (u8)(g_irq_mask & 0xFF));
    } else {
        outb(PIC2_DATA, (u8)(g_irq_mask >> 8));
        outb(PIC1_DATA, (u8)(g_irq_mask & 0xFF));
    }
}

void irq_disable(u8 irq)
{
    if (irq > 15) {
        return;
    }
    g_irq_mask |= (u16)(1u << irq);
    outb(PIC1_DATA, (u8)(g_irq_mask & 0xFF));
    if (irq >= 8) {
        outb(PIC2_DATA, (u8)(g_irq_mask >> 8));
    }
}

bool irq_register(u8 irq, irq_handler_t handler, void *context)
{
    if (irq > 15 || !handler) {
        return false;
    }
    g_irq_table[irq].handler = handler;
    g_irq_table[irq].context = context;
    irq_enable(irq);
    return true;
}

void irq_dispatch(u8 irq)
{
    if (irq >= 16) {
        return;
    }
    g_irq_table[irq].count++;
    if (g_irq_table[irq].handler) {
        g_irq_table[irq].handler(irq, g_irq_table[irq].context);
    }
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}

u64 irq_count(u8 irq)
{
    return irq < 16 ? g_irq_table[irq].count : 0;
}
