/* AURELIA OS - ring-3 entry, SYSCALL entry and kernel context restore */
.section .text

/* Saved kernel context for the synchronous user-mode self-test. */
.section .bss
.align 16
g_kernel_resume_rsp:
    .quad 0
g_kernel_cr3:
    .quad 0
g_kernel_rflags:
    .quad 0
g_user_rsp:
    .quad 0
g_user_rip:
    .quad 0
g_user_rflags:
    .quad 0
.align 16
g_syscall_stack_bottom:
    .skip 16384
g_syscall_stack_top:

.section .text
.global user_enter
/* void user_enter(u64 rip, u64 rsp, u64 cr3) */
user_enter:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, g_kernel_resume_rsp(%rip)
    movq %cr3, %rax
    movq %rax, g_kernel_cr3(%rip)
    pushfq
    popq %rax
    movq %rax, g_kernel_rflags(%rip)

    /* Build a ring-3 interrupt-return frame. USER_DS|3 and USER_CS|3. */
    pushq $0x1b
    pushq %rsi
    pushq $0x202
    pushq $0x23
    pushq %rdi

    /* Use user data segments while at CPL3. FS/GS are unused by the test. */
    movw $0x1b, %ax
    movw %ax, %ds
    movw %ax, %es

    movq %rdx, %cr3
    iretq

.global user_return_to_kernel
/* void user_return_to_kernel(u64 status) - deliberately never returns here.
 * It discards the current (user/syscall/exception) stack and resumes the C
 * caller of user_enter with the saved kernel CR3 and callee-saved registers. */
user_return_to_kernel:
    cli
    movq g_kernel_cr3(%rip), %rax
    movq %rax, %cr3
    movq %rdi, %rax

    movw $0x10, %cx
    movw %cx, %ds
    movw %cx, %es
    movw %cx, %fs
    movw %cx, %gs

    movq g_kernel_resume_rsp(%rip), %rsp
    pushq g_kernel_rflags(%rip)
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

.global syscall_entry
syscall_entry:
    cld
    movq %rsp, g_user_rsp(%rip)
    movq %rcx, g_user_rip(%rip)
    movq %r11, g_user_rflags(%rip)

    leaq g_syscall_stack_top(%rip), %rsp

    /* Preserve syscall arguments while switching to kernel data segments. */
    movq %rax, %r10
    movq %rdi, %r8
    movq %rsi, %r9
    movq %rdx, %r11
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movq %r10, %rdi
    movq %r8, %rsi
    movq %r9, %rdx
    movq %r11, %rcx
    call syscall_dispatch

    /* Restore user data segments without disturbing the RAX return value. */
    movw $0x1b, %cx
    movw %cx, %ds
    movw %cx, %es

    /* Return value is in RAX; IRETQ restores the saved user context. */
    pushq $0x1b
    pushq g_user_rsp(%rip)
    pushq g_user_rflags(%rip)
    pushq $0x23
    pushq g_user_rip(%rip)
    iretq
