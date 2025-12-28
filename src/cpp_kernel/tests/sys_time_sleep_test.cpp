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
    std::cout << "[sys_time_sleep Tests]" << std::endl;

    alignas(os::PAGE_SIZE) std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
    os::TorusContext ctx;
    ctx.scheduler = &scheduler;
    ctx.dispatcher = &dispatcher;
    ctx.phys_alloc = &phys_alloc;
    ctx.next_pid = 2;
    os::current_torus_context = &ctx;

    os::OSProcess proc(1, 0, 0);
    proc.initMemory(&phys_alloc);
    scheduler.addProcess(&proc);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &proc);

    uint64_t time_addr = proc.vmem->allocate(sizeof(uint64_t));
    assert(time_addr != 0);
    int64_t time0 = os::syscall(os::SYS_TIME, time_addr);
    assert(time0 >= 0);

    uint64_t time0_mem = 0;
    bool ok = proc.vmem->readUser(&time0_mem, time_addr, sizeof(time0_mem));
    assert(ok);
    assert(time0_mem == static_cast<uint64_t>(time0));

    constexpr uint64_t kTicksPerSecond = 1000;
    uint64_t start_ticks = scheduler.getTicks();
    for (uint64_t i = 0; i < kTicksPerSecond; ++i) {
        scheduler.tick();
    }

    int64_t time1 = os::syscall(os::SYS_TIME, time_addr);
    uint64_t time1_mem = 0;
    ok = proc.vmem->readUser(&time1_mem, time_addr, sizeof(time1_mem));
    assert(ok);
    assert(time1_mem == static_cast<uint64_t>(time1));
    uint64_t expected_time = (start_ticks + kTicksPerSecond) / kTicksPerSecond;
    assert(static_cast<uint64_t>(time1) == expected_time);

    uint64_t runtime_before = proc.total_runtime;
    int64_t sleep_rc = os::syscall(os::SYS_SLEEP, 1);
    assert(sleep_rc == 0);
    assert(scheduler.getCurrentProcess() == nullptr);

    for (uint64_t i = 0; i < kTicksPerSecond - 1; ++i) {
        scheduler.tick();
    }
    assert(scheduler.getCurrentProcess() == nullptr);
    assert(proc.total_runtime == runtime_before);

    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &proc);

    os::rse_timespec req = {};
    req.tv_nsec = 5 * 1000000ull; // 5ms
    uint64_t req_addr = proc.vmem->allocate(sizeof(req));
    assert(req_addr != 0);
    ok = proc.vmem->writeUser(req_addr, &req, sizeof(req));
    assert(ok);

    int64_t nanosleep_rc = os::syscall(os::SYS_NANOSLEEP, req_addr, 0);
    assert(nanosleep_rc == 0);
    assert(scheduler.getCurrentProcess() == nullptr);

    for (uint64_t i = 0; i < 4; ++i) {
        scheduler.tick();
    }
    assert(scheduler.getCurrentProcess() == nullptr);

    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &proc);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
