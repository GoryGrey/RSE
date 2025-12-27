#include "../os/SyscallDispatcher.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
#include "../os/FileDescriptor.h"
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

int main() {
    std::cout << "[sys_stat Tests]" << std::endl;

    std::array<uint8_t, 1 << 20> phys{};
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

    int fd = vfs.open(&proc.fd_table, "hello.txt",
                      os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(fd >= 0);
    const char payload[] = "hello";
    int64_t wrote = vfs.write(&proc.fd_table, fd, payload, sizeof(payload) - 1);
    assert(wrote == 5);
    vfs.close(&proc.fd_table, fd);

    const char path[] = "hello.txt";
    uint64_t path_addr = proc.vmem->allocate(sizeof(path));
    assert(path_addr != 0);
    assert(proc.vmem->writeUser(path_addr, path, sizeof(path)));

    uint64_t stat_addr = proc.vmem->allocate(sizeof(os::rse_stat));
    assert(stat_addr != 0);
    int64_t rc = os::syscall(os::SYS_STAT, path_addr, stat_addr);
    assert(rc == 0);

    os::rse_stat st{};
    bool ok = proc.vmem->readUser(&st, stat_addr, sizeof(st));
    assert(ok);
    assert(st.size == 5);
    assert(st.type == os::RSE_STAT_FILE);

    const char root_path[] = "/";
    uint64_t root_addr = proc.vmem->allocate(sizeof(root_path));
    assert(root_addr != 0);
    assert(proc.vmem->writeUser(root_addr, root_path, sizeof(root_path)));

    uint64_t root_stat_addr = proc.vmem->allocate(sizeof(os::rse_stat));
    assert(root_stat_addr != 0);
    rc = os::syscall(os::SYS_STAT, root_addr, root_stat_addr);
    assert(rc == 0);
    ok = proc.vmem->readUser(&st, root_stat_addr, sizeof(st));
    assert(ok);
    assert(st.type == os::RSE_STAT_DIR);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
