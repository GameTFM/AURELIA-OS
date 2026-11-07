/* AURELIA OS - initial SYSCALL implementation */
#include <aurelia/syscall.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>
#include <aurelia/paging.h>
#include <aurelia/process.h>

#define MSR_EFER   0xC0000080u
#define MSR_STAR   0xC0000081u
#define MSR_LSTAR  0xC0000082u
#define MSR_FMASK  0xC0000084u

extern void syscall_entry(void);

static u64 g_write_count;

bool syscall_init(void)
{
    cpuid_regs_t ext = cpuid(0x80000000u, 0);
    if (ext.eax < 0x80000001u) {
        return false;
    }
    cpuid_regs_t features = cpuid(0x80000001u, 0);
    if (!(features.edx & (1u << 11))) {
        return false;
    }

    u64 efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | 1ull); /* EFER.SCE */

    /* SYSCALL loads CS=KERNEL_CS and SS=KERNEL_CS+8. The upper STAR field is
       populated for completeness; return uses IRETQ, not SYSRET, because the
       current GDT user selectors are ordered for IRETQ. */
    u64 star = ((u64)USER_DS << 48) | ((u64)KERNEL_CS << 32);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (u64)&syscall_entry);
    wrmsr(MSR_FMASK, (1ull << 9) | (1ull << 10)); /* clear IF and DF on entry */

    kprintf("[ok] syscall: LSTAR=%p STAR=%#llx FMASK=%#llx\n",
            (void *)&syscall_entry, star, (1ull << 9) | (1ull << 10));
    return true;
}

u64 syscall_write_count(void) { return g_write_count; }

u64 syscall_dispatch(u64 number, u64 arg1, u64 arg2, u64 arg3)
{
    (void)arg3;

    if (number == SYS_WRITE) {
        u64 root = paging_current_root();
        if (!arg2 || arg2 > 1024 || !paging_user_range_ok(root, arg1, arg2, false)) {
            return (u64)-1;
        }
        char buffer[1025];
        memcpy(buffer, (const void *)arg1, (size_t)arg2);
        buffer[arg2] = '\0';

        kputs("[ring3] ");
        for (u64 i = 0; i < arg2; i++) {
            kputc(buffer[i]);
        }
        if (arg2 == 0 || buffer[arg2 - 1] != '\n') {
            kputc('\n');
        }
        g_write_count++;
        return arg2;
    }

    if (number == SYS_GETPID) {
        return 1;
    }

    if (number == SYS_EXIT) {
        process_user_exit(arg1);
    }

    return (u64)-1;
}
