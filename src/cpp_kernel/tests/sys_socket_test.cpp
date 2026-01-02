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

int main() {
    std::cout << "[sys_socket Tests]" << std::endl;

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

    int64_t server_fd = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP, os::RSE_SOCK_STREAM, 0);
    assert(server_fd >= 0);

    os::rse_sockaddr addr{};
    addr.family = os::RSE_AF_LOOP;
    addr.port = 4242;
    addr.addr = os::RSE_ADDR_LOOPBACK;
    uint64_t addr_ptr = write_user(proc, &addr, sizeof(addr));

    int64_t bind_rc = os::syscall(os::SYS_BIND, server_fd, addr_ptr, sizeof(addr));
    assert(bind_rc == 0);

    int64_t listen_rc = os::syscall(os::SYS_LISTEN, server_fd, 1);
    assert(listen_rc == 0);

    int64_t client_fd = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP, os::RSE_SOCK_STREAM, 0);
    assert(client_fd >= 0);

    int64_t connect_rc = os::syscall(os::SYS_CONNECT, client_fd, addr_ptr, sizeof(addr));
    assert(connect_rc == 0);

    uint32_t addrlen = sizeof(os::rse_sockaddr);
    uint64_t addrlen_ptr = write_user(proc, &addrlen, sizeof(addrlen));
    uint64_t peer_addr_ptr = proc.vmem->allocate(sizeof(os::rse_sockaddr));
    assert(peer_addr_ptr != 0);

    int64_t accept_fd = os::syscall(os::SYS_ACCEPT, server_fd, peer_addr_ptr, addrlen_ptr);
    assert(accept_fd >= 0);

    os::rse_sockaddr peer{};
    assert(proc.vmem->readUser(&peer, peer_addr_ptr, sizeof(peer)));
    assert(peer.family == os::RSE_AF_LOOP);
    assert(peer.port != 0);

    const char msg[] = "ping";
    uint64_t msg_ptr = write_user(proc, msg, sizeof(msg) - 1);
    int64_t wrote = os::syscall(os::SYS_WRITE, client_fd, msg_ptr, sizeof(msg) - 1);
    assert(wrote == static_cast<int64_t>(sizeof(msg) - 1));

    std::array<char, 8> recv_buf{};
    uint64_t recv_ptr = proc.vmem->allocate(recv_buf.size());
    assert(recv_ptr != 0);
    int64_t read_rc = os::syscall(os::SYS_READ, accept_fd, recv_ptr, sizeof(msg) - 1);
    assert(read_rc == static_cast<int64_t>(sizeof(msg) - 1));
    assert(proc.vmem->readUser(recv_buf.data(), recv_ptr, recv_buf.size()));
    assert(std::memcmp(recv_buf.data(), msg, sizeof(msg) - 1) == 0);

    const char reply[] = "pong";
    uint64_t reply_ptr = write_user(proc, reply, sizeof(reply) - 1);
    int64_t reply_wrote = os::syscall(os::SYS_WRITE, accept_fd, reply_ptr, sizeof(reply) - 1);
    assert(reply_wrote == static_cast<int64_t>(sizeof(reply) - 1));

    std::array<char, 8> reply_buf{};
    uint64_t reply_buf_ptr = proc.vmem->allocate(reply_buf.size());
    assert(reply_buf_ptr != 0);
    int64_t reply_read = os::syscall(os::SYS_READ, client_fd, reply_buf_ptr, sizeof(reply) - 1);
    assert(reply_read == static_cast<int64_t>(sizeof(reply) - 1));
    assert(proc.vmem->readUser(reply_buf.data(), reply_buf_ptr, reply_buf.size()));
    assert(std::memcmp(reply_buf.data(), reply, sizeof(reply) - 1) == 0);

    int64_t dup_bind_fd = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP, os::RSE_SOCK_STREAM, 0);
    assert(dup_bind_fd >= 0);
    int64_t bind_again = os::syscall(os::SYS_BIND, dup_bind_fd, addr_ptr, sizeof(addr));
    assert(bind_again == -os::EADDRINUSE);

    os::rse_sockaddr missing{};
    missing.family = os::RSE_AF_LOOP;
    missing.port = 5151;
    missing.addr = os::RSE_ADDR_LOOPBACK;
    uint64_t missing_ptr = write_user(proc, &missing, sizeof(missing));
    int64_t no_listener_fd = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP, os::RSE_SOCK_STREAM, 0);
    assert(no_listener_fd >= 0);
    int64_t refused = os::syscall(os::SYS_CONNECT, no_listener_fd, missing_ptr, sizeof(missing));
    assert(refused == -os::ECONNREFUSED);

    int64_t second_client = os::syscall(os::SYS_SOCKET, os::RSE_AF_LOOP, os::RSE_SOCK_STREAM, 0);
    assert(second_client >= 0);
    int64_t second_connect = os::syscall(os::SYS_CONNECT, second_client, addr_ptr, sizeof(addr));
    assert(second_connect == 0);

    uint64_t bad_addr = proc.vmem->getStackEnd() + os::PAGE_SIZE;
    int64_t bad_accept = os::syscall(os::SYS_ACCEPT, server_fd, bad_addr, addrlen_ptr);
    assert(bad_accept == -os::EFAULT);

    int64_t accept_fd2 = os::syscall(os::SYS_ACCEPT, server_fd, peer_addr_ptr, addrlen_ptr);
    assert(accept_fd2 >= 0);

    int64_t bad_connect = os::syscall(os::SYS_CONNECT, second_client, bad_addr, sizeof(os::rse_sockaddr));
    assert(bad_connect == -os::EFAULT);

    int64_t bad_bind = os::syscall(os::SYS_BIND, dup_bind_fd, bad_addr, sizeof(os::rse_sockaddr));
    assert(bad_bind == -os::EFAULT);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
