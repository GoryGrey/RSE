#include "../os/ElfLoader.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

using os::Elf64_Ehdr;
using os::Elf64_Phdr;
using os::ElfImage;
using os::ElfLoadError;
using os::parseElf64;

static void testValidElf() {
  std::array<uint8_t, 1024> buf{};
  std::memset(buf.data(), 0, buf.size());

  Elf64_Ehdr ehdr{};
  ehdr.e_ident[os::EI_MAG0] = os::ELF_MAGIC_0;
  ehdr.e_ident[os::EI_MAG1] = os::ELF_MAGIC_1;
  ehdr.e_ident[os::EI_MAG2] = os::ELF_MAGIC_2;
  ehdr.e_ident[os::EI_MAG3] = os::ELF_MAGIC_3;
  ehdr.e_ident[os::EI_CLASS] = os::ELFCLASS64;
  ehdr.e_ident[os::EI_DATA] = os::ELFDATA2LSB;
  ehdr.e_ident[os::EI_VERSION] = 1;
  ehdr.e_machine = os::EM_X86_64;
  ehdr.e_entry = 0x400000;
  ehdr.e_phoff = sizeof(Elf64_Ehdr);
  ehdr.e_phentsize = sizeof(Elf64_Phdr);
  ehdr.e_phnum = 1;

  Elf64_Phdr phdr{};
  phdr.p_type = os::PT_LOAD;
  phdr.p_flags = os::PF_R | os::PF_X;
  phdr.p_offset = 0x100;
  phdr.p_vaddr = 0x400000;
  phdr.p_paddr = 0x400000;
  phdr.p_filesz = 0x10;
  phdr.p_memsz = 0x20;
  phdr.p_align = 0x1000;

  std::memcpy(buf.data(), &ehdr, sizeof(ehdr));
  std::memcpy(buf.data() + ehdr.e_phoff, &phdr, sizeof(phdr));

  ElfImage image{};
  ElfLoadError err = ElfLoadError::kOk;
  bool ok = parseElf64(buf.data(), buf.size(), image, &err);
  assert(ok);
  assert(err == ElfLoadError::kOk);
  assert(image.entry == 0x400000);
  assert(image.segments.size() == 1);
  assert(image.segments[0].vaddr == 0x400000);
  assert(image.segments[0].memsz == 0x20);
  assert(image.segments[0].filesz == 0x10);
  assert(image.segments[0].offset == 0x100);
  assert((image.segments[0].flags & os::PF_X) != 0);
}

static void testBadMagic() {
  std::array<uint8_t, 64> buf{};
  ElfImage image{};
  ElfLoadError err = ElfLoadError::kOk;
  bool ok = parseElf64(buf.data(), buf.size(), image, &err);
  assert(!ok);
  assert(err == ElfLoadError::kBadMagic);
}

int main() {
  std::cout << "[ElfLoader Tests]" << std::endl;
  testValidElf();
  testBadMagic();
  std::cout << "  ✓ all tests passed" << std::endl;
  return 0;
}
