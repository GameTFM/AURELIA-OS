/* AURELIA OS - kernel heap: first-fit free list with block splitting/coalescing */
#include <aurelia/heap.h>
#include <aurelia/pmm.h>
#include <aurelia/kstring.h>
#include <aurelia/log.h>
#include <aurelia/arch.h>

#define HEAP_ALIGNMENT 16
#define BLOCK_MAGIC 0xA0B10C4Bull

typedef struct block {
    u64 magic;
    u64 size;            /* payload bytes */
    bool free;
    u32 guard;
    struct block *next;
    struct block *prev;
} block_t;

static block_t *g_head;
static u8 *g_base;
static u64 g_total;
static u64 g_used;

static inline u64 block_overhead(void) { return align_up(sizeof(block_t), HEAP_ALIGNMENT); }

void heap_init(u64 bytes)
{
    bytes = align_up(bytes, 4096);
    u64 pages = bytes / 4096;
    u64 phys = pmm_alloc_pages(pages);
    if (!phys) {
        kpanic("heap: unable to reserve %llu KiB of physical memory", bytes / 1024);
    }
    g_base = (u8 *)pmm_phys_to_virt(phys);
    g_total = bytes;
    g_used = 0;

    g_head = (block_t *)g_base;
    g_head->magic = BLOCK_MAGIC;
    g_head->size = bytes - block_overhead();
    g_head->free = true;
    g_head->guard = 0;
    g_head->next = NULLPTR;
    g_head->prev = NULLPTR;

    kprintf("[heap] %llu KiB at phys %#llx (virtual %p)\n", bytes / 1024, phys, (void *)g_base);
}

static void split(block_t *block, u64 wanted)
{
    u64 overhead = block_overhead();
    if (block->size < wanted + overhead + HEAP_ALIGNMENT) {
        return;
    }
    block_t *rest = (block_t *)((u8 *)block + overhead + wanted);
    rest->magic = BLOCK_MAGIC;
    rest->size = block->size - wanted - overhead;
    rest->free = true;
    rest->guard = 0;
    rest->next = block->next;
    rest->prev = block;
    if (block->next) {
        block->next->prev = rest;
    }
    block->next = rest;
    block->size = wanted;
}

static void coalesce(block_t *block)
{
    if (block->next && block->next->free) {
        block_t *next = block->next;
        block->size += block_overhead() + next->size;
        block->next = next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    if (block->prev && block->prev->free) {
        block_t *prev = block->prev;
        prev->size += block_overhead() + block->size;
        prev->next = block->next;
        if (prev->next) {
            prev->next->prev = prev;
        }
    }
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULLPTR;
    }
    u64 wanted = align_up(size, HEAP_ALIGNMENT);
    u64 flags = irq_save();
    for (block_t *block = g_head; block; block = block->next) {
        if (block->magic != BLOCK_MAGIC) {
            irq_restore(flags);
            kpanic("heap: corrupted block header at %p", (void *)block);
        }
        if (!block->free || block->size < wanted) {
            continue;
        }
        split(block, wanted);
        block->free = false;
        g_used += block->size;
        irq_restore(flags);
        return (u8 *)block + block_overhead();
    }
    irq_restore(flags);
    return NULLPTR;
}

void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    block_t *block = (block_t *)((u8 *)ptr - block_overhead());
    if (block->magic != BLOCK_MAGIC || block->free) {
        kpanic("heap: invalid free of %p", ptr);
    }
    u64 flags = irq_save();
    block->free = true;
    g_used -= block->size;
    coalesce(block);
    irq_restore(flags);
}

void *krealloc(void *ptr, size_t new_size)
{
    if (!ptr) {
        return kmalloc(new_size);
    }
    block_t *block = (block_t *)((u8 *)ptr - block_overhead());
    if (block->magic != BLOCK_MAGIC) {
        kpanic("heap: krealloc on invalid pointer %p", ptr);
    }
    if (block->size >= new_size) {
        return ptr;
    }
    void *fresh = kmalloc(new_size);
    if (!fresh) {
        return NULLPTR;
    }
    memcpy(fresh, ptr, block->size);
    kfree(ptr);
    return fresh;
}

u64 heap_total(void) { return g_total; }
u64 heap_used(void) { return g_used; }

u64 heap_free(void)
{
    u64 total_free = 0;
    for (block_t *block = g_head; block; block = block->next) {
        if (block->free) {
            total_free += block->size;
        }
    }
    return total_free;
}

void heap_dump(void)
{
    int index = 0;
    for (block_t *block = g_head; block && index < 24; block = block->next, index++) {
        kprintf("  heap[%d] %-10p size=%llu %s\n", index, (void *)block,
                block->size, block->free ? "free" : "used");
    }
}
