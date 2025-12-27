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
    std::cout << "[sys_mmap Tests]" << std::endl;

    alignas(os::PAGE_SIZE) std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
    os::TorusContext ctx;
    ctx.scheduler = &scheduler;
    ctx.dispatcher = &dispatcher;
    ctx.phys_alloc = &phys_alloc;
    os::current_torus_context = &ctx;

    os::OSProcess proc(1, 0, 0);
    proc.initMemory(&phys_alloc);
    scheduler.addProcess(&proc);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &proc);

    int64_t mapped = os::syscall(os::SYS_MMAP, 0, os::PAGE_SIZE * 2,
                                 os::PROT_READ | os::PROT_WRITE);
    assert(mapped > 0);
    assert(proc.vmem->isUserRange((uint64_t)mapped, os::PAGE_SIZE * 2));

    int64_t rc = os::syscall(os::SYS_MPROTECT, (uint64_t)mapped, os::PAGE_SIZE,
                             os::PROT_READ);
    assert(rc == 0);

    rc = os::syscall(os::SYS_MUNMAP, (uint64_t)mapped, os::PAGE_SIZE * 2);
    assert(rc == 0);

    uint64_t bad_addr = proc.vmem->getStackEnd() + os::PAGE_SIZE;
    rc = os::syscall(os::SYS_MMAP, bad_addr, os::PAGE_SIZE, os::PROT_READ);
    assert(rc == -os::EFAULT);
    rc = os::syscall(os::SYS_MPROTECT, bad_addr, os::PAGE_SIZE, os::PROT_READ);
    assert(rc == -os::EFAULT);
    rc = os::syscall(os::SYS_MUNMAP, bad_addr, os::PAGE_SIZE);
    assert(rc == -os::EFAULT);

    uint64_t bad_brk = proc.vmem->getHeapEnd() + os::PAGE_SIZE;
    rc = os::syscall(os::SYS_BRK, bad_brk);
    assert(rc == -os::ENOMEM);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
