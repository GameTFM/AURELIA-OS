/* AURELIA OS - ring-3 process foundation and boot-time isolation test */
#include <aurelia/process.h>
#include <aurelia/paging.h>
#include <aurelia/pmm.h>
#include <aurelia/syscall.h>
#include <aurelia/gdt.h>
#include <aurelia/arch.h>
#include <aurelia/log.h>
#include <aurelia/kstring.h>

#define USER_CODE_VA  0x0000000000400000ull
#define USER_MSG_VA   0x0000000000400100ull
#define USER_STACK_TOP 0x0000000000800000ull
#define USER_PAGE     4096ull

/* This module owns one synchronous ring-3 test context. It is deliberately
   narrow: a general scheduler/process table is the next milestone. */
static u8 g_exception_stack[16384] ALIGNED(16);
static volatile bool g_user_active;
static volatile bool g_user_exited;
static volatile u64 g_user_fault_vector;
static volatile u64 g_user_fault_cs;

extern void user_enter(u64 rip, u64 rsp, u64 cr3);
extern NORETURN void user_return_to_kernel(u64 status);

/* Template:
 *   mov eax, 1
 *   mov rdi, USER_MSG_VA
 *   mov esi, msg_len
 *   syscall
 *   cli                    <- must #GP at CPL 3
 *   mov eax, 60
 *   xor edi, edi
 *   syscall                <- fallback exit if cli was wrongly allowed
 *   hlt
 */
static const u8 g_user_code[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00,
    0x48, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xBE, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x05,
    0xFA,
    0xB8, 0x3C, 0x00, 0x00, 0x00,
    0x31, 0xFF,
    0x0F, 0x05,
    0xF4
};

static const char g_user_message[] = "AURELIA ring 3: write syscall reached the kernel\n";

static void patch32(u8 *dst, u32 value)
{
    dst[0] = (u8)(value & 0xFF);
    dst[1] = (u8)((value >> 8) & 0xFF);
    dst[2] = (u8)((value >> 16) & 0xFF);
    dst[3] = (u8)((value >> 24) & 0xFF);
}

static void patch64(u8 *dst, u64 value)
{
    for (u32 i = 0; i < 8; i++) {
        dst[i] = (u8)((value >> (i * 8)) & 0xFF);
    }
}

bool process_init(void)
{
    g_user_active = false;
    g_user_exited = false;
    g_user_fault_vector = 0;
    g_user_fault_cs = 0;

    if (!syscall_init()) {
        kprintf("[warn] process: CPU does not expose SYSCALL; ring-3 test skipped\n");
        return false;
    }
    kprintf("[ok] process: minimal ring-3 foundation armed (IRETQ entry/return)\n");
    return true;
}

bool process_user_active(void) { return g_user_active; }
u64 process_user_write_count(void) { return syscall_write_count(); }

bool process_run_user_selftest(void)
{
    if (!paging_selftest()) {
        return false;
    }

    u64 root_phys = paging_create_user_space();
    if (!root_phys) {
        kprintf("[warn] ring-3 self-test: could not create user address space\n");
        return false;
    }

    u64 code_phys = pmm_alloc_page();
    u64 stack_phys = pmm_alloc_page();
    if (!code_phys || !stack_phys) {
        kprintf("[warn] ring-3 self-test: user pages unavailable\n");
        return false;
    }

    u8 *code = (u8 *)pmm_phys_to_virt(code_phys);
    memset(code, 0, USER_PAGE);
    memcpy(code, g_user_code, sizeof(g_user_code));

    size_t message_len = strlen(g_user_message);
    if (message_len >= 2048) {
        kprintf("[warn] ring-3 self-test: message template too large\n");
        return false;
    }
    memcpy(code + (USER_MSG_VA - USER_CODE_VA), g_user_message, message_len + 1);
    patch64(code + 7, USER_MSG_VA);
    patch32(code + 16, (u32)message_len);

    if (!paging_map_user(root_phys, USER_CODE_VA, code_phys, 0) ||
        !paging_map_user(root_phys, USER_STACK_TOP - USER_PAGE, stack_phys,
                         PG_WRITE)) {
        kprintf("[warn] ring-3 self-test: failed to map user pages\n");
        return false;
    }

    u64 code_query = 0, code_flags = 0;
    u64 kernel_query = 0, kernel_flags = 0;
    if (!paging_query(root_phys, USER_CODE_VA, &code_query, &code_flags) ||
        !(code_flags & PG_USER)) {
        kprintf("[warn] ring-3 self-test: user code page lacks the user bit\n");
        return false;
    }
    if (!paging_query(root_phys, (u64)&process_run_user_selftest, &kernel_query,
                      &kernel_flags) || (kernel_flags & PG_USER)) {
        kprintf("[warn] ring-3 self-test: kernel text is user-accessible in user CR3\n");
        return false;
    }
    kprintf("[ok] isolation: user page U=1, kernel text U=0 in the user address space\n");

    g_user_fault_vector = 0;
    g_user_fault_cs = 0;
    g_user_exited = false;
    g_user_active = true;

    /* TSS rsp0 is used by the CPU when the user program faults. */
    gdt_set_kernel_stack((u64)(g_exception_stack + sizeof(g_exception_stack)) - 16);

    u64 status = 0;
    user_enter(USER_CODE_VA, USER_STACK_TOP, root_phys);
    (void)status;

    /* user_return_to_kernel restores the kernel CR3 and this stack, so both the
       syscall-exit fallback and a contained fault arrive here. */
    paging_activate_kernel();
    g_user_active = false;

    bool syscall_worked = process_user_write_count() > 0;
    bool fault_contained = g_user_fault_vector == 13 && g_user_fault_cs == 0x23;

    if (syscall_worked && fault_contained) {
        kprintf("[ok] ring-3 self-test: syscall write succeeded; CLI raised vector 13 at CPL 3\n");
        kprintf("[ok] ring-3 self-test: user fault contained, kernel resumed at CPL 0\n");
        return true;
    }

    kprintf("[warn] ring-3 self-test failed: writes=%llu fault=%llu cs=%#llx exited=%u\n",
            process_user_write_count(), g_user_fault_vector, g_user_fault_cs,
            (u32)g_user_exited);
    return false;
}

void process_user_exception(interrupt_frame_t *frame)
{
    g_user_fault_vector = frame->vector;
    g_user_fault_cs = frame->cs;
    g_user_active = false;
    /* Do not return through the interrupt stub: its stack belongs to the
       abandoned user context. Resume the self-test's kernel stack instead. */
    user_return_to_kernel(frame->vector);
}

void process_user_exit(u64 code)
{
    g_user_exited = true;
    user_return_to_kernel(code);
}
