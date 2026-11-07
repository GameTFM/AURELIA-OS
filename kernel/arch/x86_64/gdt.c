/* AURELIA OS - Global Descriptor Table + Task State Segment */
#include <aurelia/gdt.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>

typedef struct {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
} PACKED gdt_entry_t;

typedef struct {
    u16 limit;
    u64 base;
} PACKED gdt_ptr_t;

typedef struct {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist[7];
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} PACKED tss_t;

static gdt_entry_t g_gdt[7]; /* 5 base entries + 2 slots for the 64-bit TSS descriptor */
static gdt_ptr_t g_gdt_ptr;
static tss_t g_tss;
static u64 g_kernel_stack_top;

extern void gdt_flush(void *descriptor);
extern void tss_flush(void);

static void gdt_set_entry(int index, u32 base, u32 limit, u8 access, u8 granularity)
{
    g_gdt[index].base_low = (u16)(base & 0xFFFF);
    g_gdt[index].base_middle = (u8)((base >> 16) & 0xFF);
    g_gdt[index].base_high = (u8)((base >> 24) & 0xFF);
    g_gdt[index].limit_low = (u16)(limit & 0xFFFF);
    g_gdt[index].granularity = (u8)(((limit >> 16) & 0x0F) | (granularity & 0xF0));
    g_gdt[index].access = access;
}

static void gdt_set_tss(int index, u64 base, u32 limit)
{
    gdt_entry_t *e = &g_gdt[index];
    e->limit_low = (u16)(limit & 0xFFFF);
    e->base_low = (u16)(base & 0xFFFF);
    e->base_middle = (u8)((base >> 16) & 0xFF);
    e->access = 0x89; /* present, 64-bit available TSS */
    e->granularity = (u8)((limit >> 16) & 0x0F);
    e->base_high = (u8)((base >> 24) & 0xFF);

    /* Upper 32 bits of the 64-bit TSS base live in the next descriptor slot. */
    gdt_entry_t *hi = &g_gdt[index + 1];
    hi->limit_low = (u16)((base >> 32) & 0xFFFF);
    hi->base_low = (u16)((base >> 48) & 0xFFFF);
    hi->base_middle = 0;
    hi->access = 0;
    hi->granularity = 0;
    hi->base_high = 0;
}

void gdt_init(void)
{
    memset(&g_gdt, 0, sizeof(g_gdt));
    memset(&g_tss, 0, sizeof(g_tss));

    gdt_set_entry(0, 0, 0, 0, 0);
    /* 0x08 kernel code: base 0, limit 4GiB pages, long mode, ring 0 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
    /* 0x10 kernel data */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);
    /* 0x18 user data (ring 3) */
    gdt_set_entry(3, 0, 0xFFFFF, 0xF2, 0xA0);
    /* 0x20 user code (ring 3) */
    gdt_set_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);

    g_tss.iomap_base = sizeof(tss_t);
    gdt_set_tss(5, (u64)&g_tss, sizeof(tss_t) - 1);

    g_gdt_ptr.limit = (u16)(sizeof(g_gdt) - 1);
    g_gdt_ptr.base = (u64)&g_gdt;

    gdt_flush(&g_gdt_ptr);
    tss_flush();
}

void gdt_set_kernel_stack(u64 stack_top)
{
    g_kernel_stack_top = stack_top;
    g_tss.rsp0 = stack_top;
    /* The CPU reads RSP0 from the TSS in memory on every CPL transition.
       Reloading TR here would fault: an already-busy TSS cannot be LTR'd. */
}

u64 gdt_kernel_stack(void) { return g_kernel_stack_top; }
