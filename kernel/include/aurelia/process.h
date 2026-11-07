/* AURELIA OS - minimal process/ring-3 foundation */
#pragma once

#include <aurelia/types.h>
#include <aurelia/idt.h>

/* Configure SYSCALL MSRs. Returns false if the CPU lacks SYSCALL. */
bool process_init(void);

/* Create a user address space, run a ring-3 test program and return true
   only if the syscall path works and a privileged instruction is trapped. */
bool process_run_user_selftest(void);

bool process_user_active(void);
u64  process_user_write_count(void);

/* Called from the exception dispatcher when a ring-3 fault is contained. */
NORETURN void process_user_exception(interrupt_frame_t *frame);

/* Called by SYS_EXIT. Switches back to the kernel context and never returns. */
NORETURN void process_user_exit(u64 code);
