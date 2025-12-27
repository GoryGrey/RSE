#pragma once

#include "Syscall.h"
#include "OSProcess.h"
#include "TorusScheduler.h"
#include "VFS.h"
#include "PhysicalAllocator.h"
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
    (void)status_ptr;
    OSProcess* current = get_current_process();
    if (!current) {
        return -ESRCH;
    }
    
    // For now, just return -ECHILD (no child processes)
    // Real implementation would search for zombie children
    // and block if none are ready
    
    std::cout << "[sys_wait] Process " << current->pid 
              << " waiting for child (not implemented)" << std::endl;
    
    return -ECHILD;
}

/**
 * sys_kill: Send signal to process
 */
inline int64_t sys_kill(uint64_t pid, uint64_t sig, uint64_t,
                        uint64_t, uint64_t, uint64_t) {
    std::cout << "[sys_kill] Sending signal " << sig 
              << " to process " << pid << " (not implemented)" << std::endl;
    
    // Not implemented yet
    return -ENOSYS;
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
        path = path_buf;
    }
    char* buf = reinterpret_cast<char*>(buf_addr);
    if (!buf || count == 0) {
        return -EINVAL;
    }
    if (!validate_user_range(current, buf_addr, count, true)) {
        return -EFAULT;
    }
    return current_torus_context->vfs->list(path ? path : "/", buf,
                                           static_cast<uint32_t>(count));
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
    current->vmem->munmap(addr, size);
    return 0;
}

inline int64_t sys_mprotect(uint64_t addr, uint64_t size, uint64_t prot,
                            uint64_t, uint64_t, uint64_t) {
    OSProcess* current = get_current_process();
    if (!current || !current->vmem) {
        return -ESRCH;
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
        register_handler(SYS_EXIT, sys_exit);
        register_handler(SYS_FORK, sys_fork);
        register_handler(SYS_WAIT, sys_wait);
        register_handler(SYS_KILL, sys_kill);
        register_handler(SYS_EXEC, sys_exec);
        register_handler(SYS_OPEN, sys_open);
        register_handler(SYS_CLOSE, sys_close);
        register_handler(SYS_WRITE, sys_write);
        register_handler(SYS_READ, sys_read);
        register_handler(SYS_LSEEK, sys_lseek);
        register_handler(SYS_UNLINK, sys_unlink);
        register_handler(SYS_LIST, sys_list);
        register_handler(SYS_BRK, sys_brk);
        register_handler(SYS_MMAP, sys_mmap);
        register_handler(SYS_MUNMAP, sys_munmap);
        register_handler(SYS_MPROTECT, sys_mprotect);
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
