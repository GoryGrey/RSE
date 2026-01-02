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
    std::cout << "[sys_dup Tests]" << std::endl;
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

    const char path[] = "/dup.txt";
    uint64_t path_addr = proc.vmem->allocate(sizeof(path));
    if (!require(path_addr != 0, "alloc path")) {
        return 1;
    }
    if (!require(proc.vmem->writeUser(path_addr, path, sizeof(path)), "write path")) {
        return 1;
    }

    int64_t fd = os::syscall(os::SYS_OPEN, path_addr,
                             os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    if (!require(fd >= 0, "open file")) {
        return 1;
    }

    const char payload[] = "dupdata";
    uint64_t payload_addr = proc.vmem->allocate(sizeof(payload) - 1);
    if (!require(payload_addr != 0, "alloc payload")) {
        return 1;
    }
    if (!require(proc.vmem->writeUser(payload_addr, payload, sizeof(payload) - 1),
                 "write payload")) {
        return 1;
    }
    int64_t wrote = os::syscall(os::SYS_WRITE, fd, payload_addr, sizeof(payload) - 1);
    if (!require(wrote == (int64_t)(sizeof(payload) - 1), "write payload via fd")) {
        return 1;
    }
    (void)os::syscall(os::SYS_LSEEK, fd, 0, SEEK_SET);

    int64_t dup_fd = os::syscall(os::SYS_DUP, fd);
    if (!require(dup_fd >= 0, "dup fd")) {
        return 1;
    }

    char out[16] = {};
    uint64_t out_addr = proc.vmem->allocate(sizeof(out));
    if (!require(out_addr != 0, "alloc read buffer")) {
        return 1;
    }
    int64_t read_len = os::syscall(os::SYS_READ, dup_fd, out_addr, sizeof(out));
    if (!require(read_len == (int64_t)(sizeof(payload) - 1), "read via dup fd")) {
        return 1;
    }
    bool ok = proc.vmem->readUser(out, out_addr, sizeof(out));
    if (!require(ok, "read dup payload")) {
        return 1;
    }
    if (!require(std::memcmp(out, payload, sizeof(payload) - 1) == 0,
                 "dup payload matches")) {
        return 1;
    }

    int target_fd = 8;
    int64_t dup2_fd = os::syscall(os::SYS_DUP2, fd, target_fd);
    if (!require(dup2_fd == target_fd, "dup2 target")) {
        return 1;
    }
    (void)os::syscall(os::SYS_LSEEK, dup2_fd, 0, SEEK_SET);
    std::memset(out, 0, sizeof(out));
    read_len = os::syscall(os::SYS_READ, dup2_fd, out_addr, sizeof(out));
    if (!require(read_len == (int64_t)(sizeof(payload) - 1), "read via dup2")) {
        return 1;
    }
    ok = proc.vmem->readUser(out, out_addr, sizeof(out));
    if (!require(ok, "read dup2 payload")) {
        return 1;
    }
    if (!require(std::memcmp(out, payload, sizeof(payload) - 1) == 0,
                 "dup2 payload matches")) {
        return 1;
    }

    int64_t clo_fd = os::syscall(os::SYS_OPEN, path_addr,
                                 os::O_CREAT | os::O_TRUNC | os::O_RDWR | os::O_CLOEXEC);
    if (!require(clo_fd >= 0, "open O_CLOEXEC")) {
        return 1;
    }
    os::FileDescriptor* clo_desc = proc.fd_table.get(static_cast<int32_t>(clo_fd));
    if (!require(clo_desc && clo_desc->closeOnExec(), "CLOEXEC set on original")) {
        return 1;
    }

    int64_t clo_dup = os::syscall(os::SYS_DUP, clo_fd);
    if (!require(clo_dup >= 0, "dup CLOEXEC fd")) {
        return 1;
    }
    os::FileDescriptor* clo_dup_desc = proc.fd_table.get(static_cast<int32_t>(clo_dup));
    if (!require(clo_dup_desc && !clo_dup_desc->closeOnExec(),
                 "dup clears CLOEXEC")) {
        return 1;
    }

    int target_clo_fd = 9;
    int64_t clo_dup2 = os::syscall(os::SYS_DUP2, clo_fd, target_clo_fd);
    if (!require(clo_dup2 == target_clo_fd, "dup2 CLOEXEC target")) {
        return 1;
    }
    os::FileDescriptor* clo_dup2_desc = proc.fd_table.get(static_cast<int32_t>(clo_dup2));
    if (!require(clo_dup2_desc && !clo_dup2_desc->closeOnExec(),
                 "dup2 clears CLOEXEC")) {
        return 1;
    }

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
