/* AURELIA OS - kernel heap allocator */
#pragma once

#include <aurelia/types.h>

void  heap_init(u64 bytes);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);

u64   heap_total(void);
u64   heap_used(void);
u64   heap_free(void);
void  heap_dump(void);
