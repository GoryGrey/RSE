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

static int g_signal_seen = 0;

static void test_signal_handler(int sig) {
    g_signal_seen = sig;
}

static void noop_user_step(os::OSProcess*, void*, const rse_syscalls*) {}

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
    parent.uid = 0;
    parent.initMemory(&phys_alloc);
    os::OSProcess child(2, 1, 0);
    child.uid = 1001;
    child.initMemory(&phys_alloc);
    os::OSProcess other(3, 2, 0);
    other.uid = 1002;
    other.initMemory(&phys_alloc);

    scheduler.addProcess(&parent);
    scheduler.addProcess(&child);
    scheduler.addProcess(&other);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &parent);

    int64_t prev_term = os::syscall(os::SYS_SIGNAL, os::SIGTERM, os::SIG_IGN);
    assert(prev_term == os::SIG_DFL);
    int64_t bad_sig = os::syscall(os::SYS_SIGNAL, os::SIGKILL, os::SIG_IGN);
    assert(bad_sig == -os::EINVAL);
    int64_t bad_handler = os::syscall(os::SYS_SIGNAL, os::SIGTERM, 0xdeadbeef);
    assert(bad_handler == -os::EFAULT);

    uint64_t user_handler_addr = parent.vmem->allocate(sizeof(uint64_t));
    assert(user_handler_addr != 0);
    int64_t user_prev = os::syscall(os::SYS_SIGNAL, os::SIGTERM, user_handler_addr);
    assert(user_prev == os::SIG_DFL);
    parent.signal_handlers[os::SIGTERM] = os::SIG_DFL;

    parent.setUserEntry(noop_user_step, nullptr, nullptr);
    int64_t handler_prev = os::syscall(os::SYS_SIGNAL, os::SIGTERM,
                                       (uint64_t)test_signal_handler);
    assert(handler_prev == os::SIG_DFL);
    int64_t handler_kill = os::syscall(os::SYS_KILL, 1, os::SIGTERM);
    assert(handler_kill == 0);
    assert(g_signal_seen == os::SIGTERM);
    assert(scheduler.hasProcess(1));
    parent.setUserEntry(nullptr, nullptr, nullptr);
    parent.signal_handlers[os::SIGTERM] = os::SIG_DFL;

    int64_t ignored = os::syscall(os::SYS_KILL, 1, os::SIGTERM);
    assert(ignored == 0);
    assert(scheduler.hasProcess(1));

    int64_t exists_rc = os::syscall(os::SYS_KILL, 2, 0);
    assert(exists_rc == 0);
    int64_t missing_rc = os::syscall(os::SYS_KILL, 999, 0);
    assert(missing_rc == -os::ESRCH);

    parent.uid = 1000;
    int64_t denied_probe = os::syscall(os::SYS_KILL, 2, 0);
    assert(denied_probe == -os::EACCES);
    parent.uid = 0;

    scheduler.forceCurrentProcess(&other);
    int64_t denied = os::syscall(os::SYS_KILL, 2, os::SIGTERM);
    assert(denied == -os::EACCES);
    scheduler.forceCurrentProcess(&parent);

    int64_t stop_rc = os::syscall(os::SYS_KILL, 2, os::SIGSTOP);
    assert(stop_rc == 0);
    assert(child.isBlocked());
    assert(child.isStopped());

    int64_t cont_rc = os::syscall(os::SYS_KILL, 2, os::SIGCONT);
    assert(cont_rc == 0);
    assert(!child.isStopped());
    assert(child.isReady());

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
