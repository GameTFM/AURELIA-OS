/* AURELIA OS - 4-level x86_64 page tables, kernel/user address spaces */
#pragma once

#include <aurelia/types.h>

#define AURELIA_PAGE_SIZE 4096ull

#define PG_PRESENT  (1ull << 0)
#define PG_WRITE    (1ull << 1)
#define PG_USER     (1ull << 2)
#define PG_PWT      (1ull << 3)
#define PG_PCD      (1ull << 4)
#define PG_ACCESSED (1ull << 5)
#define PG_DIRTY    (1ull << 6)
#define PG_HUGE     (1ull << 7)
#define PG_GLOBAL   (1ull << 8)
#define PG_NX       (1ull << 63)

#define PG_ADDR_MASK 0x000FFFFFFFFFF000ull

void paging_init(void);
void paging_activate(u64 root_phys);
void paging_activate_kernel(void);

u64  paging_kernel_root(void);
u64  paging_current_root(void);
u64  paging_create_user_space(void);

bool paging_map_user(u64 root_phys, u64 virt, u64 phys, u64 flags);
bool paging_query(u64 root_phys, u64 virt, u64 *phys_out, u64 *flags_out);
bool paging_user_range_ok(u64 root_phys, u64 virt, u64 length, bool write);
bool paging_selftest(void);
