/* AURELIA OS - Interrupt Descriptor Table setup and dispatch */
#include <aurelia/idt.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/arch.h>
#include <aurelia/irq.h>
#include <aurelia/process.h>

#define IDT_ENTRIES 256

typedef struct {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_middle;
    u32 offset_high;
    u32 zero;
} PACKED idt_entry_t;

typedef struct {
    u16 limit;
    u64 base;
} PACKED idt_ptr_t;

static idt_entry_t g_idt[IDT_ENTRIES];
static idt_ptr_t g_idt_ptr;

#define ISR_DECL(n) extern void isr##n(void)
ISR_DECL(0);  ISR_DECL(1);  ISR_DECL(2);  ISR_DECL(3);
ISR_DECL(4);  ISR_DECL(5);  ISR_DECL(6);  ISR_DECL(7);
ISR_DECL(8);  ISR_DECL(9);  ISR_DECL(10); ISR_DECL(11);
ISR_DECL(12); ISR_DECL(13); ISR_DECL(14); ISR_DECL(15);
ISR_DECL(16); ISR_DECL(17); ISR_DECL(18); ISR_DECL(19);
ISR_DECL(20); ISR_DECL(21); ISR_DECL(22); ISR_DECL(23);
ISR_DECL(24); ISR_DECL(25); ISR_DECL(26); ISR_DECL(27);
ISR_DECL(28); ISR_DECL(29); ISR_DECL(30); ISR_DECL(31);
ISR_DECL(32); ISR_DECL(33); ISR_DECL(34); ISR_DECL(35);
ISR_DECL(36); ISR_DECL(37); ISR_DECL(38); ISR_DECL(39);
ISR_DECL(40); ISR_DECL(41); ISR_DECL(42); ISR_DECL(43);
ISR_DECL(44); ISR_DECL(45); ISR_DECL(46); ISR_DECL(47);
#undef ISR_DECL

static void (*const g_isr_table[48])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
};

void idt_set_gate(u8 vector, void (*handler)(void), u8 type_attr)
{
    u64 address = (u64)handler;
    g_idt[vector].offset_low = (u16)(address & 0xFFFF);
    g_idt[vector].selector = 0x08;
    g_idt[vector].ist = 0;
    g_idt[vector].type_attr = type_attr;
    g_idt[vector].offset_middle = (u16)((address >> 16) & 0xFFFF);
    g_idt[vector].offset_high = (u32)((address >> 32) & 0xFFFFFFFF);
    g_idt[vector].zero = 0;
}

void idt_set_ist(u8 vector, u8 ist)
{
    g_idt[vector].ist = (u8)(ist & 0x07);
}

void idt_init(void)
{
    memset(&g_idt, 0, sizeof(g_idt));
    for (u32 i = 0; i < 48; i++) {
        idt_set_gate((u8)i, g_isr_table[i], 0x8E); /* present, ring 0, interrupt gate */
    }
    g_idt_ptr.limit = (u16)(sizeof(g_idt) - 1);
    g_idt_ptr.base = (u64)&g_idt;
    __asm__ volatile("lidt %0" : : "m"(g_idt_ptr));
}

void interrupt_handler(interrupt_frame_t *frame)
{
    if (frame->vector < 32) {
        if ((frame->cs & 3) == 3 && process_user_active()) {
            process_user_exception(frame);
        }
        kpanic_exception(frame);
    }
    if (frame->vector < 48) {
        irq_dispatch((u8)(frame->vector - 32));
    }
}
