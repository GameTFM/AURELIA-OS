/* AURELIA OS - Global Descriptor Table */
#pragma once

#include <aurelia/types.h>

void gdt_init(void);
void gdt_set_kernel_stack(u64 stack_top);
u64  gdt_kernel_stack(void);
