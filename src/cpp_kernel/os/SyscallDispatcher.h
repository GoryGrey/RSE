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
    
    // Copy file descriptors
    for (int i = 0; i < OSProcess::MAX_FDS; i++) {
        child->open_files[i] = parent->open_files[i];
    }
    
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
 * sys_write: Write to file descriptor
 */
inline int64_t sys_write(uint64_t fd, uint64_t buf_addr, uint64_t count,
                         uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    return current_torus_context->vfs->write(static_cast<int32_t>(fd),
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
    return current_torus_context->vfs->read(static_cast<int32_t>(fd),
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
    const char* path = reinterpret_cast<const char*>(path_addr);
    if (!path) {
        return -EINVAL;
    }
    return current_torus_context->vfs->open(path,
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
    return current_torus_context->vfs->close(static_cast<int32_t>(fd));
}

/**
 * sys_lseek: Seek within a file
 */
inline int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence,
                         uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    return current_torus_context->vfs->lseek(static_cast<int32_t>(fd),
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
    const char* path = reinterpret_cast<const char*>(path_addr);
    if (!path) {
        return -EINVAL;
    }
    return current_torus_context->vfs->unlink(path);
}

inline int64_t sys_list(uint64_t path_addr, uint64_t buf_addr, uint64_t count,
                        uint64_t, uint64_t, uint64_t) {
    if (!current_torus_context || !current_torus_context->vfs) {
        return -ENOSYS;
    }
    const char* path = reinterpret_cast<const char*>(path_addr);
    char* buf = reinterpret_cast<char*>(buf_addr);
    if (!buf || count == 0) {
        return -EINVAL;
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
