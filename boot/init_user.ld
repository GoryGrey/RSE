ENTRY(_start)

PHDRS {
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS {
  . = 0x40000000;

  .text : ALIGN(0x1000) {
    *(.text.startup)
    *(.text*)
  } :text

  .rodata : ALIGN(0x1000) {
    *(.rodata*)
  } :text

  .data : ALIGN(0x1000) {
    *(.data*)
  } :data

  .bss : ALIGN(0x1000) {
    *(.bss*)
    *(COMMON)
  } :data
}
