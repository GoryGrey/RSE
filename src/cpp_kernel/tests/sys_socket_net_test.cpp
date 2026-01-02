#include "../os/SyscallDispatcher.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
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

static uint64_t write_user(os::OSProcess& proc, const void* data, size_t size) {
    uint64_t addr = proc.vmem->allocate(size);
    assert(addr != 0);
    bool ok = proc.vmem->writeUser(addr, data, size);
    assert(ok);
    return addr;
}

static void write_exact(int64_t fd, uint64_t buf, size_t size) {
    for (int attempt = 0; attempt < 16; ++attempt) {
        int64_t wrote = os::syscall(os::SYS_WRITE, fd, buf, size);
        if (wrote == static_cast<int64_t>(size)) {
            return;
        }
        if (wrote == -os::EAGAIN) {
            continue;
        }
        assert(false);
    }
    assert(false);
}

static void read_exact(int64_t fd, uint64_t buf, size_t size) {
    size_t remaining = size;
    uint64_t cursor = buf;
    for (int attempt = 0; attempt < 32 && remaining > 0; ++attempt) {
        int64_t got = os::syscall(os::SYS_READ, fd, cursor, remaining);
        if (got == -os::EAGAIN) {
            continue;
        }
        assert(got >= 0);
        remaining -= static_cast<size_t>(got);
        cursor += static_cast<uint64_t>(got);
    }
    assert(remaining == 0);
}

int main() {
    std::cout << "[sys_socket_net Tests]" << std::endl;

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

    int64_t server_fd = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP,
                                    os::RSE_SOCK_STREAM, os::RSE_PROTO_NET);
    assert(server_fd >= 0);

    os::rse_sockaddr addr{};
    addr.family = os::RSE_AF_LOOP;
    addr.port = 5050;
    uint64_t addr_ptr = write_user(proc, &addr, sizeof(addr));

    int64_t bind_rc = os::syscall(os::SYS_BIND, server_fd, addr_ptr, sizeof(addr));
    assert(bind_rc == 0);
    int64_t listen_rc = os::syscall(os::SYS_LISTEN, server_fd, 1);
    assert(listen_rc == 0);

    int64_t client_fd = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP,
                                    os::RSE_SOCK_STREAM, os::RSE_PROTO_NET);
    assert(client_fd >= 0);

    int64_t connect_rc = os::syscall(os::SYS_CONNECT, client_fd, addr_ptr, sizeof(addr));
    assert(connect_rc == 0 || connect_rc == -os::EAGAIN);

    uint32_t addrlen = sizeof(os::rse_sockaddr);
    uint64_t addrlen_ptr = write_user(proc, &addrlen, sizeof(addrlen));
    uint64_t peer_addr_ptr = proc.vmem->allocate(sizeof(os::rse_sockaddr));
    assert(peer_addr_ptr != 0);

    int64_t accept_fd = -1;
    for (int attempt = 0; attempt < 8; ++attempt) {
        accept_fd = os::syscall(os::SYS_ACCEPT, server_fd, peer_addr_ptr, addrlen_ptr);
        if (accept_fd >= 0) {
            break;
        }
        assert(accept_fd == -os::EAGAIN);
        int64_t retry = os::syscall(os::SYS_CONNECT, client_fd, addr_ptr, sizeof(addr));
        if (retry == 0 || retry == -EISCONN) {
            // OK
        } else {
            assert(retry == -os::EAGAIN);
        }
    }
    assert(accept_fd >= 0);

    const char msg[] = "netping";
    uint64_t msg_ptr = write_user(proc, msg, sizeof(msg) - 1);
    int64_t wrote = os::syscall(os::SYS_WRITE, client_fd, msg_ptr, sizeof(msg) - 1);
    assert(wrote == static_cast<int64_t>(sizeof(msg) - 1));

    std::array<char, 16> recv_buf{};
    uint64_t recv_ptr = proc.vmem->allocate(recv_buf.size());
    assert(recv_ptr != 0);
    int64_t read_rc = os::syscall(os::SYS_READ, accept_fd, recv_ptr, sizeof(msg) - 1);
    assert(read_rc == static_cast<int64_t>(sizeof(msg) - 1));
    assert(proc.vmem->readUser(recv_buf.data(), recv_ptr, recv_buf.size()));
    assert(std::memcmp(recv_buf.data(), msg, sizeof(msg) - 1) == 0);

    const char reply[] = "netpong";
    uint64_t reply_ptr = write_user(proc, reply, sizeof(reply) - 1);
    int64_t reply_wrote = os::syscall(os::SYS_WRITE, accept_fd, reply_ptr, sizeof(reply) - 1);
    assert(reply_wrote == static_cast<int64_t>(sizeof(reply) - 1));

    std::array<char, 16> reply_buf{};
    uint64_t reply_buf_ptr = proc.vmem->allocate(reply_buf.size());
    assert(reply_buf_ptr != 0);
    int64_t reply_read = os::syscall(os::SYS_READ, client_fd, reply_buf_ptr, sizeof(reply) - 1);
    assert(reply_read == static_cast<int64_t>(sizeof(reply) - 1));
    assert(proc.vmem->readUser(reply_buf.data(), reply_buf_ptr, reply_buf.size()));
    assert(std::memcmp(reply_buf.data(), reply, sizeof(reply) - 1) == 0);

    static constexpr size_t kBulkSize = 256;
    std::array<uint8_t, kBulkSize> bulk{};
    std::array<uint8_t, kBulkSize> bulk_out{};
    uint64_t bulk_send_ptr = proc.vmem->allocate(kBulkSize);
    uint64_t bulk_recv_ptr = proc.vmem->allocate(kBulkSize);
    assert(bulk_send_ptr != 0);
    assert(bulk_recv_ptr != 0);

    for (uint8_t iter = 0; iter < 16; ++iter) {
        for (size_t i = 0; i < bulk.size(); ++i) {
            bulk[i] = static_cast<uint8_t>(iter + i);
        }
        assert(proc.vmem->writeUser(bulk_send_ptr, bulk.data(), bulk.size()));
        write_exact(client_fd, bulk_send_ptr, bulk.size());
        read_exact(accept_fd, bulk_recv_ptr, bulk.size());
        assert(proc.vmem->readUser(bulk_out.data(), bulk_recv_ptr, bulk_out.size()));
        assert(std::memcmp(bulk_out.data(), bulk.data(), bulk.size()) == 0);

        bulk[0] ^= 0x5a;
        assert(proc.vmem->writeUser(bulk_send_ptr, bulk.data(), bulk.size()));
        write_exact(accept_fd, bulk_send_ptr, bulk.size());
        read_exact(client_fd, bulk_recv_ptr, bulk.size());
        assert(proc.vmem->readUser(bulk_out.data(), bulk_recv_ptr, bulk_out.size()));
        assert(std::memcmp(bulk_out.data(), bulk.data(), bulk.size()) == 0);
    }
    assert(os::net_wire_state().drops == 0);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
