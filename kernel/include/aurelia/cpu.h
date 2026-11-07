/* AURELIA OS - CPU identification and feature detection */
#pragma once

#include <aurelia/types.h>

typedef struct {
    char vendor[13];
    char brand[49];
    u32 max_basic_leaf;
    u32 max_extended_leaf;
    u32 family;
    u32 model;
    u32 stepping;
    u32 cores;
    bool has_apic;
    bool has_x2apic;
    bool has_tsc;
    bool has_sse2;
    bool has_avx;
    u64 tsc_hz;
} cpu_info_t;

void cpu_init(void);
const cpu_info_t *cpu_info(void);
const char *cpu_vendor(void);
const char *cpu_brand(void);
u64 cpu_tsc_hz(void);
u64 cpu_tsc(void);
void cpu_set_tsc_hz(u64 hz);
