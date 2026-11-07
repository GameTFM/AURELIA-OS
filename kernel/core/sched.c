/* AURELIA OS - cooperative kernel thread context switch */
#include <aurelia/sched.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>

#define SCHED_THREADS 2
#define SCHED_STACK_SIZE 8192
#define SCHED_ITERATIONS 4

typedef struct {
    u64 rsp;
    volatile u32 counter;
    volatile bool done;
} sched_thread_t;

static u8 g_thread_stacks[SCHED_THREADS][SCHED_STACK_SIZE] ALIGNED(16);
static sched_thread_t g_threads[SCHED_THREADS];
static u64 g_main_rsp;
static volatile int g_current = -1;

extern void sched_switch(u64 *old_rsp, u64 new_rsp);

static void sched_yield(void);

/* Entered through sched_switch's RET. There is intentionally no return
   address: the function must never return to the bootstrap frame. */
static NORETURN void sched_thread_bootstrap(void)
{
    int id = g_current;
    for (u32 i = 0; i < SCHED_ITERATIONS; i++) {
        g_threads[id].counter++;
        sched_yield();
    }
    g_threads[id].done = true;
    sched_yield();
    kpanic("scheduler: finished thread was resumed");
}

static int next_runnable(int current)
{
    for (int step = 1; step <= SCHED_THREADS; step++) {
        int id = (current + step) % SCHED_THREADS;
        if (!g_threads[id].done) {
            return id;
        }
    }
    return -1;
}

static void sched_yield(void)
{
    u64 flags = irq_save();
    int previous = g_current;
    int next = next_runnable(previous);

    if (next < 0) {
        if (previous >= 0) {
            g_current = -1;
            sched_switch(&g_threads[previous].rsp, g_main_rsp);
        }
        irq_restore(flags);
        return;
    }

    if (next != previous) {
        g_current = next;
        sched_switch(&g_threads[previous].rsp, g_threads[next].rsp);
    }
    irq_restore(flags);
}

static void init_thread_stack(int id)
{
    u8 *top = g_thread_stacks[id] + SCHED_STACK_SIZE;
    u64 *frame = (u64 *)(top - 8 * sizeof(u64));

    /* Layout consumed by sched_switch:
       pop r15,r14,r13,r12,rbx,rbp; ret. */
    frame[0] = 0; /* r15 */
    frame[1] = 0; /* r14 */
    frame[2] = 0; /* r13 */
    frame[3] = 0; /* r12 */
    frame[4] = 0; /* rbx */
    frame[5] = 0; /* rbp */
    frame[6] = (u64)&sched_thread_bootstrap;
    /* RET leaves RSP at top - 8, which is 8 mod 16 as required at a
       normal function entry boundary. */

    g_threads[id].rsp = (u64)frame;
    g_threads[id].counter = 0;
    g_threads[id].done = false;
}

bool sched_selftest(void)
{
    memset(&g_threads, 0, sizeof(g_threads));
    for (int i = 0; i < SCHED_THREADS; i++) {
        init_thread_stack(i);
    }

    u64 flags = irq_save();
    g_current = 0;
    sched_switch(&g_main_rsp, g_threads[0].rsp);
    g_current = -1;
    irq_restore(flags);

    bool ok = true;
    for (int i = 0; i < SCHED_THREADS; i++) {
        if (!g_threads[i].done || g_threads[i].counter != SCHED_ITERATIONS) {
            ok = false;
        }
    }

    if (ok) {
        kprintf("[ok] threads: cooperative context switch A=%u B=%u, stacks restored\n",
                g_threads[0].counter, g_threads[1].counter);
    } else {
        kprintf("[warn] threads: context-switch self-test failed (A=%u done=%u, B=%u done=%u)\n",
                g_threads[0].counter, (u32)g_threads[0].done,
                g_threads[1].counter, (u32)g_threads[1].done);
    }
    return ok;
}
