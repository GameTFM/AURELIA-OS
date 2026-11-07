/* AURELIA OS - physical memory manager */
#include <aurelia/pmm.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/arch.h>

#define PMM_PAGE_SIZE   4096ull
#define PMM_MAX_PAGES   (64ull * 1024 * 1024 * 1024 / PMM_PAGE_SIZE) /* 64 GiB */
#define PMM_BITMAP_WORDS (PMM_MAX_PAGES / 64ull)

static u64 g_bitmap[PMM_BITMAP_WORDS];
static u64 g_hhdm;
static u64 g_total_pages;     /* usable pages discovered from the memory map */
static u64 g_free_pages;      /* usable pages not yet handed out */
static u64 g_search_hint;
static u64 g_max_page;

static inline bool page_is_used(u64 page)
{
    return (g_bitmap[page / 64] & (1ull << (page % 64))) != 0;
}

static inline void page_mark_used(u64 page)
{
    g_bitmap[page / 64] |= (1ull << (page % 64));
}

static inline void page_mark_free(u64 page)
{
    g_bitmap[page / 64] &= ~(1ull << (page % 64));
}

static void reserve_range(u64 base, u64 length)
{
    u64 first = align_down(base, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
    u64 last = align_up(base + length, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
    if (last > PMM_MAX_PAGES) {
        last = PMM_MAX_PAGES;
    }
    for (u64 page = first; page < last; page++) {
        if (!page_is_used(page)) {
            page_mark_used(page);
            if (g_free_pages) {
                g_free_pages--;
            }
        }
    }
}

static void free_range(u64 base, u64 length)
{
    u64 first = align_up(base, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
    u64 last = align_down(base + length, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
    if (last > PMM_MAX_PAGES) {
        last = PMM_MAX_PAGES;
    }
    for (u64 page = first; page < last; page++) {
        if (page_is_used(page)) {
            page_mark_free(page);
            g_free_pages++;
        }
    }
    if (last > g_max_page) {
        g_max_page = last;
    }
}

void pmm_init(struct limine_memmap_response *memmap, u64 hhdm_offset,
              u64 kernel_phys_base, u64 kernel_size)
{
    g_hhdm = hhdm_offset;
    g_search_hint = 0;
    g_total_pages = 0;
    g_free_pages = 0;
    g_max_page = 0;

    /* Everything reserved; only usable regions are released. */
    memset(g_bitmap, 0xFF, sizeof(g_bitmap));

    if (memmap) {
        for (u64 i = 0; i < memmap->entry_count; i++) {
            struct limine_memmap_entry *entry = memmap->entries[i];
            if (entry->type != LIMINE_MEMMAP_USABLE) {
                continue;
            }
            free_range(entry->base, entry->length);
            g_total_pages += align_down(entry->length, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
        }
    }

    reserve_range(0, 0x100000);                 /* BIOS / real-mode area */
    reserve_range(kernel_phys_base, kernel_size); /* kernel image */
    reserve_range((u64)&g_bitmap, sizeof(g_bitmap));

    kprintf("[pmm] %llu MiB usable, %llu MiB free after reservations\n",
            g_total_pages * PMM_PAGE_SIZE / (1024 * 1024),
            g_free_pages * PMM_PAGE_SIZE / (1024 * 1024));
}

u64 pmm_total_bytes(void) { return g_total_pages * PMM_PAGE_SIZE; }
u64 pmm_used_bytes(void) { return (g_total_pages - g_free_pages) * PMM_PAGE_SIZE; }
u64 pmm_free_bytes(void) { return g_free_pages * PMM_PAGE_SIZE; }
u64 pmm_hhdm_offset(void) { return g_hhdm; }

void *pmm_phys_to_virt(u64 phys) { return (void *)(phys + g_hhdm); }
u64 pmm_virt_to_phys(const void *virt) { return (u64)virt - g_hhdm; }

static u64 find_free_run(u64 count)
{
    if (count == 0) {
        return 0;
    }
    u64 run = 0;
    for (u64 page = g_search_hint; page < g_max_page; page++) {
        if (page_is_used(page)) {
            run = 0;
            continue;
        }
        run++;
        if (run == count) {
            u64 start = page - count + 1;
            for (u64 i = 0; i < count; i++) {
                page_mark_used(start + i);
            }
            g_free_pages -= count;
            g_search_hint = page + 1;
            return start;
        }
    }
    return 0;
}

u64 pmm_alloc_pages(u64 count)
{
    u64 flags = irq_save();
    u64 page = find_free_run(count);
    irq_restore(flags);
    if (page == 0) {
        return 0;
    }
    u64 phys = page * PMM_PAGE_SIZE;
    memset((void *)(phys + g_hhdm), 0, count * PMM_PAGE_SIZE);
    return phys;
}

u64 pmm_alloc_page(void) { return pmm_alloc_pages(1); }

void pmm_free_pages(u64 phys, u64 count)
{
    if (!phys || count == 0) {
        return;
    }
    u64 flags = irq_save();
    u64 first = phys / PMM_PAGE_SIZE;
    for (u64 i = 0; i < count; i++) {
        if (page_is_used(first + i)) {
            page_mark_free(first + i);
            g_free_pages++;
        }
    }
    if (first < g_search_hint) {
        g_search_hint = first;
    }
    irq_restore(flags);
}

void pmm_free_page(u64 phys) { pmm_free_pages(phys, 1); }
