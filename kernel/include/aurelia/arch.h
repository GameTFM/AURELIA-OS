/* AURELIA OS - low level x86_64 primitives */
#pragma once

#include <aurelia/types.h>

static inline void outb(u16 port, u8 value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(u16 port, u16 value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(u16 port, u32 value)
{
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port)
{
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline u16 inw(u16 port)
{
    u16 value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline u32 inl(u16 port)
{
    u32 value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) { outb(0x80, 0); }

static inline void cli(void) { __asm__ volatile("cli" : : : "memory"); }
static inline void sti(void) { __asm__ volatile("sti" : : : "memory"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/* Save RFLAGS and disable interrupts; restore with irq_restore(). */
static inline u64 irq_save(void)
{
    u64 flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(u64 flags)
{
    if (flags & (1ull << 9)) {
        __asm__ volatile("sti" : : : "memory");
    } else {
        __asm__ volatile("cli" : : : "memory");
    }
}

static inline void pause_cpu(void) { __asm__ volatile("pause"); }

static inline u64 read_cr0(void) { u64 v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline void write_cr0(u64 v) { __asm__ volatile("mov %0, %%cr0" : : "r"(v) : "memory"); }
static inline u64 read_cr2(void) { u64 v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v; }
static inline u64 read_cr3(void) { u64 v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v; }
static inline void write_cr3(u64 v) { __asm__ volatile("mov %0, %%cr3" : : "r"(v) : "memory"); }
static inline u64 read_cr4(void) { u64 v; __asm__ volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static inline void write_cr4(u64 v) { __asm__ volatile("mov %0, %%cr4" : : "r"(v) : "memory"); }

static inline void invlpg(u64 addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void wrmsr(u32 msr, u64 value)
{
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((u32)value), "d"((u32)(value >> 32)));
}

static inline u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static inline u64 rdtsc(void)
{
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

static inline void rdtsc_ordered(u32 *lo, u32 *hi)
{
    __asm__ volatile("rdtscp" : "=a"(*lo), "=d"(*hi) : : "rcx");
}

typedef struct {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
} cpuid_regs_t;

static inline cpuid_regs_t cpuid(u32 leaf, u32 subleaf)
{
    cpuid_regs_t r;
    __asm__ volatile("cpuid"
                     : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
                     : "a"(leaf), "c"(subleaf));
    return r;
}

/* A small wrapper used by CPU feature probing. */
static inline u64 cpu_tsc_frequency_hz(void);
