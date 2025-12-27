#include "../os/SyscallDispatcher.h"
#include "../os/PhysicalAllocator.h"
#include "../os/TorusScheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace os {
TorusContext* current_torus_context = nullptr;
}

int main() {
    std::cout << "[sys_wait Tests]" << std::endl;

    std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
    os::TorusContext ctx;
    ctx.scheduler = &scheduler;
    ctx.dispatcher = &dispatcher;
    ctx.phys_alloc = &phys_alloc;
    ctx.next_pid = 2;
    os::current_torus_context = &ctx;

    os::OSProcess parent(1, 0, 0);
    parent.initMemory(&phys_alloc);
    scheduler.addProcess(&parent);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &parent);

    int64_t child_pid = os::syscall(os::SYS_FORK);
    assert(child_pid > 1);

    int64_t early_wait = os::syscall(os::SYS_WAIT, 0);
    assert(early_wait == -os::EAGAIN);

    os::OSProcess* child = nullptr;
    scheduler.forEachProcess([&](os::OSProcess* proc) {
        if (proc && proc->pid == static_cast<uint32_t>(child_pid)) {
            child = proc;
        }
    });
    assert(child != nullptr);

    parent.time_slice = 0;
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == child);

    int64_t exit_rc = os::syscall(os::SYS_EXIT, 42);
    assert(exit_rc == 0);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &parent);

    uint64_t status_addr = parent.vmem->allocate(sizeof(int));
    assert(status_addr != 0);
    int64_t waited = os::syscall(os::SYS_WAIT, status_addr);
    assert(waited == child_pid);

    int status = 0;
    bool ok = parent.vmem->readUser(&status, status_addr, sizeof(status));
    assert(ok);
    assert(status == 42);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
