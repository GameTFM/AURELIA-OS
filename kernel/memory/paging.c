/* AURELIA OS - kernel-owned 4-level page tables and user mappings */
#include <aurelia/paging.h>
#include <aurelia/pmm.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>

#define PML4_ENTRIES 512u
#define PML4_FIRST_KERNEL_ENTRY 256u
#define USER_SPACE_LIMIT 0x0000800000000000ull

static u64 g_kernel_root;
static u64 g_table_pages;

static inline u64 *table_at(u64 phys)
{
    return (u64 *)pmm_phys_to_virt(phys & PG_ADDR_MASK);
}

static inline u64 next_level(u64 entry)
{
    return entry & PG_ADDR_MASK;
}

static u64 clone_table(u64 table_phys, u32 level)
{
    u64 new_phys = pmm_alloc_page();
    if (!new_phys) {
        kpanic("paging: out of memory cloning kernel page tables");
    }
    g_table_pages++;

    u64 *src = table_at(table_phys);
    u64 *dst = table_at(new_phys);

    for (u32 i = 0; i < PML4_ENTRIES; i++) {
        u64 entry = src[i];
        if (!(entry & PG_PRESENT)) {
            continue;
        }
        u64 flags = entry & 0xFFFull;
        if (level > 1 && (entry & PG_HUGE)) {
            /* A 1 GiB or 2 MiB leaf: preserve it, but make it kernel-only. */
            dst[i] = (entry & ~PG_USER) | PG_PRESENT | (entry & PG_NX);
        } else if (level == 1) {
            dst[i] = (entry & ~PG_USER) | PG_PRESENT;
        } else {
            u64 child = clone_table(next_level(entry), level - 1);
            dst[i] = (child | flags | PG_PRESENT) & ~PG_USER;
        }
    }
    return new_phys;
}

void paging_init(void)
{
    u64 limine_root = read_cr3() & PG_ADDR_MASK;
    if (!limine_root) {
        kpanic("paging: CR3 is zero at kernel paging init");
    }

    u64 *old_root = table_at(limine_root);
    u64 root_phys = pmm_alloc_page();
    if (!root_phys) {
        kpanic("paging: could not allocate kernel PML4");
    }
    g_table_pages = 1;

    u64 *root = table_at(root_phys);
    u32 cloned = 0;
    for (u32 i = PML4_FIRST_KERNEL_ENTRY; i < PML4_ENTRIES; i++) {
        u64 entry = old_root[i];
        if (!(entry & PG_PRESENT)) {
            continue;
        }
        u64 child = clone_table(next_level(entry), 3);
        root[i] = (child | (entry & 0xFFFull) | PG_PRESENT) & ~PG_USER;
        cloned++;
    }

    if (!cloned) {
        kpanic("paging: Limine supplied no higher-half mappings");
    }

    g_kernel_root = root_phys;

    /* Verify the new root can resolve the kernel before switching CR3. */
    u64 phys = 0;
    u64 flags = 0;
    if (!paging_query(root_phys, (u64)&paging_init, &phys, &flags) ||
        !(flags & PG_PRESENT) || (flags & PG_USER)) {
        kpanic("paging: cloned root did not resolve kernel text as supervisor");
    }

    write_cr3(root_phys);
    kprintf("[ok] paging: kernel-owned 4-level tables at phys %#llx (%u high entries, %llu tables)\n",
            root_phys, cloned, g_table_pages);
    kprintf("[ok] paging: kernel text supervisor-only; user address space available\n");
}

void paging_activate(u64 root_phys)
{
    if (root_phys) {
        write_cr3(root_phys & PG_ADDR_MASK);
    }
}

void paging_activate_kernel(void)
{
    paging_activate(g_kernel_root);
}

u64 paging_kernel_root(void) { return g_kernel_root; }
u64 paging_current_root(void) { return read_cr3() & PG_ADDR_MASK; }

u64 paging_create_user_space(void)
{
    u64 root_phys = pmm_alloc_page();
    if (!root_phys) {
        return 0;
    }
    u64 *root = table_at(root_phys);
    u64 *kernel = table_at(g_kernel_root);
    if (!g_kernel_root) {
        pmm_free_page(root_phys);
        return 0;
    }
    for (u32 i = PML4_FIRST_KERNEL_ENTRY; i < PML4_ENTRIES; i++) {
        root[i] = kernel[i] & ~PG_USER;
    }
    g_table_pages++;
    return root_phys;
}

static u64 ensure_user_table(u64 *entry, u64 flags)
{
    if (!(*entry & PG_PRESENT)) {
        u64 phys = pmm_alloc_page();
        if (!phys) {
            return 0;
        }
        *entry = phys | PG_PRESENT | PG_WRITE | PG_USER | (flags & 0);
        g_table_pages++;
        return phys;
    }
    return next_level(*entry);
}

bool paging_map_user(u64 root_phys, u64 virt, u64 phys, u64 flags)
{
    if (!root_phys || virt >= USER_SPACE_LIMIT || (virt & 0xFFF) || (phys & 0xFFF)) {
        return false;
    }
    u64 *root = table_at(root_phys);

    u64 pml4_index = (virt >> 39) & 511;
    u64 pdpt_index = (virt >> 30) & 511;
    u64 pd_index = (virt >> 21) & 511;
    u64 pt_index = (virt >> 12) & 511;

    u64 *pml4 = &root[pml4_index];
    u64 pdpt_phys = ensure_user_table(pml4, 0);
    if (!pdpt_phys) return false;

    u64 *pdpt = table_at(pdpt_phys);
    u64 *pdpt_entry = &pdpt[pdpt_index];
    u64 pd_phys = ensure_user_table(pdpt_entry, 0);
    if (!pd_phys) return false;

    u64 *pd = table_at(pd_phys);
    u64 *pd_entry = &pd[pd_index];
    u64 pt_phys = ensure_user_table(pd_entry, 0);
    if (!pt_phys) return false;

    u64 *pt = table_at(pt_phys);
    if (pt[pt_index] & PG_PRESENT) {
        return false;
    }
    pt[pt_index] = (phys & PG_ADDR_MASK) | flags | PG_PRESENT | PG_USER;
    return true;
}

bool paging_query(u64 root_phys, u64 virt, u64 *phys_out, u64 *flags_out)
{
    if (!root_phys) {
        return false;
    }
    u64 *table = table_at(root_phys);
    for (u32 level = 4; level >= 1; level--) {
        u32 shift = 12 + 9 * (level - 1);
        u64 index = (virt >> shift) & 511;
        u64 entry = table[index];
        if (!(entry & PG_PRESENT)) {
            return false;
        }
        if (level == 1 || (entry & PG_HUGE)) {
            u64 page_size = 1ull << shift;
            if (phys_out) {
                *phys_out = (entry & PG_ADDR_MASK) + (virt & (page_size - 1));
            }
            if (flags_out) {
                *flags_out = entry;
            }
            return true;
        }
        table = table_at(next_level(entry));
    }
    return false;
}

bool paging_user_range_ok(u64 root_phys, u64 virt, u64 length, bool write)
{
    if (!length) {
        return false;
    }
    if (virt >= USER_SPACE_LIMIT || virt + length < virt || virt + length > USER_SPACE_LIMIT) {
        return false;
    }
    u64 cursor = virt;
    u64 end = virt + length;
    while (cursor < end) {
        u64 phys = 0;
        u64 flags = 0;
        if (!paging_query(root_phys, cursor, &phys, &flags)) {
            return false;
        }
        if (!(flags & PG_USER) || (!write && !(flags & PG_PRESENT))) {
            return false;
        }
        if (write && !(flags & PG_WRITE)) {
            return false;
        }
        u64 next = align_down(cursor, AURELIA_PAGE_SIZE) + AURELIA_PAGE_SIZE;
        cursor = next > cursor ? next : end;
    }
    return true;
}

bool paging_selftest(void)
{
    if (!g_kernel_root || paging_current_root() != g_kernel_root) {
        kprintf("[warn] paging self-test: active CR3 is not the kernel root\n");
        return false;
    }
    u64 phys = 0;
    u64 flags = 0;
    if (!paging_query(g_kernel_root, (u64)&paging_selftest, &phys, &flags) ||
        (flags & PG_USER)) {
        kprintf("[warn] paging self-test: kernel mapping is missing or user-accessible\n");
        return false;
    }
    kprintf("[ok] paging self-test: kernel text resolves supervisor-only (phys %#llx)\n", phys);
    return true;
}
