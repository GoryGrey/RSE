#include "../os/SyscallDispatcher.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
#include "../os/FileDescriptor.h"
#include "../os/TorusScheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace os {
TorusContext* current_torus_context = nullptr;
}

using os::Elf64_Ehdr;
using os::Elf64_Phdr;

static bool readUserBytes(const os::OSProcess& proc, const os::PhysicalAllocator& phys,
                          uint64_t addr, void* dst, size_t len) {
  uint8_t* out = static_cast<uint8_t*>(dst);
  uint64_t remaining = len;
  uint64_t cursor = addr;
  while (remaining > 0) {
    uint64_t phys_addr = proc.memory.page_table->translate(cursor);
    if (phys_addr == 0) {
      return false;
    }
    void* ptr = phys.ptrFromPhys(phys_addr);
    if (!ptr) {
      return false;
    }
    uint64_t page_off = phys_addr & (os::PAGE_SIZE - 1);
    uint64_t chunk = os::PAGE_SIZE - page_off;
    if (chunk > remaining) {
      chunk = remaining;
    }
    std::memcpy(out, ptr, chunk);
    out += chunk;
    cursor += chunk;
    remaining -= chunk;
  }
  return true;
}

static uint64_t readUserU64(const os::OSProcess& proc, const os::PhysicalAllocator& phys,
                            uint64_t addr) {
  uint64_t value = 0;
  bool ok = readUserBytes(proc, phys, addr, &value, sizeof(value));
  assert(ok);
  return value;
}

static void readUserString(const os::OSProcess& proc, const os::PhysicalAllocator& phys,
                           uint64_t addr, char* out, size_t cap) {
  size_t pos = 0;
  while (pos + 1 < cap) {
    char c = '\0';
    bool ok = readUserBytes(proc, phys, addr + pos, &c, 1);
    assert(ok);
    out[pos++] = c;
    if (c == '\0') {
      return;
    }
  }
  out[cap - 1] = '\0';
}

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
  std::cout << "[Exec VFS Tests]" << std::endl;

  os::MemFS fs;
  os::FileDescriptorTable fdt;
  os::VFS vfs(&fs, &fdt);
  os::TorusScheduler scheduler(0);
  os::SyscallDispatcher dispatcher;

  std::array<uint8_t, 1 << 20> phys{};
  os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

  os::TorusContext ctx;
  ctx.scheduler = &scheduler;
  ctx.dispatcher = &dispatcher;
  ctx.vfs = &vfs;
  ctx.phys_alloc = &phys_alloc;
  os::current_torus_context = &ctx;

  os::OSProcess proc(1, 0, 0);
  proc.initMemory(&phys_alloc);
  scheduler.addProcess(&proc);
  scheduler.tick();

  const char payload[] = "EXEC-VFS";
  std::array<uint8_t, 2048> image{};
  writeElfImage(image, reinterpret_cast<const uint8_t*>(payload), sizeof(payload), 0x500000);

  const char path[] = "/hello.elf";
  int32_t fd = vfs.open(path, os::O_CREAT | os::O_TRUNC | os::O_WRONLY);
  assert(fd >= 0);
  int64_t written = vfs.write(fd, image.data(), image.size());
  assert(written == static_cast<int64_t>(image.size()));
  vfs.close(fd);

  int32_t cloexec_fd = vfs.open("/tmp.txt", os::O_CREAT | os::O_TRUNC | os::O_WRONLY | os::O_CLOEXEC);
  assert(cloexec_fd >= 0);

  auto writeUserString = [&](const char* str) -> uint64_t {
    size_t len = std::strlen(str) + 1;
    uint64_t addr = proc.vmem->allocate(len);
    assert(addr != 0);
    bool ok = proc.vmem->writeUser(addr, str, len);
    assert(ok);
    return addr;
  };

  uint64_t user_path = writeUserString(path);
  uint64_t arg0 = writeUserString("hello");
  uint64_t arg1 = writeUserString("world");
  uint64_t env0 = writeUserString("RSE=1");

  uint64_t argv_mem = proc.vmem->allocate(3 * sizeof(uint64_t));
  uint64_t envp_mem = proc.vmem->allocate(2 * sizeof(uint64_t));
  assert(argv_mem != 0);
  assert(envp_mem != 0);

  uint64_t argv_ptrs[3] = {arg0, arg1, 0};
  uint64_t envp_ptrs[2] = {env0, 0};
  assert(proc.vmem->writeUser(argv_mem, argv_ptrs, sizeof(argv_ptrs)));
  assert(proc.vmem->writeUser(envp_mem, envp_ptrs, sizeof(envp_ptrs)));

  int64_t rc = os::syscall(os::SYS_EXEC,
                           user_path,
                           argv_mem,
                           envp_mem);
  assert(rc == 0);
  assert(proc.context.rip == 0x500000);
  assert(proc.context.rdi == 2);
  assert(proc.context.rsi != 0);
  assert(proc.context.rdx != 0);
  assert(fdt.get(cloexec_fd) == nullptr);

  uint64_t phys_addr = proc.memory.page_table->translate(0x500000);
  assert(phys_addr != 0);
  auto* ptr = static_cast<uint8_t*>(phys_alloc.ptrFromPhys(phys_addr));
  assert(ptr != nullptr);
  assert(std::memcmp(ptr, payload, sizeof(payload)) == 0);

  uint64_t sp = proc.context.rsp;
  uint64_t argc = readUserU64(proc, phys_alloc, sp);
  assert(argc == 2);
  uint64_t argv_ptr = readUserU64(proc, phys_alloc, sp + sizeof(uint64_t));
  uint64_t envp_ptr = proc.context.rdx;
  assert(argv_ptr == proc.context.rsi);
  assert(envp_ptr != 0);

  uint64_t argv0 = readUserU64(proc, phys_alloc, argv_ptr);
  uint64_t argv1 = readUserU64(proc, phys_alloc, argv_ptr + sizeof(uint64_t));
  char buf0[16] = {};
  char buf1[16] = {};
  readUserString(proc, phys_alloc, argv0, buf0, sizeof(buf0));
  readUserString(proc, phys_alloc, argv1, buf1, sizeof(buf1));
  assert(std::strcmp(buf0, "hello") == 0);
  assert(std::strcmp(buf1, "world") == 0);

  std::cout << "  ✓ all tests passed" << std::endl;
  return 0;
}
