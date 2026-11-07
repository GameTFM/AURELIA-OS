/* AURELIA OS - AURELIA Kernel linker script (x86_64 higher-half, Limine protocol) */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R X */
    rodata PT_LOAD FLAGS(4); /* R   */
    data PT_LOAD FLAGS(6);   /* RW  */
}

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(4096)
    {
        *(.text.entry)
        *(.text .text.*)
    } :text

    .rodata : ALIGN(4096)
    {
        *(.rodata .rodata.*)
    } :rodata

    /* Writable: the bootloader stores response pointers in these structures. */
    .data : ALIGN(4096)
    {
        *(.limine_requests_start)
        *(.limine_requests)
        *(.limine_requests_end)
        *(.data .data.*)
    } :data

    .bss : ALIGN(4096)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(16);
        __bss_end = .;
    } :data

    __kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
        *(.interp)
        *(.dynamic)
        *(.got .got.plt)
    }
}
