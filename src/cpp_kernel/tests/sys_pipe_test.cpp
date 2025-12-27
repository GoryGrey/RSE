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
    std::cout << "[sys_pipe Tests]" << std::endl;
    auto require = [](bool ok, const char* msg) -> bool {
        if (!ok) {
            std::cerr << "  ✗ " << msg << std::endl;
            return false;
        }
        return true;
    };

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

    uint64_t fds_addr = proc.vmem->allocate(sizeof(int) * 2);
    if (!require(fds_addr != 0, "alloc fds addr")) {
        return 1;
    }
    int64_t rc = os::syscall(os::SYS_PIPE, fds_addr);
    if (!require(rc == 0, "sys_pipe returned error")) {
        return 1;
    }

    int fds[2] = {-1, -1};
    bool ok = proc.vmem->readUser(fds, fds_addr, sizeof(fds));
    if (!require(ok, "read fds from user memory")) {
        return 1;
    }
    if (!require(fds[0] >= 0 && fds[1] >= 0, "pipe fds are valid")) {
        return 1;
    }

    const char payload[] = "pipe-data";
    uint64_t payload_addr = proc.vmem->allocate(sizeof(payload) - 1);
    if (!require(payload_addr != 0, "alloc payload addr")) {
        return 1;
    }
    if (!require(proc.vmem->writeUser(payload_addr, payload, sizeof(payload) - 1),
                 "write payload")) {
        return 1;
    }
    rc = os::syscall(os::SYS_WRITE, fds[1], payload_addr, sizeof(payload) - 1);
    if (!require(rc == (int64_t)(sizeof(payload) - 1), "pipe write")) {
        return 1;
    }

    char out[32] = {};
    uint64_t out_addr = proc.vmem->allocate(sizeof(out));
    if (!require(out_addr != 0, "alloc read addr")) {
        return 1;
    }
    rc = os::syscall(os::SYS_READ, fds[0], out_addr, sizeof(out));
    if (!require(rc == (int64_t)(sizeof(payload) - 1), "pipe read")) {
        return 1;
    }
    ok = proc.vmem->readUser(out, out_addr, sizeof(out));
    if (!require(ok, "read pipe payload")) {
        return 1;
    }
    if (!require(std::memcmp(out, payload, sizeof(payload) - 1) == 0,
                 "pipe payload matches")) {
        return 1;
    }

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
