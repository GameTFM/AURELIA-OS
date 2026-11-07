/* AURELIA OS - system call ABI */
#pragma once

#include <aurelia/types.h>

#define SYS_WRITE 1
#define SYS_GETPID 2
#define SYS_EXIT 60

/* Configure LSTAR/STAR/FMASK and EFER.SCE. */
bool syscall_init(void);
u64 syscall_dispatch(u64 number, u64 arg1, u64 arg2, u64 arg3);
u64 syscall_write_count(void);
