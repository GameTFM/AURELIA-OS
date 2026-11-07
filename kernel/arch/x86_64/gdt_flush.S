/* AURELIA OS - GDT and TSS loading */
.section .text
.global gdt_flush
gdt_flush:
    lgdt (%rdi)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    pushq $0x08
    lea 1f(%rip), %rax
    pushq %rax
    lretq
1:
    ret

.global tss_flush
tss_flush:
    mov $0x28, %ax
    ltr %ax
    ret

.global cpu_halt_forever
cpu_halt_forever:
    cli
1:  hlt
    jmp 1b
