#include "../os/SyscallDispatcher.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
#include "../os/PhysicalAllocator.h"
#include "../os/TorusScheduler.h"
#include "../os/PageTable.h"

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
    std::cout << "[sys_user_isolation Tests]" << std::endl;

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
    ctx.next_pid = 2;
    os::current_torus_context = &ctx;

    os::OSProcess proc(1, 0, 0);
    proc.uid = 1001;
    proc.gid = 1002;
    proc.signal_handlers[os::SIGTERM] = os::SIG_IGN;
    proc.initMemory(&phys_alloc);
    scheduler.addProcess(&proc);
    scheduler.tick();
    assert(scheduler.getCurrentProcess() == &proc);

    int64_t fork_pid = os::syscall(os::SYS_FORK);
    assert(fork_pid == 2);
    os::OSProcess* child = scheduler.findProcess(static_cast<uint32_t>(fork_pid));
    assert(child != nullptr);
    assert(child->parent_pid == proc.pid);
    assert(child->uid == proc.uid);
    assert(child->gid == proc.gid);
    assert(child->signal_handlers[os::SIGTERM] == os::SIG_IGN);

    uint64_t guard_bytes = proc.vmem->getStackGuardBytes();
    if (guard_bytes > 0) {
        uint64_t guard_start = proc.vmem->getStackMappedStart() - guard_bytes;
        for (uint64_t addr = guard_start;
             addr < proc.vmem->getStackMappedStart();
             addr += os::PAGE_SIZE) {
            assert(!proc.vmem->getPageTable()->isMapped(addr));
        }
    }

    const char path[] = "/hello.txt";
    uint64_t path_addr = write_user_string(proc, path);
    int64_t fd = os::syscall(os::SYS_OPEN, path_addr,
                             os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(fd >= 0);

    const char payload[] = "isolation";
    uint64_t payload_addr = proc.vmem->allocate(sizeof(payload) - 1);
    assert(payload_addr != 0);
    assert(proc.vmem->writeUser(payload_addr, payload, sizeof(payload) - 1));
    int64_t wrote = os::syscall(os::SYS_WRITE, fd, payload_addr, sizeof(payload) - 1);
    assert(wrote == static_cast<int64_t>(sizeof(payload) - 1));

    uint64_t huge_count = static_cast<uint64_t>(UINT32_MAX) + 1;
    int64_t huge_write = os::syscall(os::SYS_WRITE, fd, payload_addr, huge_count);
    assert(huge_write == -os::EINVAL);
    int64_t huge_read = os::syscall(os::SYS_READ, fd, payload_addr, huge_count);
    assert(huge_read == -os::EINVAL);

    uint64_t bad_addr = proc.vmem->getStackEnd() + os::PAGE_SIZE;
    int64_t bad_write = os::syscall(os::SYS_WRITE, fd, bad_addr, 4);
    assert(bad_write == -os::EFAULT);
    int64_t bad_read = os::syscall(os::SYS_READ, fd, bad_addr, 4);
    assert(bad_read == -os::EFAULT);

    std::array<char, 32> list_buf{};
    uint64_t list_addr = proc.vmem->allocate(list_buf.size());
    assert(list_addr != 0);
    int64_t bad_list = os::syscall(os::SYS_LIST, bad_addr, list_addr, list_buf.size());
    assert(bad_list == -os::EFAULT);
    int64_t bad_list_buf = os::syscall(os::SYS_LIST, path_addr, bad_addr, list_buf.size());
    assert(bad_list_buf == -os::EFAULT);

    uint64_t stat_addr = proc.vmem->allocate(sizeof(os::rse_stat));
    assert(stat_addr != 0);
    int64_t bad_stat = os::syscall(os::SYS_STAT, bad_addr, stat_addr);
    assert(bad_stat == -os::EFAULT);
    int64_t bad_mkdir = os::syscall(os::SYS_MKDIR, bad_addr, 0755);
    assert(bad_mkdir == -os::EFAULT);

    int64_t bad_pipe = os::syscall(os::SYS_PIPE, bad_addr);
    assert(bad_pipe == -os::EFAULT);

    int64_t bad_time = os::syscall(os::SYS_TIME, bad_addr);
    assert(bad_time == -os::EFAULT);

    os::rse_timespec req = { 0, 1000 };
    uint64_t req_addr = proc.vmem->allocate(sizeof(req));
    assert(req_addr != 0);
    assert(proc.vmem->writeUser(req_addr, &req, sizeof(req)));
    int64_t bad_nanosleep_req = os::syscall(os::SYS_NANOSLEEP, bad_addr, 0);
    assert(bad_nanosleep_req == -os::EFAULT);
    int64_t bad_nanosleep_rem = os::syscall(os::SYS_NANOSLEEP, req_addr, bad_addr);
    assert(bad_nanosleep_rem == -os::EFAULT);

    os::PageTable* pt = proc.vmem->getPageTable();
    assert(pt != nullptr);
    uint64_t no_user = proc.vmem->getHeapEnd() - os::PAGE_SIZE * 4;
    no_user = os::align_down(no_user);
    while (pt->isMapped(no_user)) {
        no_user -= os::PAGE_SIZE;
    }
    assert(proc.vmem->isUserRange(no_user, os::PAGE_SIZE));
    uint64_t phys_addr = phys_alloc.allocateFrame();
    assert(phys_addr != 0);
    bool mapped = pt->map(no_user, phys_addr, os::PTE_PRESENT | os::PTE_WRITABLE);
    assert(mapped);

    uint8_t tmp = 0;
    bool read_ok = proc.vmem->readUser(&tmp, no_user, sizeof(tmp));
    assert(!read_ok);
    bool write_ok = proc.vmem->writeUser(no_user, &tmp, sizeof(tmp));
    assert(!write_ok);

    int64_t no_user_write = os::syscall(os::SYS_WRITE, fd, no_user, 1);
    assert(no_user_write == -os::EFAULT);
    int64_t no_user_read = os::syscall(os::SYS_READ, fd, no_user, 1);
    assert(no_user_read == -os::EFAULT);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
