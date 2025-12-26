ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS {
    . = 0x10000000;

    .text : ALIGN(16) {
        *(.text .text.*)
    } :text

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    } :text

    .limine_reqs : ALIGN(16) {
        KEEP(*(.limine_reqs))
    } :text

    .data : ALIGN(16) {
        *(.data .data.*)
    } :data

    .bss : ALIGN(16) {
        *(COMMON)
        *(.bss .bss.*)
    } :data
}
