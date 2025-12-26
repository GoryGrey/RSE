OUTPUT_FORMAT(efi-app-x86_64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS {
  . = 0;
  .text : { *(.text*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  .reloc : { *(.reloc) }
}
