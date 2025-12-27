#pragma once

#include <cstdint>
#include <cstring>
#include "VirtualAllocator.h"
#include "ElfLoader.h"
#include "FileDescriptor.h"
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

inline size_t cstr_len(const char* s) {
    size_t len = 0;
    if (!s) {
        return 0;
    }
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

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
    bool kernel_owned;          // True if allocated by kernel heap
    
    // ========== CPU Context ==========
    CPUContext context;
    CPUContext saved_context;
    bool context_saved;
    
    // ========== Memory ==========
    MemoryLayout memory;
    
    // ========== Scheduling ==========
    uint32_t priority;          // Higher = more important (0-255)
    uint64_t time_slice;        // Ticks remaining in current slice
    uint64_t total_runtime;     // Total ticks this process has run
    uint64_t last_scheduled;    // Last time this process was scheduled
    
    // ========== I/O ==========
    FileDescriptorTable fd_table;

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
          kernel_owned(false),
          context(),
          saved_context(),
          context_saved(false),
          memory(),
          priority(100),          // Default priority
          time_slice(100),        // Default time slice
          total_runtime(0),
          last_scheduled(0),
          fd_table(),
          vmem(nullptr),
          user_step(nullptr),
          user_ctx(nullptr),
          syscalls(nullptr),
          x(0), y(0), z(0)
    {
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

    bool loadElfImage(const uint8_t* data, size_t size, uint64_t stack_size = 64 * 1024) {
        if (!vmem || !data || size == 0) {
            return false;
        }

        ElfImage image = {};
        ElfLoadError err = ElfLoadError::kOk;
        if (!parseElf64(data, size, image, &err)) {
            return false;
        }

        uint64_t min_vaddr = 0xFFFFFFFFFFFFFFFFull;
        uint64_t max_vaddr = 0;
        uint64_t data_start = 0xFFFFFFFFFFFFFFFFull;
        uint64_t data_end = 0;
        bool has_writable = false;

        for (size_t i = 0; i < image.segments.size(); ++i) {
            const ElfSegment& seg = image.segments[i];
            const uint8_t* seg_data = data + seg.offset;
            if (!vmem->mapSegment(seg_data, seg.filesz, seg.vaddr, seg.memsz, seg.flags)) {
                return false;
            }

            uint64_t seg_start = seg.vaddr;
            uint64_t seg_end = seg.vaddr + seg.memsz;
            if (seg_start < min_vaddr) {
                min_vaddr = seg_start;
            }
            if (seg_end > max_vaddr) {
                max_vaddr = seg_end;
            }
            if (seg.flags & PF_W) {
                has_writable = true;
                if (seg_start < data_start) {
                    data_start = seg_start;
                }
                if (seg_end > data_end) {
                    data_end = seg_end;
                }
            }
        }

        if (min_vaddr == 0xFFFFFFFFFFFFFFFFull) {
            return false;
        }

        memory.code_start = min_vaddr;
        memory.code_end = max_vaddr;
        if (has_writable) {
            memory.data_start = data_start;
            memory.data_end = data_end;
        } else {
            memory.data_start = 0;
            memory.data_end = 0;
        }

        vmem->setHeapStart(align_up(max_vaddr));
        memory.heap_start = vmem->getHeapStart();
        memory.heap_end = vmem->getHeapEnd();
        memory.heap_brk = vmem->getHeapBrk();

        uint64_t sp = vmem->allocateStack(stack_size);
        if (sp == 0) {
            return false;
        }
        uint64_t stack_bytes = align_up(stack_size);
        memory.stack_end = vmem->getStackEnd();
        uint64_t guard = stack_bytes > PAGE_SIZE ? PAGE_SIZE : 0;
        memory.stack_start = memory.stack_end - stack_bytes + guard;
        memory.stack_pointer = sp;

        context.rip = image.entry;
        context.rsp = sp;
        if (context.rflags == 0) {
            context.rflags = 0x202;
        }
        return true;
    }

    bool loadElfImageWithArgs(const uint8_t* data, size_t size,
                              const char* const* argv,
                              const char* const* envp,
                              uint64_t stack_size = 64 * 1024) {
        if (!loadElfImage(data, size, stack_size)) {
            return false;
        }
        return setupUserStack(argv, envp);
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

    void setKernelOwned(bool owned) {
        kernel_owned = owned;
    }

    bool isKernelOwned() const {
        return kernel_owned;
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
     * For the simulation, capture the tracked register state and stack pointer.
     */
    void saveContext() {
        saved_context = context;
        context_saved = true;
    }
    
    /**
     * Restore CPU context (called when resuming).
     * For the simulation, restore the tracked register state.
     */
    void restoreContext() {
        if (context_saved) {
            context = saved_context;
        }
        if (context.rsp == 0 && memory.stack_pointer != 0) {
            context.rsp = memory.stack_pointer;
        }
        if (context.rip == 0 && memory.code_start != 0) {
            context.rip = memory.code_start;
        }
    }
    
    /**
     * Execute one tick of this process.
     * In the simulation, advance a synthetic instruction pointer and invoke
     * the cooperative userspace hook when present.
     */
    void execute() {
        if (user_step && syscalls) {
            user_step(this, user_ctx, syscalls);
        }
        context.rip += 1;
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

private:
    bool setupUserStack(const char* const* argv, const char* const* envp) {
        if (!vmem) {
            return false;
        }
        uint64_t sp = memory.stack_pointer;
        const uint64_t stack_min = memory.stack_start;

        uint32_t argc = 0;
        uint32_t envc = 0;
        if (argv) {
            while (argv[argc]) {
                argc++;
            }
        }
        if (envp) {
            while (envp[envc]) {
                envc++;
            }
        }

        static constexpr uint32_t kMaxArgs = 32;
        static constexpr uint32_t kMaxEnv = 32;
        if (argc > kMaxArgs || envc > kMaxEnv) {
            return false;
        }

        uint64_t argv_addrs[kMaxArgs] = {};
        uint64_t envp_addrs[kMaxEnv] = {};

        for (uint32_t i = 0; i < argc; ++i) {
            size_t len = cstr_len(argv[i]) + 1;
            if (sp < stack_min + len) {
                return false;
            }
            sp -= static_cast<uint64_t>(len);
            if (!vmem->writeUser(sp, argv[i], len)) {
                return false;
            }
            argv_addrs[i] = sp;
        }

        for (uint32_t i = 0; i < envc; ++i) {
            size_t len = cstr_len(envp[i]) + 1;
            if (sp < stack_min + len) {
                return false;
            }
            sp -= static_cast<uint64_t>(len);
            if (!vmem->writeUser(sp, envp[i], len)) {
                return false;
            }
            envp_addrs[i] = sp;
        }

        sp &= ~0xFULL;

        auto push_u64 = [&](uint64_t value) -> bool {
            if (sp < stack_min + sizeof(uint64_t)) {
                return false;
            }
            sp -= sizeof(uint64_t);
            return vmem->writeUser(sp, &value, sizeof(uint64_t));
        };

        if (!push_u64(0)) {
            return false;
        }
        for (uint32_t i = envc; i-- > 0;) {
            if (!push_u64(envp_addrs[i])) {
                return false;
            }
        }
        uint64_t envp_ptr = sp;

        if (!push_u64(0)) {
            return false;
        }
        for (uint32_t i = argc; i-- > 0;) {
            if (!push_u64(argv_addrs[i])) {
                return false;
            }
        }
        uint64_t argv_ptr = sp;

        if (!push_u64(argc)) {
            return false;
        }

        context.rsp = sp;
        context.rdi = argc;
        context.rsi = argv_ptr;
        context.rdx = envp_ptr;
        if (context.rflags == 0) {
            context.rflags = 0x202;
        }
        memory.stack_pointer = sp;
        return true;
    }
};

} // namespace os
