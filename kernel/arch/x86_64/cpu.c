/* AURELIA OS - CPU identification, feature detection and floating point setup */
#include <aurelia/cpu.h>
#include <aurelia/arch.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>

static cpu_info_t g_cpu;

void cpu_init(void)
{
    memset(&g_cpu, 0, sizeof(g_cpu));

    cpuid_regs_t r = cpuid(0, 0);
    g_cpu.max_basic_leaf = r.eax;
    char vendor[13];
    memcpy(vendor + 0, &r.ebx, 4);
    memcpy(vendor + 4, &r.edx, 4);
    memcpy(vendor + 8, &r.ecx, 4);
    vendor[12] = '\0';
    strncpy(g_cpu.vendor, vendor, sizeof(g_cpu.vendor) - 1);

    if (g_cpu.max_basic_leaf >= 1) {
        cpuid_regs_t f = cpuid(1, 0);
        g_cpu.stepping = f.eax & 0x0F;
        g_cpu.model = (f.eax >> 4) & 0x0F;
        g_cpu.family = (f.eax >> 8) & 0x0F;
        if (g_cpu.family == 6 || g_cpu.family == 15) {
            g_cpu.model += ((f.eax >> 16) & 0x0F) << 4;
        }
        g_cpu.has_apic = (f.edx & (1u << 9)) != 0;
        g_cpu.has_tsc = (f.edx & (1u << 4)) != 0;
        g_cpu.has_sse2 = (f.edx & (1u << 26)) != 0;
        g_cpu.has_x2apic = (f.ecx & (1u << 21)) != 0;
        g_cpu.has_avx = (f.ecx & (1u << 28)) != 0;
        if ((f.ebx >> 16) & 0xFF) {
            g_cpu.cores = (f.ebx >> 16) & 0xFF;
        } else {
            g_cpu.cores = 1;
        }
    }

    r = cpuid(0x80000000u, 0);
    g_cpu.max_extended_leaf = r.eax;
    if (r.eax >= 0x80000004u) {
        char brand[49];
        for (u32 i = 0; i < 3; i++) {
            cpuid_regs_t b = cpuid(0x80000002u + i, 0);
            memcpy(brand + i * 16 + 0, &b.eax, 4);
            memcpy(brand + i * 16 + 4, &b.ebx, 4);
            memcpy(brand + i * 16 + 8, &b.ecx, 4);
            memcpy(brand + i * 16 + 12, &b.edx, 4);
        }
        brand[48] = '\0';
        /* Trim leading spaces produced by Intel */
        const char *src = brand;
        while (*src == ' ') {
            src++;
        }
        strncpy(g_cpu.brand, src, sizeof(g_cpu.brand) - 1);
        /* Trim trailing spaces */
        size_t len = strlen(g_cpu.brand);
        while (len > 0 && g_cpu.brand[len - 1] == ' ') {
            g_cpu.brand[--len] = '\0';
        }
    } else {
        strcpy(g_cpu.brand, "Unknown x86_64 processor");
    }

    /* Enable SSE so the compiler may use vector instructions in the future. */
    if (g_cpu.has_sse2) {
        u64 cr0 = read_cr0();
        cr0 &= ~(1ull << 2);  /* EM */
        cr0 &= ~(1ull << 3);  /* TS */
        cr0 |= (1ull << 1);   /* MP */
        write_cr0(cr0);
        u64 cr4 = read_cr4();
        cr4 |= (1ull << 9);   /* OSFXSR */
        cr4 |= (1ull << 10);  /* OSXMMEXCPT */
        write_cr4(cr4);
    }

    /* Estimate TSC frequency if the bootloader did not supply one. */
    if (g_cpu.tsc_hz == 0) {
        g_cpu.tsc_hz = 0;
    }
}

const cpu_info_t *cpu_info(void) { return &g_cpu; }
const char *cpu_vendor(void) { return g_cpu.vendor; }
const char *cpu_brand(void) { return g_cpu.brand; }
u64 cpu_tsc_hz(void) { return g_cpu.tsc_hz; }
u64 cpu_tsc(void) { return rdtsc(); }
void cpu_set_tsc_hz(u64 hz) { g_cpu.tsc_hz = hz; }
