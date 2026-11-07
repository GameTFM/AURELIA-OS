/* AURELIA OS - Interrupt Descriptor Table */
#pragma once

#include <aurelia/types.h>

/* Layout must match the push order in kernel/arch/x86_64/isr.S */
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 vector, error;
    u64 rip, cs, rflags, rsp, ss;
} interrupt_frame_t;

void idt_init(void);
void idt_set_gate(u8 vector, void (*handler)(void), u8 type_attr);
void idt_set_ist(u8 vector, u8 ist);

/* Called from assembly; never returns for CPU exceptions. */
void interrupt_handler(interrupt_frame_t *frame);
