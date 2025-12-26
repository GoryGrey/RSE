#include "../os/OSProcess.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

using os::Elf64_Ehdr;
using os::Elf64_Phdr;

static void writeElfImage(std::array<uint8_t, 2048>& buf, const uint8_t* payload,
                          size_t payload_len, uint64_t entry) {
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
  ehdr.e_entry = entry;
  ehdr.e_phoff = sizeof(Elf64_Ehdr);
  ehdr.e_phentsize = sizeof(Elf64_Phdr);
  ehdr.e_phnum = 1;

  Elf64_Phdr phdr{};
  phdr.p_type = os::PT_LOAD;
  phdr.p_flags = os::PF_R | os::PF_X;
  phdr.p_offset = 0x100;
  phdr.p_vaddr = entry;
  phdr.p_paddr = entry;
  phdr.p_filesz = payload_len;
  phdr.p_memsz = 0x1000;
  phdr.p_align = 0x1000;

  std::memcpy(buf.data(), &ehdr, sizeof(ehdr));
  std::memcpy(buf.data() + ehdr.e_phoff, &phdr, sizeof(phdr));
  std::memcpy(buf.data() + phdr.p_offset, payload, payload_len);
}

int main() {
  std::cout << "[ElfProcess Tests]" << std::endl;

  std::array<uint8_t, 1 << 20> phys{};
  os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

  os::OSProcess proc(1, 0, 0);
  proc.initMemory(&phys_alloc);

  const char payload[] = "ELF-OK";
  std::array<uint8_t, 2048> image{};
  writeElfImage(image, reinterpret_cast<const uint8_t*>(payload), sizeof(payload), 0x400000);

  bool ok = proc.loadElfImage(image.data(), image.size());
  assert(ok);
  assert(proc.context.rip == 0x400000);
  assert(proc.context.rsp == proc.memory.stack_pointer);

  uint64_t phys_addr = proc.memory.page_table->translate(0x400000);
  assert(phys_addr != 0);
  auto* ptr = static_cast<uint8_t*>(phys_alloc.ptrFromPhys(phys_addr));
  assert(ptr != nullptr);
  assert(std::memcmp(ptr, payload, sizeof(payload)) == 0);
  assert(ptr[sizeof(payload)] == 0);

  std::cout << "  ✓ all tests passed" << std::endl;
  return 0;
}
