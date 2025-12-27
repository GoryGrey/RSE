#include "../os/SyscallDispatcher.h"
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
    std::cout << "[sys_ps Tests]" << std::endl;

    std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
    os::TorusContext ctx;
    ctx.scheduler = &scheduler;
    ctx.dispatcher = &dispatcher;
    ctx.phys_alloc = &phys_alloc;
    os::current_torus_context = &ctx;

    os::OSProcess proc1(1, 0, 0);
    os::OSProcess proc2(2, 1, 0);
    proc1.initMemory(&phys_alloc);
    proc2.initMemory(&phys_alloc);

    scheduler.addProcess(&proc1);
    scheduler.addProcess(&proc2);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() != nullptr);

    uint64_t buf_addr = proc1.vmem->allocate(512);
    assert(buf_addr != 0);

    int64_t wrote = os::syscall(os::SYS_PS, buf_addr, 512);
    assert(wrote > 0);

    char out[512] = {};
    size_t read_len = (wrote < (int64_t)(sizeof(out) - 1))
        ? (size_t)wrote
        : (sizeof(out) - 1);
    bool ok = proc1.vmem->readUser(out, buf_addr, read_len);
    assert(ok);
    out[read_len] = '\0';

    assert(std::strstr(out, "pid=1") != nullptr);
    assert(std::strstr(out, "pid=2") != nullptr);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
