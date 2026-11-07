/* AURELIA OS - kernel entry (handoff from Limine, 64-bit long mode) */
.section .text.entry
.global _start
_start:
    cli
    lea stack_top(%rip), %rsp
    and $-16, %rsp
    xor %ebp, %ebp

    /* Zero the BSS region (Limine does not guarantee zeroed BSS). */
    lea __bss_start(%rip), %rdi
    lea __bss_end(%rip), %rcx
    sub %rdi, %rcx
    xor %eax, %eax
    rep stosb

    call kmain

1:  cli
    hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 65536
stack_top:
