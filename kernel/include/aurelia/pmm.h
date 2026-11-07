/* AURELIA OS - physical page allocator (bitmap, 4 KiB pages) */
#pragma once

#include <aurelia/types.h>
#include <aurelia/limine.h>

void  pmm_init(struct limine_memmap_response *memmap, u64 hhdm_offset,
               u64 kernel_phys_base, u64 kernel_size);
u64   pmm_alloc_page(void);
u64   pmm_alloc_pages(u64 count);
void  pmm_free_page(u64 phys);
void  pmm_free_pages(u64 phys, u64 count);

u64   pmm_total_bytes(void);
u64   pmm_free_bytes(void);
u64   pmm_used_bytes(void);
u64   pmm_hhdm_offset(void);

void *pmm_phys_to_virt(u64 phys);
u64   pmm_virt_to_phys(const void *virt);
