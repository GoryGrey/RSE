#include "../os/SyscallDispatcher.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
#include "../os/PhysicalAllocator.h"
#include "../os/TorusScheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace os {
TorusContext* current_torus_context = nullptr;
}

static uint64_t write_user_string(os::OSProcess& proc, const char* value) {
    size_t len = std::strlen(value) + 1;
    uint64_t addr = proc.vmem->allocate(len);
    assert(addr != 0);
    bool ok = proc.vmem->writeUser(addr, value, len);
    assert(ok);
    return addr;
}

int main() {
    std::cout << "[sys_memfs_dir Tests]" << std::endl;

    alignas(os::PAGE_SIZE) std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::MemFS memfs;
    os::VFS vfs(&memfs);

    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
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
    assert(scheduler.getCurrentProcess() == &proc);

    const char dir_path[] = "/docs";
    uint64_t dir_addr = write_user_string(proc, dir_path);
    int64_t mkdir_rc = os::syscall(os::SYS_MKDIR, dir_addr, 0755);
    assert(mkdir_rc == 0);

    uint64_t stat_addr = proc.vmem->allocate(sizeof(os::rse_stat));
    assert(stat_addr != 0);
    int64_t stat_rc = os::syscall(os::SYS_STAT, dir_addr, stat_addr);
    assert(stat_rc == 0);
    os::rse_stat st{};
    assert(proc.vmem->readUser(&st, stat_addr, sizeof(st)));
    assert(st.type == os::RSE_STAT_DIR);
    assert((st.mode & 0777u) == 0755u);

    int64_t dir_open = os::syscall(os::SYS_OPEN, dir_addr, os::O_RDONLY);
    assert(dir_open == -os::EISDIR);

    const char file_path[] = "/docs/readme.txt";
    uint64_t file_addr = write_user_string(proc, file_path);
    int64_t file_fd = os::syscall(os::SYS_OPEN, file_addr,
                                  os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(file_fd >= 0);

    std::array<char, 128> list_buf{};
    uint64_t list_addr = proc.vmem->allocate(list_buf.size());
    assert(list_addr != 0);
    int64_t list_rc = os::syscall(os::SYS_LIST, 0, list_addr, list_buf.size());
    assert(list_rc >= 0);
    assert(proc.vmem->readUser(list_buf.data(), list_addr, list_buf.size()));
    assert(std::strstr(list_buf.data(), "docs/") != nullptr);

    list_buf.fill(0);
    uint64_t dir_list_addr = proc.vmem->allocate(list_buf.size());
    assert(dir_list_addr != 0);
    int64_t dir_list_rc = os::syscall(os::SYS_LIST, dir_addr,
                                      dir_list_addr, list_buf.size());
    assert(dir_list_rc >= 0);
    assert(proc.vmem->readUser(list_buf.data(), dir_list_addr, list_buf.size()));
    assert(std::strstr(list_buf.data(), "readme.txt") != nullptr);

    int64_t rm_dir_busy = os::syscall(os::SYS_UNLINK, dir_addr);
    assert(rm_dir_busy < 0);

    int64_t rm_file = os::syscall(os::SYS_UNLINK, file_addr);
    assert(rm_file == 0);
    int64_t rm_dir = os::syscall(os::SYS_UNLINK, dir_addr);
    assert(rm_dir == 0);

    const char writeonly_dir[] = "/private";
    uint64_t writeonly_addr = write_user_string(proc, writeonly_dir);
    int64_t writeonly_mkdir = os::syscall(os::SYS_MKDIR, writeonly_addr, 0300);
    assert(writeonly_mkdir == 0);

    std::array<char, 64> perm_list_buf{};
    uint64_t perm_list_addr = proc.vmem->allocate(perm_list_buf.size());
    assert(perm_list_addr != 0);
    int64_t perm_list_rc = os::syscall(os::SYS_LIST, writeonly_addr,
                                       perm_list_addr, perm_list_buf.size());
    assert(perm_list_rc == -os::EACCES);

    const char writeonly_file[] = "/private/data.txt";
    uint64_t writeonly_file_addr = write_user_string(proc, writeonly_file);
    int64_t writeonly_fd = os::syscall(os::SYS_OPEN, writeonly_file_addr,
                                       os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(writeonly_fd >= 0);
    (void)os::syscall(os::SYS_CLOSE, writeonly_fd);

    const char ro_path[] = "/ro.txt";
    uint64_t ro_addr = write_user_string(proc, ro_path);
    int64_t ro_fd = os::syscall(os::SYS_OPEN, ro_addr,
                                os::O_CREAT | os::O_TRUNC, 0400);
    assert(ro_fd >= 0);
    int64_t ro_close = os::syscall(os::SYS_CLOSE, ro_fd);
    assert(ro_close == 0);

    int64_t ro_open_write = os::syscall(os::SYS_OPEN, ro_addr, os::O_WRONLY);
    assert(ro_open_write == -os::EACCES);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
