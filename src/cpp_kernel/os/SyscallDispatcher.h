#pragma once

#include "Syscall.h"
#include "OSProcess.h"
#include "TorusScheduler.h"
#include "VFS.h"
#include "PhysicalAllocator.h"
#include "LoopbackDevice.h"
#include "SocketLite.h"
#include <cstring>
#ifdef RSE_KERNEL
#include "KernelStubs.h"
#else
#include <iostream>
#endif

/**
 * SyscallDispatcher: Routes system calls to appropriate handlers.
 * 
 * Each torus has its own dispatcher - no global syscall handler.
 */

namespace os {

// Forward declarations
class SyscallDispatcher;

// Global state (per-torus)
struct TorusContext {
    TorusScheduler* scheduler;
    SyscallDispatcher* dispatcher;
    VFS* vfs;
    PhysicalAllocator* phys_alloc;
    uint32_t next_pid;
    
    TorusContext() : scheduler(nullptr), dispatcher(nullptr), vfs(nullptr),
                     phys_alloc(nullptr), next_pid(1) {}
};

// Thread-local torus context (simulated for now)
extern TorusContext* current_torus_context;

// Helper functions
inline OSProcess* get_current_process() {
    if (!current_torus_context || !current_torus_context->scheduler) {
        return nullptr;
    }
    return current_torus_context->scheduler->getCurrentProcess();
}

inline TorusScheduler* get_current_scheduler() {
    if (!current_torus_context) {
        return nullptr;
    }
    return current_torus_context->scheduler;
}

inline uint32_t allocate_pid() {
    if (!current_torus_context) {
        return 0;
    }
    return current_torus_context->next_pid++;
}

inline bool enforce_user_memory(OSProcess* proc) {
    return proc && proc->vmem && !proc->user_step;
}

inline bool validate_user_range(OSProcess* proc, uint64_t addr, uint64_t size, bool write) {
    if (!enforce_user_memory(proc)) {
        return true;
    }
    return proc->vmem->validateUserRange(addr, size, write);
}

inline bool read_user_bytes(OSProcess* proc, uint64_t addr, void* dst, uint64_t size) {
    if (!dst || size == 0) {
        return false;
    }
    if (!enforce_user_memory(proc)) {
        if (!addr) {
            return false;
        }
        const uint8_t* src = reinterpret_cast<const uint8_t*>(addr);
        uint8_t* out = static_cast<uint8_t*>(dst);
        for (uint64_t i = 0; i < size; ++i) {
            out[i] = src[i];
        }
        return true;
    }
    return proc->vmem->readUser(dst, addr, size);
}

inline bool write_user_bytes(OSProcess* proc, uint64_t addr, const void* src, uint64_t size) {
    if (!src || size == 0) {
        return false;
    }
    if (!enforce_user_memory(proc)) {
        if (!addr) {
            return false;
        }
        uint8_t* dst = reinterpret_cast<uint8_t*>(addr);
        const uint8_t* in = static_cast<const uint8_t*>(src);
        for (uint64_t i = 0; i < size; ++i) {
            dst[i] = in[i];
        }
        return true;
    }
    return proc->vmem->writeUser(addr, src, size);
}

inline bool read_user_u32(OSProcess* proc, uint64_t addr, uint32_t* out) {
    if (!out) {
        return false;
    }
    if (!validate_user_range(proc, addr, sizeof(uint32_t), false)) {
        return false;
    }
    return read_user_bytes(proc, addr, out, sizeof(uint32_t));
}

inline bool write_user_u32(OSProcess* proc, uint64_t addr, uint32_t value) {
    if (!validate_user_range(proc, addr, sizeof(uint32_t), true)) {
        return false;
    }
    return write_user_bytes(proc, addr, &value, sizeof(uint32_t));
}

inline bool copy_user_string(OSProcess* proc, uint64_t addr, char* dst,
                             uint32_t cap, uint32_t* out_len) {
    if (!dst || cap == 0 || addr == 0) {
        return false;
    }
    uint32_t idx = 0;
    char c = '\0';
    do {
        if (idx + 1 >= cap) {
            return false;
        }
        if (!read_user_bytes(proc, addr + idx, &c, 1)) {
            return false;
        }
        dst[idx++] = c;
    } while (c != '\0');
    if (out_len) {
        *out_len = idx;
    }
    return true;
}

inline bool require_absolute_path(OSProcess* proc, const char* path) {
    if (!enforce_user_memory(proc)) {
        return true;
    }
    return path && path[0] == '/';
}

inline bool persist_prefix(const char* path) {
    const char* prefix = "/persist";
    uint32_t i = 0;
    if (!path) {
        return false;
    }
    for (; prefix[i] != '\0'; ++i) {
        if (path[i] != prefix[i]) {
            return false;
        }
    }
    return path[i] == '\0' || path[i] == '/';
}

inline bool persist_root(const char* path) {
    if (!persist_prefix(path)) {
        return false;
    }
    const uint32_t len = 8;
    return path[len] == '\0' || (path[len] == '/' && path[len + 1] == '\0');
}

inline bool persist_path(const char* path) {
    if (!persist_prefix(path)) {
        return false;
    }
    const uint32_t len = 8;
    if (path[len] != '/' || path[len + 1] == '\0') {
        return false;
    }
    uint32_t used = 0;
    bool in_segment = false;
    for (const char* p = path + len + 1; *p; ++p) {
        if (*p == '\\') {
            return false;
        }
        if (*p == '/') {
            if (!in_segment) {
                return false;
            }
            in_segment = false;
        } else {
            in_segment = true;
        }
        if (++used > BlockFS::kNameMax) {
            return false;
        }
    }
    return in_segment;
}

inline bool validate_user_path(OSProcess* proc, const char* path, bool allow_persist_root) {
    if (!enforce_user_memory(proc)) {
        return true;
    }
    if (!require_absolute_path(proc, path)) {
        return false;
    }
    if (!persist_prefix(path)) {
        return true;
    }
    if (persist_root(path)) {
        return allow_persist_root;
    }
    return persist_path(path);
}

inline SocketLite* fd_to_socket(OSProcess* proc, int32_t fd) {
    if (!proc) {
        return nullptr;
    }
    FileDescriptor* desc = proc->fd_table.get(fd);
    if (!desc || !desc->isDevice() || !desc->device) {
        return nullptr;
    }
    if (!is_socket_device(desc->device)) {
        return nullptr;
    }
    return static_cast<SocketLite*>(desc->device->private_data);
}

struct ExecStringTable {
    static constexpr uint32_t kMaxPtrs = 32;
    static constexpr uint32_t kStorageBytes = 4096;
    const char* ptrs[kMaxPtrs + 1];
    char storage[kStorageBytes];
    uint32_t count;
    uint32_t used;
};

inline bool collect_exec_strings(OSProcess* proc, uint64_t list_ptr,
                                 ExecStringTable* out) {
    if (!out) {
        return false;
    }
    out->count = 0;
    out->used = 0;
    for (uint32_t i = 0; i <= ExecStringTable::kMaxPtrs; ++i) {
        out->ptrs[i] = nullptr;
    }
    if (list_ptr == 0) {
        return true;
    }
    for (uint32_t i = 0; i < ExecStringTable::kMaxPtrs; ++i) {
        uint64_t str_ptr = 0;
        if (!read_user_bytes(proc, list_ptr + i * sizeof(uint64_t),
                             &str_ptr, sizeof(str_ptr))) {
            return false;
        }
        if (str_ptr == 0) {
            out->count = i;
            return true;
        }
        if (out->used >= ExecStringTable::kStorageBytes) {
            return false;
        }
        uint32_t len = 0;
        if (!copy_user_string(proc, str_ptr, out->storage + out->used,
                              ExecStringTable::kStorageBytes - out->used, &len)) {
            return false;
        }
        out->ptrs[i] = out->storage + out->used;
        out->used += len;
    }
    return false;
}

static constexpr uint64_t kNanosPerSecond = 1000000000ull;
static constexpr uint64_t kTickNanos = 1000000ull;
static constexpr uint64_t kTicksPerSecond = kNanosPerSecond / kTickNanos;

// ========== System Call Handlers ==========

/**
 * sys_getpid: Get current process ID
 */
inline int64_t sys_getpid(uint64_t, uint64_t, uint64_t, 
                          uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    return current->pid;
}

/**
 * sys_getppid: Get parent process ID
 */
inline int64_t sys_getppid(uint64_t, uint64_t, uint64_t,
                           uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    return current->parent_pid;
}

/**
 * sys_torus_id: Get current torus ID
 */
inline int64_t sys_torus_id(uint64_t, uint64_t, uint64_t,
                            uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    return current->torus_id;
}

/**
 * sys_exit: Terminate current process
 */
inline int64_t sys_exit(uint64_t status, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    
    // Mark process as zombie
    current->setZombie(status);
    
    std::cout << "[sys_exit] Process " << current->pid 
              << " exited with status " << status << std::endl;
    
    // Scheduler will clean up on next tick
    return 0;
}

/**
 * sys_fork: Create child process
 */
inline int64_t sys_fork(uint64_t, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* parent = get_current_process();
    if (!parent) {
        return -ESRCH;
    }
    
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }
    
    // Allocate new PID
    uint32_t child_pid = allocate_pid();
    
    // Create child process
    OSProcess* child = new OSProcess(child_pid, parent->pid, parent->torus_id);
    child->setKernelOwned(true);
    
    // Copy parent's context
    child->context = parent->context;
    child->memory = parent->memory;
    child->priority = parent->priority;
    
    // Copy file descriptors (per-process table)
    child->fd_table = parent->fd_table;
    
    // Copy spatial position
    child->x = parent->x;
    child->y = parent->y;
    child->z = parent->z;

    if (parent->vmem) {
        VirtualAllocator* cloned = parent->vmem->clone();
        if (!cloned) {
            delete child;
            return -ENOMEM;
        }
        child->vmem = cloned;
        child->memory.page_table = cloned->getPageTable();
        child->memory.heap_start = cloned->getHeapStart();
        child->memory.heap_end = cloned->getHeapEnd();
        child->memory.heap_brk = cloned->getHeapBrk();
    } else if (current_torus_context && current_torus_context->phys_alloc) {
        child->initMemory(current_torus_context->phys_alloc);
    }
    
    // Add child to scheduler
    if (!scheduler->addProcess(child)) {
        delete child;
        return -ENOMEM;
    }
    
    std::cout << "[sys_fork] Process " << parent->pid 
              << " forked child " << child_pid << std::endl;
    
    // Return child PID to parent
    // (In real implementation, child would see 0 when it runs)
    return child_pid;
}

/**
 * sys_wait: Wait for child process to exit
 */
inline int64_t sys_wait(uint64_t status_ptr, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }

    int exit_code = 0;
    OSProcess* zombie = scheduler->reapZombie(current->pid, &exit_code);
    if (!zombie) {
        bool has_child = false;
        scheduler->forEachProcess([&](OSProcess* proc) {
            if (proc && proc->parent_pid == current->pid) {
                has_child = true;
            }
        });
        return has_child ? -EAGAIN : -ECHILD;
    }

    if (status_ptr != 0) {
        if (!validate_user_range(current, status_ptr, sizeof(int), true)) {
            (void)scheduler->pushZombie(zombie);
            return -EFAULT;
        }
        if (!write_user_bytes(current, status_ptr, &exit_code, sizeof(exit_code))) {
            (void)scheduler->pushZombie(zombie);
            return -EFAULT;
        }
    }

    int64_t pid = static_cast<int64_t>(zombie->pid);
    if (zombie->isKernelOwned()) {
        delete zombie;
    }
    return pid;
}

/**
 * sys_kill: Send signal to process
 */
inline int64_t sys_kill(uint64_t pid, uint64_t sig, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }
    if (pid == 0) {
        return -EINVAL;
    }
    if (current->pid == pid && sig == 0) {
        return 0;
    }
    int exit_code = 128 + static_cast<int>(sig & 0x7F);
    if (!scheduler->killProcess(static_cast<uint32_t>(pid), exit_code)) {
        return -ESRCH;
    }
    return 0;
}

inline int64_t sys_pipe(uint64_t fds_addr, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (!fds_addr) {
        return -EINVAL;
    }
    if (!validate_user_range(current, fds_addr, sizeof(int) * 2, true)) {
        return -EFAULT;
    }

    Device* dev = create_loopback_device("pipe");
    if (!dev) {
        return -ENOMEM;
    }
    if (dev->open) {
        dev->open(dev);
    }

    int32_t read_fd = current->fd_table.allocateDevice(dev, O_RDONLY);
    if (read_fd < 0) {
        delete static_cast<LoopbackData*>(dev->private_data);
        delete dev;
        return -ENOMEM;
    }
    int32_t write_fd = current->fd_table.allocateDevice(dev, O_WRONLY);
    if (write_fd < 0) {
        current->fd_table.free(read_fd);
        delete static_cast<LoopbackData*>(dev->private_data);
        delete dev;
        return -ENOMEM;
    }

    int fds[2] = { read_fd, write_fd };
    if (!write_user_bytes(current, fds_addr, fds, sizeof(fds))) {
        current->fd_table.free(read_fd);
        current->fd_table.free(write_fd);
        delete static_cast<LoopbackData*>(dev->private_data);
        delete dev;
        return -EFAULT;
    }
    return 0;
}

inline int64_t sys_socket(uint64_t domain, uint64_t type, uint64_t protocol,
                          uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (domain != RSE_AF_LOOP) {
        return -EOPNOTSUPP;
    }
    if (type != 0 && type != RSE_SOCK_STREAM) {
        return -EOPNOTSUPP;
    }
    SocketLite::Backend backend = SocketLite::Backend::LOOPBACK;
    if (protocol == RSE_PROTO_NET) {
        backend = SocketLite::Backend::NET_LITE;
        socket_manager().ensure_net_online();
        if (!socket_manager().net_online()) {
            return -EIO;
        }
    } else if (protocol != 0) {
        return -EOPNOTSUPP;
    }
    SocketLite* sock = socket_manager().allocate(backend);
    if (!sock) {
        return -ENOMEM;
    }
    Device* dev = create_socket_device(sock);
    if (!dev) {
        socket_manager().release(sock);
        return -ENOMEM;
    }
    int32_t fd = current->fd_table.allocateDevice(dev, O_RDWR);
    if (fd < 0) {
        socket_manager().release(sock);
        return -1;
    }
    return fd;
}

inline int64_t sys_bind(uint64_t fd, uint64_t addr_ptr, uint64_t addr_len,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (addr_len < sizeof(rse_sockaddr)) {
        return -EINVAL;
    }
    if (!validate_user_range(current, addr_ptr, sizeof(rse_sockaddr), false)) {
        return -EFAULT;
    }
    rse_sockaddr addr{};
    if (!read_user_bytes(current, addr_ptr, &addr, sizeof(addr))) {
        return -EFAULT;
    }
    if (addr.family != RSE_AF_LOOP) {
        return -EOPNOTSUPP;
    }
    SocketLite* sock = fd_to_socket(current, static_cast<int32_t>(fd));
    if (!sock) {
        return -ENOTSOCK;
    }
    if (sock->state != SocketLite::State::CREATED) {
        return -EINVAL;
    }
    uint16_t port = addr.port;
    if (port == 0) {
        port = socket_manager().allocate_ephemeral_port(sock->backend);
        if (port == 0) {
            return -EAGAIN;
        }
    }
    if (socket_manager().port_in_use(port, sock->backend)) {
        return -EADDRINUSE;
    }
    sock->port = port;
    sock->state = SocketLite::State::BOUND;
    return 0;
}

inline int64_t sys_listen(uint64_t fd, uint64_t backlog, uint64_t,
                          uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    (void)backlog;
    SocketLite* sock = fd_to_socket(current, static_cast<int32_t>(fd));
    if (!sock) {
        return -ENOTSOCK;
    }
    if (sock->state != SocketLite::State::BOUND || sock->port == 0) {
        return -EINVAL;
    }
    sock->state = SocketLite::State::LISTENING;
    sock->pending = nullptr;
    return 0;
}

inline int64_t sys_accept(uint64_t fd, uint64_t addr_ptr, uint64_t addrlen_ptr,
                          uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    SocketLite* listener = fd_to_socket(current, static_cast<int32_t>(fd));
    if (!listener) {
        return -ENOTSOCK;
    }
    if (listener->state != SocketLite::State::LISTENING) {
        return -EINVAL;
    }
    if (listener->backend == SocketLite::Backend::NET_LITE) {
        socket_poll_net();
    }
    SocketLite* server_sock = listener->pending;
    if (!server_sock) {
        return -EAGAIN;
    }

    if (addr_ptr != 0) {
        if (addrlen_ptr == 0) {
            return -EINVAL;
        }
        uint32_t max_len = 0;
        if (!read_user_u32(current, addrlen_ptr, &max_len)) {
            return -EFAULT;
        }
        if (max_len < sizeof(rse_sockaddr)) {
            return -EINVAL;
        }
        if (!validate_user_range(current, addr_ptr, sizeof(rse_sockaddr), true)) {
            return -EFAULT;
        }
    } else if (addrlen_ptr != 0) {
        if (!validate_user_range(current, addrlen_ptr, sizeof(uint32_t), true)) {
            return -EFAULT;
        }
    }

    Device* dev = create_socket_device(server_sock);
    if (!dev) {
        socket_manager().release(server_sock);
        return -ENOMEM;
    }
    int32_t new_fd = current->fd_table.allocateDevice(dev, O_RDWR);
    if (new_fd < 0) {
        socket_manager().release(server_sock);
        return -1;
    }

    listener->pending = nullptr;

    if (addr_ptr != 0) {
        rse_sockaddr out{};
        out.family = RSE_AF_LOOP;
        out.port = server_sock->peer_port;
        if (!write_user_bytes(current, addr_ptr, &out, sizeof(out))) {
            return -EFAULT;
        }
        if (!write_user_u32(current, addrlen_ptr, sizeof(rse_sockaddr))) {
            return -EFAULT;
        }
    } else if (addrlen_ptr != 0) {
        if (!write_user_u32(current, addrlen_ptr, sizeof(rse_sockaddr))) {
            return -EFAULT;
        }
    }

    return new_fd;
}

inline int64_t sys_connect(uint64_t fd, uint64_t addr_ptr, uint64_t addr_len,
                           uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (addr_len < sizeof(rse_sockaddr)) {
        return -EINVAL;
    }
    if (!validate_user_range(current, addr_ptr, sizeof(rse_sockaddr), false)) {
        return -EFAULT;
    }
    rse_sockaddr addr{};
    if (!read_user_bytes(current, addr_ptr, &addr, sizeof(addr))) {
        return -EFAULT;
    }
    if (addr.family != RSE_AF_LOOP) {
        return -EOPNOTSUPP;
    }
    if (addr.port == 0) {
        return -EINVAL;
    }
    SocketLite* sock = fd_to_socket(current, static_cast<int32_t>(fd));
    if (!sock) {
        return -ENOTSOCK;
    }
    if (sock->state == SocketLite::State::CONNECTED) {
        return -EISCONN;
    }
    if (sock->state == SocketLite::State::CONNECTING &&
        sock->backend == SocketLite::Backend::NET_LITE) {
        socket_poll_net();
        return sock->state == SocketLite::State::CONNECTED ? 0 : -EAGAIN;
    }
    if (sock->state == SocketLite::State::LISTENING) {
        return -EOPNOTSUPP;
    }
    if (sock->state != SocketLite::State::CREATED &&
        sock->state != SocketLite::State::BOUND) {
        return -EINVAL;
    }
    if (sock->backend == SocketLite::Backend::NET_LITE) {
        if (sock->port == 0) {
            uint16_t port = socket_manager().allocate_ephemeral_port(sock->backend);
            if (port == 0) {
                return -EAGAIN;
            }
            sock->port = port;
        }
        if (sock->conn_id == 0) {
            sock->conn_id = socket_manager().allocate_conn_id();
            if (sock->conn_id == 0) {
                return -EAGAIN;
            }
        }
        TcpLiteSynPayload syn{ addr.port, sock->port };
        int rc = net_send_frame(sock->conn_id, kTcpLiteSyn,
                                &syn, sizeof(syn));
        if (rc < 0) {
            return rc;
        }
        sock->peer_port = addr.port;
        sock->state = SocketLite::State::CONNECTING;
        socket_poll_net();
        return sock->state == SocketLite::State::CONNECTED ? 0 : -EAGAIN;
    }

    SocketLite* listener = socket_manager().find_listener(addr.port, sock->backend);
    if (!listener) {
        return -ECONNREFUSED;
    }
    if (listener->pending) {
        return -EAGAIN;
    }
    if (sock->port == 0) {
        uint16_t port = socket_manager().allocate_ephemeral_port(sock->backend);
        if (port == 0) {
            return -EAGAIN;
        }
        sock->port = port;
    }
    SocketLite* server_sock = socket_manager().allocate(sock->backend);
    if (!server_sock) {
        return -ENOMEM;
    }
    server_sock->state = SocketLite::State::CONNECTED;
    server_sock->port = listener->port;
    server_sock->peer_port = sock->port;
    server_sock->peer = sock;
    server_sock->pending = nullptr;

    sock->state = SocketLite::State::CONNECTED;
    sock->peer_port = listener->port;
    sock->peer = server_sock;

    listener->pending = server_sock;
    return 0;
}

inline int64_t sys_dup(uint64_t old_fd, uint64_t, uint64_t,
                       uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    int32_t new_fd = current->fd_table.duplicate(static_cast<int32_t>(old_fd));
    if (new_fd < 0) {
        return -EBADF;
    }
    return new_fd;
}

inline int64_t sys_dup2(uint64_t old_fd, uint64_t new_fd, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (!current->fd_table.get(static_cast<int32_t>(old_fd))) {
        return -EBADF;
    }
    int32_t duped = current->fd_table.duplicateTo(static_cast<int32_t>(old_fd),
                                                  static_cast<int32_t>(new_fd));
    if (duped < 0) {
        return -EINVAL;
    }
    return duped;
}

inline int64_t sys_time(uint64_t out_ptr, uint64_t, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }
    uint64_t ticks = scheduler->getTicks();
    uint64_t seconds = ticks / kTicksPerSecond;
    if (out_ptr != 0) {
        OSProcess* current = get_current_process();
        if (!current) {
            return -ESRCH;
        }
        if (!validate_user_range(current, out_ptr, sizeof(uint64_t), true)) {
            return -EFAULT;
        }
        if (!write_user_bytes(current, out_ptr, &seconds, sizeof(seconds))) {
            return -EFAULT;
        }
    }
    return static_cast<int64_t>(seconds);
}

inline int64_t sys_sleep(uint64_t seconds, uint64_t, uint64_t,
                         uint64_t, uint64_t, uint64_t) {
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }
    if (seconds == 0) {
        return 0;
    }
    if (seconds > (UINT64_MAX / kTicksPerSecond)) {
        return -EINVAL;
    }
    uint64_t ticks = seconds * kTicksPerSecond;
    if (ticks == 0) {
        ticks = 1;
    }
    return scheduler->sleepCurrent(ticks) ? 0 : -EAGAIN;
}

inline int64_t sys_nanosleep(uint64_t req_ptr, uint64_t rem_ptr, uint64_t,
                             uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }
    if (req_ptr == 0) {
        return -EINVAL;
    }
    if (!validate_user_range(current, req_ptr, sizeof(rse_timespec), false)) {
        return -EFAULT;
    }
    rse_timespec req = {};
    if (!read_user_bytes(current, req_ptr, &req, sizeof(req))) {
        return -EFAULT;
    }
    if (req.tv_nsec >= kNanosPerSecond) {
        req.tv_sec += req.tv_nsec / kNanosPerSecond;
        req.tv_nsec = req.tv_nsec % kNanosPerSecond;
    }
    if (req.tv_sec > (UINT64_MAX / kNanosPerSecond)) {
        return -EINVAL;
    }
    uint64_t nanos = req.tv_sec * kNanosPerSecond + req.tv_nsec;
    if (nanos == 0) {
        return 0;
    }
    uint64_t ticks = (nanos + kTickNanos - 1) / kTickNanos;
    if (ticks == 0) {
        ticks = 1;
    }
    if (rem_ptr != 0) {
        rse_timespec rem = {};
        if (!validate_user_range(current, rem_ptr, sizeof(rem), true)) {
            return -EFAULT;
        }
        if (!write_user_bytes(current, rem_ptr, &rem, sizeof(rem))) {
            return -EFAULT;
        }
    }
    return scheduler->sleepCurrent(ticks) ? 0 : -EAGAIN;
}

/**
 * sys_exec: Replace current process image with a new ELF binary.
 */
inline int64_t sys_exec(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp_ptr,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (!current_torus_context || !current_torus_context->vfs || !current_torus_context->phys_alloc) {
        return -ENOSYS;
    }
    FileDescriptorTable* fdt = &current->fd_table;

    static constexpr uint32_t kMaxPath = 256;
    char path_buf[kMaxPath] = {};
    if (!copy_user_string(current, path_ptr, path_buf, kMaxPath, nullptr)) {
        return -EFAULT;
    }
    if (!validate_user_path(current, path_buf, false)) {
        return -EINVAL;
    }

    ExecStringTable argv = {};
    ExecStringTable envp = {};
    if (!collect_exec_strings(current, argv_ptr, &argv)) {
        return -EFAULT;
    }
    if (!collect_exec_strings(current, envp_ptr, &envp)) {
        return -EFAULT;
    }

    int32_t fd = current_torus_context->vfs->open(fdt, path_buf, O_RDONLY);
    if (fd < 0) {
        return -ENOENT;
    }

    static constexpr uint32_t kChunk = 4096;
    static constexpr uint32_t kMaxElfSize = 512 * 1024;
#ifdef RSE_KERNEL
    static uint8_t image[kMaxElfSize];
    uint8_t* image_buf = image;
#else
    uint8_t* image_buf = new uint8_t[kMaxElfSize];
    if (!image_buf) {
        current_torus_context->vfs->close(fdt, fd);
        return -ENOMEM;
    }
#endif

    uint32_t total = 0;
    while (true) {
        if (total + kChunk > kMaxElfSize) {
            current_torus_context->vfs->close(fdt, fd);
#ifndef RSE_KERNEL
            delete[] image_buf;
#endif
            return -ENOMEM;
        }
        int64_t bytes = current_torus_context->vfs->read(fdt, fd, image_buf + total, kChunk);
        if (bytes < 0) {
            current_torus_context->vfs->close(fdt, fd);
#ifndef RSE_KERNEL
            delete[] image_buf;
#endif
            return -EIO;
        }
        if (bytes == 0) {
            break;
        }
        total += static_cast<uint32_t>(bytes);
    }
    current_torus_context->vfs->close(fdt, fd);

    if (total == 0) {
#ifndef RSE_KERNEL
        delete[] image_buf;
#endif
        return -EINVAL;
    }

    VirtualAllocator* old_vmem = current->vmem;
    MemoryLayout old_mem = current->memory;
    CPUContext old_ctx = current->context;

    PageTable* new_pt = new PageTable();
    if (!new_pt) {
#ifndef RSE_KERNEL
        delete[] image_buf;
#endif
        return -ENOMEM;
    }
    VirtualAllocator* new_va = new VirtualAllocator(new_pt, current_torus_context->phys_alloc);
    if (!new_va) {
        delete new_pt;
#ifndef RSE_KERNEL
        delete[] image_buf;
#endif
        return -ENOMEM;
    }
#ifdef RSE_KERNEL
    static constexpr uint64_t kKernelUserBase = 0x40000000ull;
    static constexpr uint64_t kKernelUserWindow = 0x200000ull;
    static constexpr uint64_t kKernelUserStackSize = 64 * 1024ull;
    static constexpr uint64_t kKernelUserStackTop = kKernelUserBase + kKernelUserWindow - PAGE_SIZE;
    static constexpr uint64_t kKernelUserStackBase = kKernelUserStackTop - kKernelUserStackSize;
    static constexpr uint64_t kKernelUserHeapBase = kKernelUserBase;
    static constexpr uint64_t kKernelUserHeapLimit = kKernelUserStackBase;
    new_va->setStackBounds(kKernelUserStackBase, kKernelUserStackTop);
    new_va->setHeapBounds(kKernelUserHeapBase, kKernelUserHeapLimit);
#endif

    current->vmem = new_va;
    current->memory = MemoryLayout();
    current->memory.page_table = new_pt;
    current->context = CPUContext();

#ifdef RSE_KERNEL
    const uint64_t stack_size = kKernelUserStackSize;
#else
    const uint64_t stack_size = 64 * 1024;
#endif
    if (!current->loadElfImageWithArgs(image_buf, total, argv.ptrs, envp.ptrs, stack_size)) {
        delete new_va;
        delete new_pt;
        current->vmem = old_vmem;
        current->memory = old_mem;
        current->context = old_ctx;
#ifndef RSE_KERNEL
        delete[] image_buf;
#endif
        return -EINVAL;
    }

    current->fd_table.closeOnExec();
    if (old_vmem) {
        delete old_vmem;
    }
    if (old_mem.page_table) {
        delete old_mem.page_table;
    }
    current->setUserEntry(nullptr, nullptr, nullptr);
#ifndef RSE_KERNEL
    delete[] image_buf;
#endif
    return 0;
}

/**
 * sys_write: Write to file descriptor
 */
inline int64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t count,
                         uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (count > static_cast<uint64_t>(UINT32_MAX)) {
        return -EINVAL;
    }
    if (count != 0 && !validate_user_range(current, buf_addr, count, false)) {
        return -EFAULT;
    }
    if (enforce_user_memory(current)) {
        static constexpr uint32_t kScratch = 256;
        uint8_t scratch[kScratch];
        uint64_t remaining = count;
        uint64_t addr = buf_addr;
        int64_t total = 0;
        while (remaining > 0) {
            uint32_t chunk = remaining > kScratch ? kScratch : static_cast<uint32_t>(remaining);
            if (!read_user_bytes(current, addr, scratch, chunk)) {
                return total != 0 ? total : -EFAULT;
            }
            int64_t written = current_torus_context->vfs->write(&current->fd_table,
                                                               static_cast<int32_t>(fd),
                                                               scratch,
                                                               chunk);
            if (written < 0) {
                return total != 0 ? total : written;
            }
            total += written;
            if (static_cast<uint32_t>(written) < chunk) {
                break;
            }
            addr += static_cast<uint64_t>(written);
            remaining -= static_cast<uint64_t>(written);
        }
        return total;
    }
    return current_torus_context->vfs->write(&current->fd_table,
                                             static_cast<int32_t>(fd),
                                             (const void *)buf_addr,
                                             static_cast<uint32_t>(count));
}

/**
 * sys_read: Read from file descriptor
 */
inline int64_t sys_read(uint64_t fd, uint64_t buf_addr, uint64_t count,
                        uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (count > static_cast<uint64_t>(UINT32_MAX)) {
        return -EINVAL;
    }
    if (count != 0 && !validate_user_range(current, buf_addr, count, true)) {
        return -EFAULT;
    }
    if (enforce_user_memory(current)) {
        static constexpr uint32_t kScratch = 256;
        uint8_t scratch[kScratch];
        uint64_t remaining = count;
        uint64_t addr = buf_addr;
        int64_t total = 0;
        while (remaining > 0) {
            uint32_t chunk = remaining > kScratch ? kScratch : static_cast<uint32_t>(remaining);
            int64_t got = current_torus_context->vfs->read(&current->fd_table,
                                                          static_cast<int32_t>(fd),
                                                          scratch,
                                                          chunk);
            if (got < 0) {
                return total != 0 ? total : got;
            }
            if (got == 0) {
                break;
            }
            if (!current->vmem || !current->vmem->writeUser(addr, scratch, static_cast<uint64_t>(got))) {
                return total != 0 ? total : -EFAULT;
            }
            total += got;
            addr += static_cast<uint64_t>(got);
            remaining -= static_cast<uint64_t>(got);
            if (static_cast<uint32_t>(got) < chunk) {
                break;
            }
        }
        return total;
    }
    return current_torus_context->vfs->read(&current->fd_table,
                                            static_cast<int32_t>(fd),
                                            (void *)buf_addr,
                                            static_cast<uint32_t>(count));
}

/**
 * sys_open: Open a file
 */
inline int64_t sys_open(uint64_t path_addr, uint64_t flags, uint64_t mode,
                        uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    static constexpr uint32_t kMaxPath = 256;
    char path_buf[kMaxPath] = {};
    if (!copy_user_string(current, path_addr, path_buf, kMaxPath, nullptr)) {
        return -EFAULT;
    }
    if (!validate_user_path(current, path_buf, false)) {
        return -EINVAL;
    }
    return current_torus_context->vfs->open(&current->fd_table, path_buf,
                                            static_cast<uint32_t>(flags),
                                            static_cast<uint32_t>(mode));
}

/**
 * sys_close: Close a file descriptor
 */
inline int64_t sys_close(uint64_t fd, uint64_t, uint64_t,
                         uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    return current_torus_context->vfs->close(&current->fd_table,
                                             static_cast<int32_t>(fd));
}

/**
 * sys_lseek: Seek within a file
 */
inline int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                         uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    return current_torus_context->vfs->lseek(&current->fd_table,
                                             static_cast<int32_t>(fd),
                                             static_cast<int64_t>(offset),
                                             static_cast<int>(whence));
}

/**
 * sys_unlink: Delete a file
 */
inline int64_t sys_unlink(uint64_t path_addr, uint64_t, uint64_t,
                          uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    static constexpr uint32_t kMaxPath = 256;
    char path_buf[kMaxPath] = {};
    if (!copy_user_string(current, path_addr, path_buf, kMaxPath, nullptr)) {
        return -EFAULT;
    }
    if (!validate_user_path(current, path_buf, false)) {
        return -EINVAL;
    }
    return current_torus_context->vfs->unlink(path_buf);
}

inline int64_t sys_list(uint64_t path_addr, uint64_t buf_addr, uint64_t count,
                        uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    const char* path = "/";
    static constexpr uint32_t kMaxPath = 256;
    char path_buf[kMaxPath] = {};
    if (path_addr != 0) {
        if (!copy_user_string(current, path_addr, path_buf, kMaxPath, nullptr)) {
            return -EFAULT;
        }
        if (!validate_user_path(current, path_buf, true)) {
            return -EINVAL;
        }
        path = path_buf;
    }
    char* buf = reinterpret_cast<char*>(buf_addr);
    if (!buf || count == 0) {
        return -EINVAL;
    }
    bool user_buf = enforce_user_memory(current);
    static constexpr uint32_t kMaxOut = 2048;
    uint32_t len = (count > UINT32_MAX) ? UINT32_MAX : (uint32_t)count;
    if (user_buf && len > kMaxOut) {
        len = kMaxOut;
    }
    if (!validate_user_range(current, buf_addr, len, true)) {
        return -EFAULT;
    }

    char local[kMaxOut];
    char* out = user_buf ? local : buf;
    int32_t got = current_torus_context->vfs->list(path ? path : "/", out, len);
    if (got < 0) {
        return got;
    }
    if (user_buf) {
        uint32_t copy_len = (uint32_t)got;
        if (copy_len < len) {
            copy_len += 1;
        }
        if (!write_user_bytes(current, buf_addr, out, copy_len)) {
            return -EFAULT;
        }
    }
    return got;
}

inline int64_t sys_mkdir(uint64_t path_addr, uint64_t mode, uint64_t,
                         uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    static constexpr uint32_t kMaxPath = 256;
    char path_buf[kMaxPath] = {};
    if (!copy_user_string(current, path_addr, path_buf, kMaxPath, nullptr)) {
        return -EFAULT;
    }
    if (!validate_user_path(current, path_buf, false)) {
        return -EINVAL;
    }
    return current_torus_context->vfs->mkdir(path_buf, static_cast<uint32_t>(mode));
}

inline int64_t sys_stat(uint64_t path_addr, uint64_t stat_addr, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (stat_addr == 0) {
        return -EINVAL;
    }
    static constexpr uint32_t kMaxPath = 256;
    char path_buf[kMaxPath] = {};
    if (!copy_user_string(current, path_addr, path_buf, kMaxPath, nullptr)) {
        return -EFAULT;
    }
    if (!validate_user_path(current, path_buf, true)) {
        return -EINVAL;
    }
    if (!validate_user_range(current, stat_addr, sizeof(rse_stat), true)) {
        return -EFAULT;
    }

    rse_stat info = {};
    int32_t rc = current_torus_context->vfs->stat(path_buf, &info);
    if (rc < 0) {
        return rc;
    }
    if (enforce_user_memory(current)) {
        if (!write_user_bytes(current, stat_addr, &info, sizeof(info))) {
            return -EFAULT;
        }
    } else {
        *reinterpret_cast<rse_stat*>(stat_addr) = info;
    }
    return 0;
}

inline int64_t sys_ps(uint64_t buf_addr, uint64_t count, uint64_t,
                      uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    TorusScheduler* scheduler = get_current_scheduler();
    if (!scheduler) {
        return -ESRCH;
    }
    if (!buf_addr || count == 0) {
        return -EINVAL;
    }
    if (!validate_user_range(current, buf_addr, count, true)) {
        return -EFAULT;
    }

    bool user_buf = enforce_user_memory(current);
    static constexpr uint32_t kMaxOut = 2048;
    uint32_t len = (count > UINT32_MAX) ? UINT32_MAX : (uint32_t)count;
    if (user_buf && len > kMaxOut) {
        len = kMaxOut;
    }
    char local[kMaxOut];
    char* buf = user_buf ? local : reinterpret_cast<char*>(buf_addr);
    uint32_t used = 0;

    auto append_char = [&](char c) -> bool {
        if (used + 1 >= len) {
            return false;
        }
        buf[used++] = c;
        return true;
    };
    auto append_str = [&](const char* s) -> bool {
        if (!s) {
            return true;
        }
        while (*s) {
            if (!append_char(*s++)) {
                return false;
            }
        }
        return true;
    };
    auto append_u64 = [&](uint64_t value) -> bool {
        char tmp[32];
        uint32_t idx = 0;
        if (value == 0) {
            tmp[idx++] = '0';
        } else {
            while (value && idx < sizeof(tmp)) {
                tmp[idx++] = (char)('0' + (value % 10));
                value /= 10;
            }
        }
        for (uint32_t i = 0; i < idx / 2; ++i) {
            char t = tmp[i];
            tmp[i] = tmp[idx - 1 - i];
            tmp[idx - 1 - i] = t;
        }
        for (uint32_t i = 0; i < idx; ++i) {
            if (!append_char(tmp[i])) {
                return false;
            }
        }
        return true;
    };
    auto state_str = [](ProcessState state) -> const char* {
        switch (state) {
            case ProcessState::READY: return "READY";
            case ProcessState::RUNNING: return "RUNNING";
            case ProcessState::BLOCKED: return "BLOCKED";
            case ProcessState::ZOMBIE: return "ZOMBIE";
        }
        return "UNKNOWN";
    };

    bool wrote = false;
    scheduler->forEachProcess([&](OSProcess* proc) {
        if (!proc) {
            return;
        }
        if (!append_str("pid=") || !append_u64(proc->pid) ||
            !append_str(" torus=") || !append_u64(proc->torus_id) ||
            !append_str(" state=") || !append_str(state_str(proc->state)) ||
            !append_str(" runtime=") || !append_u64(proc->total_runtime) ||
            !append_char('\n')) {
            return;
        }
        wrote = true;
    });

    if (!wrote) {
        append_str("ps: empty\n");
    }
    if (used < len) {
        buf[used] = '\0';
    }
    if (user_buf) {
        uint32_t copy_len = used;
        if (used < len) {
            copy_len = used + 1;
        }
        if (!write_user_bytes(current, buf_addr, buf, copy_len)) {
            return -EFAULT;
        }
    }
    return (int64_t)used;
}

/**
 * sys_brk: Change data segment size
 */
inline int64_t sys_brk(uint64_t addr, uint64_t, uint64_t,
                       uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    if (!current->vmem) {
        return -ENOSYS;
    }
    uint64_t new_brk = current->vmem->brk(addr);
    if (new_brk == 0) {
        return -ENOMEM;
    }
    current->memory.heap_brk = new_brk;
    return new_brk;
}

inline int64_t sys_mmap(uint64_t addr, uint64_t size, uint64_t prot,
                        uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current || !current->vmem) {
        return -ESRCH;
    }
    if (size == 0) {
        return -EINVAL;
    }
    if (addr != 0 && (addr & (PAGE_SIZE - 1)) != 0) {
        return -EINVAL;
    }
    if (addr > UINT64_MAX - size) {
        return -EINVAL;
    }
    if (prot & PROT_EXEC) {
        return -EACCES;
    }
    if ((prot & (PROT_EXEC | PROT_WRITE)) == (PROT_EXEC | PROT_WRITE)) {
        return -EACCES;
    }
    if (enforce_user_memory(current) && addr != 0 &&
        !current->vmem->isUserRange(addr, size)) {
        return -EFAULT;
    }
    uint64_t mapped = current->vmem->mmap(addr, size, prot);
    if (mapped == 0) {
        return -ENOMEM;
    }
    return mapped;
}

inline int64_t sys_munmap(uint64_t addr, uint64_t size, uint64_t,
                          uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current || !current->vmem) {
        return -ESRCH;
    }
    if (size == 0) {
        return -EINVAL;
    }
    if ((addr & (PAGE_SIZE - 1)) != 0 || (size & (PAGE_SIZE - 1)) != 0) {
        return -EINVAL;
    }
    if (addr > UINT64_MAX - size) {
        return -EINVAL;
    }
    if (enforce_user_memory(current) &&
        !current->vmem->isUserRange(addr, size)) {
        return -EFAULT;
    }
    current->vmem->munmap(addr, size);
    return 0;
}

inline int64_t sys_mprotect(uint64_t addr, uint64_t size, uint64_t prot,
                            uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current || !current->vmem) {
        return -ESRCH;
    }
    if (size == 0) {
        return -EINVAL;
    }
    if ((addr & (PAGE_SIZE - 1)) != 0 || (size & (PAGE_SIZE - 1)) != 0) {
        return -EINVAL;
    }
    if (addr > UINT64_MAX - size) {
        return -EINVAL;
    }
    if ((prot & (PROT_EXEC | PROT_WRITE)) == (PROT_EXEC | PROT_WRITE)) {
        return -EACCES;
    }
    if (prot & PROT_EXEC) {
        uint64_t end = addr + size;
        if (current->memory.code_start == 0 || current->memory.code_end == 0 ||
            addr < current->memory.code_start || end > current->memory.code_end) {
            return -EACCES;
        }
    }
    if (enforce_user_memory(current) &&
        !current->vmem->isUserRange(addr, size)) {
        return -EFAULT;
    }
    if (!current->vmem->mprotect(addr, size, prot)) {
        return -EACCES;
    }
    return 0;
}

// ========== System Call Dispatcher ==========

class SyscallDispatcher {
private:
    syscall_handler_t handlers_[256];  // Up to 256 syscalls
    
public:
    SyscallDispatcher() {
        // Initialize all handlers to nullptr
        for (int i = 0; i < 256; i++) {
            handlers_[i] = nullptr;
        }
        
        // Register core syscalls
        register_handler(SYS_GETPID, sys_getpid);
        register_handler(SYS_GETPPID, sys_getppid);
        register_handler(SYS_TORUS_ID, sys_torus_id);
        register_handler(SYS_EXIT, sys_exit);
        register_handler(SYS_FORK, sys_fork);
        register_handler(SYS_WAIT, sys_wait);
        register_handler(SYS_KILL, sys_kill);
        register_handler(SYS_PS, sys_ps);
        register_handler(SYS_EXEC, sys_exec);
        register_handler(SYS_PIPE, sys_pipe);
        register_handler(SYS_SOCKET, sys_socket);
        register_handler(SYS_BIND, sys_bind);
        register_handler(SYS_LISTEN, sys_listen);
        register_handler(SYS_ACCEPT, sys_accept);
        register_handler(SYS_CONNECT, sys_connect);
        register_handler(SYS_DUP, sys_dup);
        register_handler(SYS_DUP2, sys_dup2);
        register_handler(SYS_OPEN, sys_open);
        register_handler(SYS_CLOSE, sys_close);
        register_handler(SYS_WRITE, sys_write);
        register_handler(SYS_READ, sys_read);
        register_handler(SYS_LSEEK, sys_lseek);
        register_handler(SYS_STAT, sys_stat);
        register_handler(SYS_UNLINK, sys_unlink);
        register_handler(SYS_LIST, sys_list);
        register_handler(SYS_MKDIR, sys_mkdir);
        register_handler(SYS_BRK, sys_brk);
        register_handler(SYS_MMAP, sys_mmap);
        register_handler(SYS_MUNMAP, sys_munmap);
        register_handler(SYS_MPROTECT, sys_mprotect);
        register_handler(SYS_TIME, sys_time);
        register_handler(SYS_SLEEP, sys_sleep);
        register_handler(SYS_NANOSLEEP, sys_nanosleep);
    }
    
    /**
     * Register a syscall handler.
     */
    void register_handler(int syscall_num, syscall_handler_t handler) {
        if (syscall_num >= 0 && syscall_num < 256) {
            handlers_[syscall_num] = handler;
        }
    }
    
    /**
     * Dispatch a syscall to its handler.
     */
    int64_t dispatch(int syscall_num, 
                     uint64_t arg1, uint64_t arg2, uint64_t arg3,
                     uint64_t arg4, uint64_t arg5, uint64_t arg6) {
        
        // Check syscall number
        if (syscall_num < 0 || syscall_num >= 256) {
            std::cerr << "[SyscallDispatcher] Invalid syscall number: " 
                      << syscall_num << std::endl;
            return -EINVAL;
        }
        
        // Get handler
        syscall_handler_t handler = handlers_[syscall_num];
        if (!handler) {
            std::cerr << "[SyscallDispatcher] Syscall not implemented: " 
                      << syscall_num << std::endl;
            return -ENOSYS;
        }
        
        // Call handler
        return handler(arg1, arg2, arg3, arg4, arg5, arg6);
    }
};

// ========== Global Syscall Function ==========

/**
 * Main syscall entry point.
 */
inline int64_t syscall(int syscall_num, 
                       uint64_t arg1, uint64_t arg2, uint64_t arg3,
                       uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    
    if (!current_torus_context || !current_torus_context->dispatcher) {
        std::cerr << "[syscall] No dispatcher available!" << std::endl;
        return -ENOSYS;
    }
    
    return current_torus_context->dispatcher->dispatch(
        syscall_num, arg1, arg2, arg3, arg4, arg5, arg6);
}

} // namespace os
