#pragma once

#include <cstdint>
#include <cstring>
#include "VirtualAllocator.h"
#ifdef RSE_KERNEL
#include "KernelStubs.h"
#else
#include <iostream>
#endif

struct rse_syscalls;

/**
 * OSProcess: Real operating system process abstraction.
 * 
 * This extends the simple BettiRDLProcess to support:
 * - Process states (ready, running, blocked, zombie)
 * - CPU context (registers, stack, instruction pointer)
 * - Memory management (page table, heap, stack)
 * - Scheduling metadata (priority, runtime, time slice)
 * - Parent-child relationships
 */

namespace os {

enum class ProcessState : uint8_t {
    READY,      // Waiting to run
    RUNNING,    // Currently executing on CPU
    BLOCKED,    // Waiting for I/O or event
    ZOMBIE      // Terminated, waiting for parent to reap
};

/**
 * CPU context - saved/restored during context switch.
 * Simplified x86-64 register set.
 */
struct CPUContext {
    // Instruction and stack pointers
    uint64_t rip;  // Instruction pointer
    uint64_t rsp;  // Stack pointer
    uint64_t rbp;  // Base pointer
    
    // General purpose registers
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    
    // Flags
    uint64_t rflags;
    
    CPUContext() {
        std::memset(this, 0, sizeof(CPUContext));
    }
};

/**
 * Memory layout for a process.
 */
struct MemoryLayout {
    // Virtual address space
    PageTable* page_table;      // Pointer to page table
    
    // Code segment
    uint64_t code_start;
    uint64_t code_end;
    
    // Data segment
    uint64_t data_start;
    uint64_t data_end;
    
    // Heap (grows up)
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t heap_brk;          // Current break point
    
    // Stack (grows down)
    uint64_t stack_start;
    uint64_t stack_end;
    uint64_t stack_pointer;
    
    MemoryLayout() {
        std::memset(this, 0, sizeof(MemoryLayout));
    }
};

/**
 * Process-local file descriptor placeholder.
 */
struct ProcessFD {
    int fd;
    uint64_t offset;
    uint32_t flags;
    void* private_data;
    
    ProcessFD() : fd(-1), offset(0), flags(0), private_data(nullptr) {}
};

/**
 * OSProcess: Full operating system process.
 */
class OSProcess {
public:
    // ========== Identity ==========
    uint32_t pid;               // Process ID
    uint32_t parent_pid;        // Parent process ID
    uint32_t torus_id;          // Which torus owns this process
    
    // ========== State ==========
    ProcessState state;
    int exit_code;              // Exit code (if zombie)
    
    // ========== CPU Context ==========
    CPUContext context;
    
    // ========== Memory ==========
    MemoryLayout memory;
    
    // ========== Scheduling ==========
    uint32_t priority;          // Higher = more important (0-255)
    uint64_t time_slice;        // Ticks remaining in current slice
    uint64_t total_runtime;     // Total ticks this process has run
    uint64_t last_scheduled;    // Last time this process was scheduled
    
    // ========== I/O ==========
    static constexpr int MAX_FDS = 64;
    ProcessFD open_files[MAX_FDS];

    VirtualAllocator* vmem;

    // Optional userspace entry hook (cooperative in-kernel runner)
    using UserStepFn = void (*)(OSProcess* proc, void* user_ctx, const ::rse_syscalls* sys);
    UserStepFn user_step;
    void* user_ctx;
    const ::rse_syscalls* syscalls;
    
    // ========== Spatial Position (for Betti integration) ==========
    int x, y, z;                // Position in toroidal space
    
    // ========== Constructor ==========
    OSProcess(uint32_t pid, uint32_t parent_pid, uint32_t torus_id)
        : pid(pid), 
          parent_pid(parent_pid), 
          torus_id(torus_id),
          state(ProcessState::READY),
          exit_code(0),
          priority(100),          // Default priority
          time_slice(100),        // Default time slice
          total_runtime(0),
          last_scheduled(0),
          vmem(nullptr),
          user_step(nullptr),
          user_ctx(nullptr),
          syscalls(nullptr),
          x(0), y(0), z(0)
    {
        // Initialize standard file descriptors
        open_files[0].fd = 0;  // stdin
        open_files[1].fd = 1;  // stdout
        open_files[2].fd = 2;  // stderr
        
        for (int i = 3; i < MAX_FDS; i++) {
            open_files[i].fd = -1;
        }
    }

    void initMemory(PhysicalAllocator* phys_alloc) {
        if (vmem) {
            return;
        }
        memory.page_table = new PageTable();
        vmem = new VirtualAllocator(memory.page_table, phys_alloc);
        memory.heap_start = vmem->getHeapStart();
        memory.heap_end = vmem->getHeapEnd();
        memory.heap_brk = vmem->getHeapBrk();
    }
    
    // ========== State Management ==========
    
    bool isReady() const { return state == ProcessState::READY; }
    bool isRunning() const { return state == ProcessState::RUNNING; }
    bool isBlocked() const { return state == ProcessState::BLOCKED; }
    bool isZombie() const { return state == ProcessState::ZOMBIE; }
    
    void setReady() { state = ProcessState::READY; }
    void setRunning() { state = ProcessState::RUNNING; }
    void setBlocked() { state = ProcessState::BLOCKED; }
    void setZombie(int code) { 
        state = ProcessState::ZOMBIE; 
        exit_code = code;
    }
    
    // ========== Scheduling ==========
    
    /**
     * Check if time slice has expired.
     */
    bool timeSliceExpired() const {
        return time_slice == 0;
    }
    
    /**
     * Reset time slice (called when process is scheduled).
     */
    void resetTimeSlice(uint64_t slice = 100) {
        time_slice = slice;
    }
    
    /**
     * Consume one tick of time slice.
     */
    void tick() {
        if (time_slice > 0) {
            time_slice--;
        }
        total_runtime++;
    }
    
    /**
     * Get scheduling weight (for fair scheduling).
     */
    uint64_t getWeight() const {
        // Lower runtime = higher weight (gets scheduled sooner)
        return UINT64_MAX - total_runtime;
    }
    
    // ========== Context Switching ==========
    
    /**
     * Save CPU context (called when preempting).
     * In a real OS, this would save actual CPU registers.
     * For now, it's a placeholder.
     */
    void saveContext() {
        // In real implementation:
        // - Save all CPU registers to context
        // - Save FPU/SSE state
        // - Save stack pointer
        
        // For now: no-op (we're not running real code yet)
    }
    
    /**
     * Restore CPU context (called when resuming).
     * In a real OS, this would restore actual CPU registers.
     * For now, it's a placeholder.
     */
    void restoreContext() {
        // In real implementation:
        // - Restore all CPU registers from context
        // - Restore FPU/SSE state
        // - Switch to process's page table
        // - Jump to saved instruction pointer
        
        // For now: no-op (we're not running real code yet)
    }
    
    /**
     * Execute one tick of this process.
     * In a real OS, this would run the process's code.
     * For now, it's a placeholder.
     */
    void execute() {
        // In real implementation:
        // - Run process code for one time slice
        // - Handle system calls
        // - Handle interrupts
        if (user_step && syscalls) {
            user_step(this, user_ctx, syscalls);
        }
        // For now: just consume time
        tick();
    }

    void setUserEntry(UserStepFn step, void* ctx, const ::rse_syscalls* sys) {
        user_step = step;
        user_ctx = ctx;
        syscalls = sys;
    }
    
    // ========== Memory Management ==========
    
    /**
     * Allocate memory (sbrk-like).
     */
    uint64_t allocateMemory(uint64_t size) {
        if (vmem) {
            uint64_t addr = vmem->allocate(size);
            memory.heap_brk = vmem->getHeapBrk();
            return addr;
        }
        return 0;
    }
    
    /**
     * Free memory (simplified - no actual deallocation).
     */
    void freeMemory(uint64_t addr) {
        if (vmem) {
            vmem->free(addr, PAGE_SIZE);
            memory.heap_brk = vmem->getHeapBrk();
        }
    }
    
    // ========== File Descriptors ==========
    
    /**
     * Allocate a file descriptor.
     */
    int allocateFD() {
        for (int i = 3; i < MAX_FDS; i++) {
            if (open_files[i].fd == -1) {
                open_files[i].fd = i;
                return i;
            }
        }
        return -1;  // No free FDs
    }
    
    /**
     * Free a file descriptor.
     */
    void freeFD(int fd) {
        if (fd >= 0 && fd < MAX_FDS) {
            open_files[fd].fd = -1;
            open_files[fd].offset = 0;
            open_files[fd].flags = 0;
            open_files[fd].private_data = nullptr;
        }
    }
    
    /**
     * Get file descriptor.
     */
    ProcessFD* getFD(int fd) {
        if (fd >= 0 && fd < MAX_FDS && open_files[fd].fd != -1) {
            return &open_files[fd];
        }
        return nullptr;
    }
    
    // ========== Debugging ==========
    
    void print() const {
        const char* state_str = "UNKNOWN";
        switch (state) {
            case ProcessState::READY: state_str = "READY"; break;
            case ProcessState::RUNNING: state_str = "RUNNING"; break;
            case ProcessState::BLOCKED: state_str = "BLOCKED"; break;
            case ProcessState::ZOMBIE: state_str = "ZOMBIE"; break;
        }
        
        std::cout << "[Process " << pid << "]"
                  << " parent=" << parent_pid
                  << " torus=" << torus_id
                  << " state=" << state_str
                  << " priority=" << priority
                  << " runtime=" << total_runtime
                  << " slice=" << time_slice
                  << std::endl;
    }
};

} // namespace os
