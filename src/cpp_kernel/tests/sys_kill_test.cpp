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
    std::cout << "[sys_kill Tests]" << std::endl;

    alignas(os::PAGE_SIZE) std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
    os::TorusContext ctx;
    ctx.scheduler = &scheduler;
    ctx.dispatcher = &dispatcher;
    ctx.phys_alloc = &phys_alloc;
    os::current_torus_context = &ctx;

    os::OSProcess parent(1, 0, 0);
    parent.initMemory(&phys_alloc);
    os::OSProcess child(2, 1, 0);
    child.initMemory(&phys_alloc);

    scheduler.addProcess(&parent);
    scheduler.addProcess(&child);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &parent);

    int64_t prev_term = os::syscall(os::SYS_SIGNAL, os::SIGTERM, os::SIG_IGN);
    assert(prev_term == os::SIG_DFL);
    int64_t bad_sig = os::syscall(os::SYS_SIGNAL, os::SIGKILL, os::SIG_IGN);
    assert(bad_sig == -os::EINVAL);
    int64_t bad_handler = os::syscall(os::SYS_SIGNAL, os::SIGTERM, 0xdeadbeef);
    assert(bad_handler == -os::ENOSYS);

    int64_t ignored = os::syscall(os::SYS_KILL, 1, os::SIGTERM);
    assert(ignored == 0);
    assert(scheduler.hasProcess(1));

    int64_t exists_rc = os::syscall(os::SYS_KILL, 2, 0);
    assert(exists_rc == 0);
    int64_t missing_rc = os::syscall(os::SYS_KILL, 999, 0);
    assert(missing_rc == -os::ESRCH);

    int64_t kill_rc = os::syscall(os::SYS_KILL, 2, 9);
    assert(kill_rc == 0);

    uint64_t status_addr = parent.vmem->allocate(sizeof(int));
    assert(status_addr != 0);
    int64_t waited = os::syscall(os::SYS_WAIT, status_addr);
    assert(waited == 2);

    int status = 0;
    bool ok = parent.vmem->readUser(&status, status_addr, sizeof(status));
    assert(ok);
    assert(status == 137);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
