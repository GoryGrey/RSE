#include <stddef.h>
#include <stdint.h>

#include "rse_syscalls.h"

#include "../src/cpp_kernel/os/SyscallDispatcher.h"
#include "../src/cpp_kernel/os/VFS.h"
#include "../src/cpp_kernel/os/MemFS.h"
#include "../src/cpp_kernel/os/FileDescriptor.h"
#include "../src/cpp_kernel/os/TorusScheduler.h"
#include "../src/cpp_kernel/os/OSProcess.h"
#include "../src/cpp_kernel/os/PhysicalAllocator.h"
#include "../src/cpp_kernel/os/ConsoleDevice.h"
#include "../src/cpp_kernel/os/BasicDevices.h"
#include "../src/cpp_kernel/os/BlockDevice.h"
#include "../src/cpp_kernel/os/LoopbackDevice.h"
#include "../src/cpp_kernel/os/NetDevice.h"
#include "../src/cpp_kernel/os/FastPathDevice.h"
#include "../src/cpp_kernel/braided/BraidCoordinator.h"
#include "../src/cpp_kernel/braided/BraidedKernel.h"
#if RSE_NET_EXCHANGE
#include "net_projection.h"
#endif

extern "C" const uint8_t _binary_init_user_elf_start[];
extern "C" const uint8_t _binary_init_user_elf_end[];

namespace os {
TorusContext* current_torus_context = nullptr;
}

static void *memcpy_local(void *dst, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < count; ++i) {
        d[i] = s[i];
    }
    return dst;
}

struct alloc_header {
    size_t size;
};

static uint8_t heap_area[8 * 1024 * 1024];
static size_t heap_offset;
alignas(4096) static uint8_t phys_mem[16 * 1024 * 1024];
static constexpr uint32_t kTorusCount = 3;
static constexpr uint32_t kExtraProcs = 4;
static constexpr uint32_t kRing3Slots = 1;
static constexpr uint64_t kKernelUserBase = 0x40000000ull;
static constexpr uint64_t kKernelUserWindow = 0x200000ull;
static constexpr uint64_t kKernelUserStackSize = 64 * 1024ull;
static constexpr uint64_t kKernelUserStackTop = kKernelUserBase + kKernelUserWindow - os::PAGE_SIZE;
static constexpr uint64_t kKernelUserStackBase = kKernelUserStackTop - kKernelUserStackSize;
static constexpr uint64_t kKernelUserHeapBase = kKernelUserBase;
static constexpr uint64_t kKernelUserHeapLimit = kKernelUserStackBase;

struct UserProgramState {
    uint32_t phase;
    uint32_t ticks;
    int fd;
};

static os::OSProcess* user_procs[kTorusCount][1 + kExtraProcs] = {};
#if !RSE_ENABLE_USERMODE
static UserProgramState user_states[kTorusCount][1 + kExtraProcs] = {};
#endif
static os::OSProcess* g_ring3_proc = nullptr;
static os::OSProcess* g_ring3_procs[kTorusCount][kRing3Slots] = {};
static uint32_t g_ring3_active[kTorusCount] = {};

static bool map_user_page(os::OSProcess* proc, uint64_t vaddr, uint64_t flags, uint64_t* phys_out) {
    if (!proc || !phys_out || !proc->memory.page_table || !proc->vmem) {
        return false;
    }
    serial_write("[RSE] user map page begin\n");
    uint64_t page = os::align_down(vaddr);
    uint64_t existing = proc->memory.page_table->translate(page);
    if (existing != 0) {
        *phys_out = existing;
        serial_write("[RSE] user map page existing\n");
        return true;
    }
    os::PhysicalAllocator* phys_alloc = proc->vmem->getPhysicalAllocator();
    if (!phys_alloc) {
        return false;
    }
    uint64_t phys = phys_alloc->allocateFrame();
    if (phys == 0) {
        return false;
    }
    serial_write("[RSE] user map page alloc\n");
    if (!proc->memory.page_table->map(page, phys, flags)) {
        phys_alloc->freeFrame(phys);
        return false;
    }
    serial_write("[RSE] user map page mapped\n");
    *phys_out = phys;
    return true;
}

extern "C" int rse_os_user_map(uint64_t code_vaddr, uint64_t stack_vaddr,
                               uint64_t *code_phys_out, uint64_t *stack_phys_out) {
    if (!code_phys_out || !stack_phys_out) {
        return 0;
    }
    serial_write("[RSE] user map begin\n");
    os::OSProcess* proc = g_ring3_proc ? g_ring3_proc : user_procs[0][0];
    if (!proc || !proc->vmem || !proc->memory.page_table) {
        return 0;
    }
    uint64_t code_page = os::align_down(code_vaddr);
    uint64_t stack_page = os::align_down(stack_vaddr);
    if (!proc->vmem->isUserRange(code_page, os::PAGE_SIZE) ||
        !proc->vmem->isUserRange(stack_page, os::PAGE_SIZE)) {
        serial_write("[RSE] user map range fail\n");
        return 0;
    }
    if (!map_user_page(proc, code_vaddr, os::PTE_PRESENT | os::PTE_USER, code_phys_out)) {
        serial_write("[RSE] user map code fail\n");
        return 0;
    }
    if (!map_user_page(proc, stack_vaddr,
                       os::PTE_PRESENT | os::PTE_USER | os::PTE_WRITABLE,
                       stack_phys_out)) {
        serial_write("[RSE] user map stack fail\n");
        return 0;
    }
    serial_write("[RSE] user map ok\n");
    if (proc->memory.code_start == 0 && proc->memory.code_end == 0) {
        proc->memory.code_start = code_page;
        proc->memory.code_end = code_page + os::PAGE_SIZE;
    }
    if (proc->memory.stack_start == 0 && proc->memory.stack_end == 0) {
        proc->memory.stack_start = stack_page;
        proc->memory.stack_end = stack_page + os::PAGE_SIZE;
    }
    if (proc->memory.stack_pointer == 0) {
        proc->memory.stack_pointer = stack_page + os::PAGE_SIZE;
    }
    return 1;
}

extern "C" int rse_os_user_ranges(uint64_t *code_start, uint64_t *code_end,
                                  uint64_t *data_start, uint64_t *data_end,
                                  uint64_t *stack_start, uint64_t *stack_end) {
    os::OSProcess* proc = g_ring3_proc ? g_ring3_proc : user_procs[0][0];
    if (!proc || !proc->memory.page_table) {
        return 0;
    }
    if (code_start) {
        *code_start = proc->memory.code_start;
    }
    if (code_end) {
        *code_end = proc->memory.code_end;
    }
    if (data_start) {
        *data_start = proc->memory.data_start;
    }
    if (data_end) {
        *data_end = proc->memory.data_end;
    }
    if (stack_start) {
        *stack_start = proc->memory.stack_start;
    }
    if (stack_end) {
        *stack_end = proc->memory.stack_end;
    }
    return 1;
}

extern "C" uint64_t rse_os_user_translate(uint64_t vaddr) {
    os::OSProcess* proc = g_ring3_proc ? g_ring3_proc : user_procs[0][0];
    if (!proc || !proc->memory.page_table || !proc->vmem) {
        return 0;
    }
    uint64_t page = os::align_down(vaddr);
    if (!proc->vmem->isUserRange(page, os::PAGE_SIZE)) {
        return 0;
    }
    return proc->memory.page_table->translate(page);
}

extern "C" uint64_t rse_os_user_flags(uint64_t vaddr) {
    os::OSProcess* proc = g_ring3_proc ? g_ring3_proc : user_procs[0][0];
    if (!proc || !proc->memory.page_table || !proc->vmem) {
        return 0;
    }
    uint64_t page = os::align_down(vaddr);
    if (!proc->vmem->isUserRange(page, os::PAGE_SIZE)) {
        return 0;
    }
    const os::PageTableEntry* pte = proc->memory.page_table->getPTE(page);
    if (!pte || !pte->isPresent()) {
        return 0;
    }
    uint64_t flags = 1;
    if (pte->isWritable()) {
        flags |= 2;
    }
    return flags;
}


#if !RSE_ENABLE_USERMODE
static void user_log_prefix(os::OSProcess* proc, const rse_syscalls* sys, const char* tag) {
    if (!proc || !sys || !tag) {
        return;
    }
    sys->log(tag);
    sys->log(" pid=");
    sys->log_u64(proc->pid);
    sys->log(" torus=");
    sys->log_u64(proc->torus_id);
    sys->log("\n");
}

static void user_program_fs(os::OSProcess* proc, void* ctx, const rse_syscalls* sys) {
    if (!proc || !ctx || !sys) {
        return;
    }
    UserProgramState* st = static_cast<UserProgramState*>(ctx);
    switch (st->phase) {
        case 0: {
            user_log_prefix(proc, sys, "[user] fs start");
            st->fd = sys->open("/persist/user.txt", os::O_CREAT | os::O_TRUNC | os::O_WRONLY);
            st->phase = 1;
        } break;
        case 1: {
            if (st->fd >= 0) {
                const char msg[] = "userland fs ok\n";
                sys->write(st->fd, (const uint8_t *)msg, (uint32_t)(sizeof(msg) - 1));
                sys->close(st->fd);
            }
            st->phase = 2;
        } break;
        case 2: {
            st->fd = sys->open("/persist/user.txt", os::O_RDONLY);
            st->phase = 3;
        } break;
        case 3: {
            if (st->fd >= 0) {
                char buf[80] = {};
                int got = sys->read(st->fd, (uint8_t *)buf, (uint32_t)(sizeof(buf) - 1));
                sys->close(st->fd);
                if (got > 0) {
                    sys->log("[user] fs read: ");
                    sys->log(buf);
                    sys->log("\n");
                }
            }
            st->phase = 4;
        } break;
        case 4: {
            char listbuf[128] = {};
            int got = sys->list("/persist", listbuf, (uint32_t)(sizeof(listbuf) - 1));
            if (got > 0) {
                sys->log("[user] fs list /persist: ");
                sys->log(listbuf);
                sys->log("\n");
            }
            st->phase = 5;
        } break;
        case 5: {
            os::syscall(os::SYS_EXIT, 0);
            st->phase = 6;
        } break;
        default:
            break;
    }
}

static void user_program_net(os::OSProcess* proc, void* ctx, const rse_syscalls* sys) {
    if (!proc || !ctx || !sys) {
        return;
    }
    UserProgramState* st = static_cast<UserProgramState*>(ctx);
    switch (st->phase) {
        case 0: {
            user_log_prefix(proc, sys, "[user] net start");
            st->fd = sys->open("/dev/net0", os::O_RDWR);
            st->phase = 1;
        } break;
        case 1: {
            if (st->fd >= 0) {
                const char msg[] = "userland net ping";
                sys->write(st->fd, (const uint8_t *)msg, (uint32_t)(sizeof(msg) - 1));
            }
            st->ticks = 0;
            st->phase = 2;
        } break;
        case 2: {
            if (st->fd >= 0) {
                uint8_t buf[64] = {};
                int got = sys->read(st->fd, buf, (uint32_t)sizeof(buf));
                if (got > 0) {
                    sys->log("[user] net rx bytes=");
                    sys->log_u64((uint64_t)got);
                    sys->log("\n");
                    sys->close(st->fd);
                    st->phase = 3;
                    break;
                }
            }
            st->ticks++;
            if (st->ticks > 50) {
                sys->log("[user] net rx timeout\n");
                if (st->fd >= 0) {
                    sys->close(st->fd);
                }
                st->phase = 3;
            }
        } break;
        case 3: {
            os::syscall(os::SYS_EXIT, 0);
            st->phase = 4;
        } break;
        default:
            break;
    }
}

static void user_program_compute(os::OSProcess* proc, void* ctx, const rse_syscalls* sys) {
    if (!proc || !ctx || !sys) {
        return;
    }
    UserProgramState* st = static_cast<UserProgramState*>(ctx);
    if (st->phase == 0) {
        user_log_prefix(proc, sys, "[user] compute start");
        uint64_t start = sys->rdtsc();
        uint64_t acc = 0;
        for (uint32_t i = 0; i < 20000; ++i) {
            acc = (acc << 1) ^ (acc + i * 2654435761u);
        }
        uint64_t end = sys->rdtsc();
        sys->log("[user] compute cycles=");
        sys->log_u64(end - start);
        sys->log(" checksum=");
        sys->log_u64(acc);
        sys->log("\n");
        os::syscall(os::SYS_EXIT, 0);
        st->phase = 1;
    }
}
#endif
static constexpr uint32_t kPipeQueues = 4;
static constexpr uint32_t kPipeSlots = 64;
static constexpr uint32_t kPipeMsgMax = 128;
static constexpr uint32_t kLoadSkew = 3;
static constexpr uint32_t kReadySkew = 3;
static constexpr uint32_t kBlockedSkew = 2;
static constexpr uint32_t kBoundarySkew = 4;
static constexpr uint32_t kPressureSkew = 3;
static constexpr uint32_t kCostSkew = 6;

struct PipeSlot {
    uint32_t len;
    uint8_t data[kPipeMsgMax];
};

struct PipeQueue {
    uint32_t head;
    uint32_t tail;
    PipeSlot slots[kPipeSlots];
};

static PipeQueue pipe_queues[kPipeQueues];

struct TorusMetrics {
    uint64_t block_bytes;
    uint64_t block_cycles;
    uint64_t net_bytes;
    uint64_t net_cycles;
};

static TorusMetrics torus_metrics[kTorusCount];

extern "C" void *malloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    size_t aligned = (size + 7u) & ~7u;
    size_t total = aligned + sizeof(alloc_header);
    if (heap_offset + total > sizeof(heap_area)) {
        return nullptr;
    }
    alloc_header *hdr = (alloc_header *)(heap_area + heap_offset);
    hdr->size = aligned;
    heap_offset += total;
    return (void *)(hdr + 1);
}

extern "C" void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        return nullptr;
    }
    alloc_header *hdr = ((alloc_header *)ptr) - 1;
    size_t old_size = hdr->size;
    void *next = malloc(size);
    if (!next) {
        return nullptr;
    }
    size_t to_copy = old_size < size ? old_size : size;
    memcpy_local(next, ptr, to_copy);
    return next;
}

extern "C" void free(void *ptr) {
    (void)ptr;
}

void* operator new(size_t size) noexcept {
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void* operator new[](size_t size) noexcept {
    return malloc(size);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    free(ptr);
}


extern "C" int strcmp(const char* lhs, const char* rhs) {
    if (lhs == rhs) {
        return 0;
    }
    while (*lhs && (*lhs == *rhs)) {
        ++lhs;
        ++rhs;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

extern "C" int strncmp(const char* lhs, const char* rhs, size_t n) {
    if (n == 0 || lhs == rhs) {
        return 0;
    }
    for (size_t i = 0; i < n; ++i) {
        unsigned char lc = (unsigned char)lhs[i];
        unsigned char rc = (unsigned char)rhs[i];
        if (lc != rc || lc == '\0' || rc == '\0') {
            return lc - rc;
        }
    }
    return 0;
}

extern "C" char* strncpy(char* dst, const char* src, size_t n) {
    if (n == 0) {
        return dst;
    }
    size_t i = 0;
    for (; i < n && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    for (; i < n; ++i) {
        dst[i] = '\0';
    }
    return dst;
}

extern "C" size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}

extern "C" int memcmp(const void* lhs, const void* rhs, size_t count) {
    const uint8_t* a = static_cast<const uint8_t*>(lhs);
    const uint8_t* b = static_cast<const uint8_t*>(rhs);
    for (size_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
    }
    return 0;
}

const char* strchr(const char* str, int ch) __asm("strchr");
const char* strchr(const char* str, int ch) {
    char target = static_cast<char>(ch);
    for (const char* p = str; ; ++p) {
        if (*p == target) {
            return p;
        }
        if (*p == '\0') {
            return nullptr;
        }
    }
}

const char* strrchr(const char* str, int ch) __asm("strrchr");
const char* strrchr(const char* str, int ch) {
    char target = static_cast<char>(ch);
    const char* last = nullptr;
    for (const char* p = str; ; ++p) {
        if (*p == target) {
            last = p;
        }
        if (*p == '\0') {
            break;
        }
    }
    return last;
}

extern "C" int __cxa_guard_acquire(long long* guard) {
    if ((*guard) != 0) {
        return 0;
    }
    *guard = 1;
    return 1;
}

extern "C" void __cxa_guard_release(long long* guard) {
    *guard = 1;
}

extern "C" void __cxa_guard_abort(long long* guard) {
    *guard = 0;
}

extern "C" void serial_write(const char *s);
extern "C" void serial_write_u64(uint64_t value);
static uint32_t current_torus_id;
extern "C" uint32_t rse_get_torus_id(void) {
    return current_torus_id;
}
extern "C" uint32_t rse_pipe_push(uint32_t queue_id, const uint8_t* buf, uint32_t len) {
    if (!buf || queue_id >= kPipeQueues) {
        return 0;
    }
    PipeQueue& q = pipe_queues[queue_id];
    uint32_t next = (q.head + 1) % kPipeSlots;
    if (next == q.tail) {
        return 0;
    }
    uint32_t to_copy = len > kPipeMsgMax ? kPipeMsgMax : len;
    q.slots[q.head].len = to_copy;
    memcpy_local(q.slots[q.head].data, buf, to_copy);
    q.head = next;
    return to_copy;
}

extern "C" uint32_t rse_pipe_pop(uint32_t queue_id, uint8_t* buf, uint32_t max_len) {
    if (!buf || queue_id >= kPipeQueues) {
        return 0;
    }
    PipeQueue& q = pipe_queues[queue_id];
    if (q.head == q.tail) {
        return 0;
    }
    PipeSlot& slot = q.slots[q.tail];
    uint32_t to_copy = slot.len;
    if (to_copy > max_len) {
        to_copy = max_len;
    }
    memcpy_local(buf, slot.data, to_copy);
    q.tail = (q.tail + 1) % kPipeSlots;
    return to_copy;
}

extern "C" void rse_report_block(uint64_t bytes, uint64_t cycles) {
    TorusMetrics& m = torus_metrics[current_torus_id];
    m.block_bytes += bytes;
    m.block_cycles += cycles;
}

extern "C" void rse_report_net(uint64_t bytes, uint64_t cycles) {
    TorusMetrics& m = torus_metrics[current_torus_id];
    m.net_bytes += bytes;
    m.net_cycles += cycles;
}

extern "C" void __assert_fail(const char* expr, const char* file,
                              unsigned int line, const char* func) {
    serial_write("ASSERT: ");
    serial_write(expr ? expr : "(null)");
    serial_write(" @ ");
    serial_write(file ? file : "(null)");
    serial_write(":");
    (void)line;
    serial_write("\n");
    serial_write(func ? func : "(null)");
    serial_write("\n");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static os::MemFS* create_memfs(uint32_t torus_id) {
    alignas(os::MemFS) static uint8_t storage[kTorusCount][sizeof(os::MemFS)];
    return new (storage[torus_id]) os::MemFS();
}

static os::VFS* create_vfs(uint32_t torus_id, os::MemFS* fs) {
    alignas(os::VFS) static uint8_t storage[kTorusCount][sizeof(os::VFS)];
    return new (storage[torus_id]) os::VFS(fs);
}

static os::TorusScheduler* create_scheduler(uint32_t torus_id) {
    alignas(os::TorusScheduler) static uint8_t storage[kTorusCount][sizeof(os::TorusScheduler)];
    return new (storage[torus_id]) os::TorusScheduler(torus_id);
}

static os::SyscallDispatcher* create_dispatcher(uint32_t torus_id) {
    alignas(os::SyscallDispatcher) static uint8_t storage[kTorusCount][sizeof(os::SyscallDispatcher)];
    return new (storage[torus_id]) os::SyscallDispatcher();
}

static os::DeviceManager* create_device_manager(uint32_t torus_id) {
    alignas(os::DeviceManager) static uint8_t storage[kTorusCount][sizeof(os::DeviceManager)];
    return new (storage[torus_id]) os::DeviceManager();
}

static os::BlockFS* create_blockfs(uint32_t torus_id) {
    alignas(os::BlockFS) static uint8_t storage[kTorusCount][sizeof(os::BlockFS)];
    return new (storage[torus_id]) os::BlockFS();
}

static os::PhysicalAllocator* create_phys_alloc(uint32_t torus_id) {
    alignas(os::PhysicalAllocator) static uint8_t storage[kTorusCount][sizeof(os::PhysicalAllocator)];
    const size_t total = sizeof(phys_mem);
    static constexpr size_t kPhysAlign = 4096;
    const size_t raw_stride = total / kTorusCount;
    const size_t stride = raw_stride & ~(kPhysAlign - 1);
    const size_t offset = stride * torus_id;
    const size_t size = (torus_id == (kTorusCount - 1)) ? (total - offset) : stride;
    uint8_t* base = phys_mem + offset;
    return new (storage[torus_id]) os::PhysicalAllocator((uint64_t)base, size);
}

static os::OSProcess* create_process(uint32_t torus_id, uint32_t slot, uint32_t pid, uint32_t parent_pid) {
    static constexpr uint32_t kProcSlots = 1 + kExtraProcs;
    alignas(os::OSProcess) static uint8_t storage[kTorusCount][kProcSlots][sizeof(os::OSProcess)];
    return new (storage[torus_id][slot]) os::OSProcess(pid, parent_pid, torus_id);
}

static os::OSProcess* create_ring3_process(uint32_t torus_id, uint32_t slot,
                                           uint32_t pid, uint32_t parent_pid) {
    alignas(os::OSProcess) static uint8_t storage[kTorusCount][kRing3Slots][sizeof(os::OSProcess)];
    return new (storage[torus_id][slot]) os::OSProcess(pid, parent_pid, torus_id);
}


#if !RSE_ENABLE_USERMODE
static int os_open_shim(const char *name, uint32_t flags) {
    return (int)os::syscall(os::SYS_OPEN, (uint64_t)name, flags, 0644);
}

static int os_close_shim(int fd) {
    return (int)os::syscall(os::SYS_CLOSE, (uint64_t)fd);
}

static int os_write_shim(int fd, const uint8_t *buf, uint32_t len) {
    return (int)os::syscall(os::SYS_WRITE, (uint64_t)fd, (uint64_t)buf, len);
}

static int os_read_shim(int fd, uint8_t *buf, uint32_t len) {
    return (int)os::syscall(os::SYS_READ, (uint64_t)fd, (uint64_t)buf, len);
}

static int os_unlink_shim(const char *name) {
    return (int)os::syscall(os::SYS_UNLINK, (uint64_t)name);
}

static int os_lseek_shim(int fd, int64_t offset, int whence) {
    return (int)os::syscall(os::SYS_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence);
}

static int os_list_shim(const char *path, char *buf, uint32_t len) {
    return (int)os::syscall(os::SYS_LIST, (uint64_t)path, (uint64_t)buf, len);
}

static int os_stat_shim(const char *path, struct rse_stat *out) {
    return (int)os::syscall(os::SYS_STAT, (uint64_t)path, (uint64_t)out);
}
#endif
#if !RSE_ENABLE_USERMODE
extern "C" void init_main(const struct rse_syscalls *sys);
#endif
extern "C" uint64_t kernel_rdtsc(void);
extern "C" int rse_block_init(void);
extern "C" uint32_t rse_block_size(void);
extern "C" uint64_t rse_block_total_blocks(void);
extern "C" int rse_net_init(void);

extern "C" void rse_braid_smoke(void) {
    serial_write("[RSE] braided smoke start\n");

    alignas(braided::BraidedKernel) static uint8_t torus_a_storage[sizeof(braided::BraidedKernel)];
    alignas(braided::BraidedKernel) static uint8_t torus_b_storage[sizeof(braided::BraidedKernel)];
    alignas(braided::BraidedKernel) static uint8_t torus_c_storage[sizeof(braided::BraidedKernel)];
    alignas(braided::BraidCoordinator) static uint8_t coordinator_storage[sizeof(braided::BraidCoordinator)];
    static bool braid_inited = false;

    braided::BraidedKernel* torus_a = reinterpret_cast<braided::BraidedKernel*>(torus_a_storage);
    braided::BraidedKernel* torus_b = reinterpret_cast<braided::BraidedKernel*>(torus_b_storage);
    braided::BraidedKernel* torus_c = reinterpret_cast<braided::BraidedKernel*>(torus_c_storage);
    braided::BraidCoordinator* coordinator =
        reinterpret_cast<braided::BraidCoordinator*>(coordinator_storage);

    if (!braid_inited) {
        new (torus_a) braided::BraidedKernel();
        new (torus_b) braided::BraidedKernel();
        new (torus_c) braided::BraidedKernel();
        new (coordinator) braided::BraidCoordinator();
        braid_inited = true;
    }

    torus_a->setTorusId(0);
    torus_b->setTorusId(1);
    torus_c->setTorusId(2);

    for (int y = 0; y < 2; ++y) {
        for (int z = 0; z < 2; ++z) {
            torus_a->spawnProcess(0, y, z);
            torus_b->spawnProcess(0, y, z);
            torus_c->spawnProcess(0, y, z);
        }
    }

    torus_a->createEdge(0, 0, 0, 0, 0, 0, 1);
    torus_b->createEdge(0, 0, 0, 0, 0, 0, 1);
    torus_c->createEdge(0, 0, 0, 0, 0, 0, 1);

    torus_a->injectEvent(0, 0, 0, 0, 0, 0, 1);
    torus_b->injectEvent(0, 0, 0, 0, 0, 0, 1);
    torus_c->injectEvent(0, 0, 0, 0, 0, 0, 1);

    const uint64_t ticks = 300;
    const uint64_t braid_interval = 30;
    for (uint64_t i = 0; i < ticks; ++i) {
        torus_a->tick();
        torus_b->tick();
        torus_c->tick();

        if ((i + 1) % braid_interval == 0) {
            coordinator->exchange(*torus_a, *torus_b, *torus_c);
        }
    }

    const braided::Projection proj_a = torus_a->extractProjection();
    const braided::Projection proj_b = torus_b->extractProjection();
    const braided::Projection proj_c = torus_c->extractProjection();

    serial_write("[RSE] braid cycles=");
    serial_write_u64(coordinator->getExchangeCount());
    serial_write(" ticks=");
    serial_write_u64(ticks);
    serial_write("\n");

    serial_write("[RSE] braid A events=");
    serial_write_u64(proj_a.total_events_processed);
    serial_write(" active=");
    serial_write_u64(proj_a.active_processes);
    serial_write(" edges=");
    serial_write_u64(proj_a.edge_count);
    serial_write(" pending=");
    serial_write_u64(proj_a.pending_events);
    serial_write("\n");

    serial_write("[RSE] braid B events=");
    serial_write_u64(proj_b.total_events_processed);
    serial_write(" active=");
    serial_write_u64(proj_b.active_processes);
    serial_write(" edges=");
    serial_write_u64(proj_b.edge_count);
    serial_write(" pending=");
    serial_write_u64(proj_b.pending_events);
    serial_write("\n");

    serial_write("[RSE] braid C events=");
    serial_write_u64(proj_c.total_events_processed);
    serial_write(" active=");
    serial_write_u64(proj_c.active_processes);
    serial_write(" edges=");
    serial_write_u64(proj_c.edge_count);
    serial_write(" pending=");
    serial_write_u64(proj_c.pending_events);
    serial_write("\n");

    serial_write("[RSE] braided smoke done\n");
}

struct TorusRuntime {
    os::TorusContext ctx;
    os::MemFS* memfs;
    os::VFS* vfs;
    os::BlockFS* blockfs;
    os::DeviceManager* dev_mgr;
    os::Device* console;
    os::Device* fastio;
    os::TorusScheduler* scheduler;
    os::SyscallDispatcher* dispatcher;
    os::PhysicalAllocator* phys_alloc;
};

static bool load_init_elf_payload(const uint8_t** data_out, size_t* size_out) {
    if (!data_out || !size_out) {
        return false;
    }
    const uint8_t* start = _binary_init_user_elf_start;
    const uint8_t* end = _binary_init_user_elf_end;
    if (!start || !end || end <= start) {
        return false;
    }
    *data_out = start;
    *size_out = static_cast<size_t>(end - start);
    return true;
}

static void configure_ring3_memory(os::OSProcess* proc) {
    if (!proc || !proc->vmem) {
        return;
    }
    proc->vmem->setStackBounds(kKernelUserStackBase, kKernelUserStackTop);
    proc->vmem->setHeapBounds(kKernelUserHeapBase, kKernelUserHeapLimit);
}

static bool load_init_elf_image(os::OSProcess* proc) {
    if (!proc) {
        return false;
    }
    const uint8_t* image = nullptr;
    size_t image_size = 0;
    if (!load_init_elf_payload(&image, &image_size)) {
        return false;
    }
    return proc->loadElfImageWithArgs(image, image_size, nullptr, nullptr);
}

static bool install_init_elf(TorusRuntime& rt, os::OSProcess* proc) {
    if (!rt.vfs || !proc) {
        return false;
    }
    const uint8_t* image = nullptr;
    size_t image_size = 0;
    if (!load_init_elf_payload(&image, &image_size)) {
        serial_write("[RSE] init elf payload missing\n");
        return false;
    }
    if (rt.vfs->mkdir("/bin", 0755) < 0) {
        // Directory may already exist; ignore failure here.
    }
    const char path[] = "/bin/init";
    int32_t existing = rt.vfs->open(&proc->fd_table, path, os::O_RDONLY);
    if (existing >= 0) {
        rt.vfs->close(&proc->fd_table, existing);
        return true;
    }
    int32_t fd = rt.vfs->open(&proc->fd_table, path,
                              os::O_CREAT | os::O_TRUNC | os::O_WRONLY, 0755);
    if (fd < 0) {
        serial_write("[RSE] init elf open failed\n");
        return false;
    }
    int64_t written = rt.vfs->write(&proc->fd_table, fd, image, (uint32_t)image_size);
    rt.vfs->close(&proc->fd_table, fd);
    if (written != (int64_t)image_size) {
        serial_write("[RSE] init elf write failed\n");
        return false;
    }
    return true;
}

static TorusRuntime g_runtimes[kTorusCount] = {};
static int g_runtimes_ready = 0;

extern "C" int rse_os_fastio_bench(uint64_t* bytes_out,
                                   uint64_t* cycles_out,
                                   uint64_t* cycles_per_byte_out) {
    if (!bytes_out || !cycles_out || !cycles_per_byte_out) {
        return 0;
    }
    if (!g_runtimes_ready) {
        return 0;
    }
    TorusRuntime& rt = g_runtimes[0];
    if (!rt.fastio || !rt.fastio->write || !rt.fastio->read) {
        return 0;
    }

    os::fastpath_reset(rt.fastio);

    static uint8_t payload[4096];
    static uint8_t scratch[4096];
    for (uint32_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)(i ^ 0x5a);
        scratch[i] = 0;
    }

    const uint32_t iterations = 256;
    uint64_t bytes = 0;
    uint64_t start = kernel_rdtsc();
    for (uint32_t i = 0; i < iterations; ++i) {
        ssize_t wrote = rt.fastio->write(rt.fastio, payload, sizeof(payload));
        if (wrote <= 0) {
            break;
        }
        bytes += (uint64_t)wrote;
        uint32_t remaining = (uint32_t)wrote;
        uint32_t offset = 0;
        while (remaining > 0) {
            ssize_t got = rt.fastio->read(rt.fastio, scratch + offset, remaining);
            if (got <= 0) {
                remaining = 0;
                break;
            }
            bytes += (uint64_t)got;
            remaining -= (uint32_t)got;
            offset += (uint32_t)got;
        }
        if (offset == 0) {
            break;
        }
    }
    uint64_t end = kernel_rdtsc();

    uint64_t cycles = end - start;
    uint64_t cycles_per_byte = bytes ? (cycles / bytes) : 0;
    *bytes_out = bytes;
    *cycles_out = cycles;
    *cycles_per_byte_out = cycles_per_byte;
    return 1;
}

#if !RSE_ENABLE_USERMODE
static int os_ps_dump(char *buf, uint32_t len) {
    if (!buf || len == 0) {
        return -1;
    }
    uint32_t used = 0;
    auto append_char = [&](char c) -> bool {
        if (used + 1 >= len) {
            return false;
        }
        buf[used++] = c;
        return true;
    };
    auto append_str = [&](const char *s) -> bool {
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
    auto state_str = [](os::ProcessState state) -> const char * {
        switch (state) {
            case os::ProcessState::READY: return "READY";
            case os::ProcessState::RUNNING: return "RUNNING";
            case os::ProcessState::BLOCKED: return "BLOCKED";
            case os::ProcessState::ZOMBIE: return "ZOMBIE";
        }
        return "UNKNOWN";
    };

    bool wrote = false;
    for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
        TorusRuntime &rt = g_runtimes[torus_id];
        if (!rt.scheduler) {
            continue;
        }
        rt.scheduler->forEachProcess([&](os::OSProcess *proc) {
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
    }
    if (!wrote) {
        append_str("ps: empty\n");
    }
    if (used < len) {
        buf[used] = '\0';
    }
    return (int)used;
}
#endif

static bool ring3_alive(const os::OSProcess* proc) {
    return proc && !proc->isZombie();
}

static os::OSProcess* ring3_pick_next(uint32_t torus_id, uint32_t* out_slot) {
    if (torus_id >= kTorusCount || !out_slot) {
        return nullptr;
    }
    uint32_t start = g_ring3_active[torus_id] % kRing3Slots;
    for (uint32_t offset = 1; offset <= kRing3Slots; ++offset) {
        uint32_t idx = (start + offset) % kRing3Slots;
        os::OSProcess* proc = g_ring3_procs[torus_id][idx];
        if (ring3_alive(proc)) {
            *out_slot = idx;
            return proc;
        }
    }
    os::OSProcess* current = g_ring3_procs[torus_id][start];
    if (ring3_alive(current)) {
        *out_slot = start;
        return current;
    }
    return nullptr;
}

static void ring3_set_active(uint32_t torus_id, uint32_t slot) {
    if (torus_id >= kTorusCount || slot >= kRing3Slots) {
        return;
    }
    g_ring3_active[torus_id] = slot;
    g_ring3_proc = g_ring3_procs[torus_id][slot];
    TorusRuntime& rt = g_runtimes[torus_id];
    current_torus_id = torus_id;
    os::current_torus_context = &rt.ctx;
    if (rt.scheduler && g_ring3_proc) {
        rt.scheduler->forceCurrentProcess(g_ring3_proc);
    }
}

extern "C" int rse_os_prepare_ring3(uint32_t torus_id) {
    if (!g_runtimes_ready) {
        return 0;
    }
    if (torus_id >= kTorusCount) {
        return 0;
    }
    TorusRuntime& rt = g_runtimes[torus_id];
    if (!rt.scheduler || !rt.dispatcher) {
        return 0;
    }
    uint32_t slot = g_ring3_active[torus_id] % kRing3Slots;
    os::OSProcess* proc = g_ring3_procs[torus_id][slot];
    if (!ring3_alive(proc)) {
        proc = ring3_pick_next(torus_id, &slot);
    }
    if (!proc) {
        return 0;
    }
    ring3_set_active(torus_id, slot);
    return 1;
}

extern "C" int rse_os_ring3_context(uint64_t* entry_out, uint64_t* stack_out);

struct Ring3Frame {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, rsp, ss;
};

extern "C" void rse_os_sync_ring3_frame(const void* frame_ptr) {
    if (!g_ring3_proc || !frame_ptr) {
        return;
    }
    const Ring3Frame* frame = static_cast<const Ring3Frame*>(frame_ptr);
    os::CPUContext& ctx = g_ring3_proc->context;
    ctx.rax = frame->rax;
    ctx.rbx = frame->rbx;
    ctx.rcx = frame->rcx;
    ctx.rdx = frame->rdx;
    ctx.rsi = frame->rsi;
    ctx.rdi = frame->rdi;
    ctx.rbp = frame->rbp;
    ctx.r8 = frame->r8;
    ctx.r9 = frame->r9;
    ctx.r10 = frame->r10;
    ctx.r11 = frame->r11;
    ctx.r12 = frame->r12;
    ctx.r13 = frame->r13;
    ctx.r14 = frame->r14;
    ctx.r15 = frame->r15;
    ctx.rip = frame->rip;
    ctx.rsp = frame->rsp;
    ctx.rflags = frame->rflags;
}

extern "C" int rse_os_ring3_load_frame(void* frame_ptr) {
    if (!g_ring3_proc || !frame_ptr) {
        return 0;
    }
    Ring3Frame* frame = static_cast<Ring3Frame*>(frame_ptr);
    const os::CPUContext& ctx = g_ring3_proc->context;
    frame->rax = ctx.rax;
    frame->rbx = ctx.rbx;
    frame->rcx = ctx.rcx;
    frame->rdx = ctx.rdx;
    frame->rsi = ctx.rsi;
    frame->rdi = ctx.rdi;
    frame->rbp = ctx.rbp;
    frame->r8 = ctx.r8;
    frame->r9 = ctx.r9;
    frame->r10 = ctx.r10;
    frame->r11 = ctx.r11;
    frame->r12 = ctx.r12;
    frame->r13 = ctx.r13;
    frame->r14 = ctx.r14;
    frame->r15 = ctx.r15;
    frame->rip = ctx.rip;
    if (ctx.rsp != 0) {
        frame->rsp = ctx.rsp;
    } else if (g_ring3_proc->memory.stack_pointer != 0) {
        frame->rsp = g_ring3_proc->memory.stack_pointer;
    }
    if (ctx.rflags != 0) {
        frame->rflags = ctx.rflags;
    }
    return 1;
}

extern "C" int rse_os_ring3_yield(uint64_t* entry_out, uint64_t* stack_out) {
    if (!entry_out || !stack_out) {
        return 0;
    }
    if (!g_runtimes_ready || !g_ring3_proc) {
        return 0;
    }
    uint32_t torus_id = g_ring3_proc->torus_id;
    if (torus_id >= kTorusCount) {
        return 0;
    }
    uint32_t slot = 0;
    os::OSProcess* next = ring3_pick_next(torus_id, &slot);
    if (!next) {
        return 0;
    }
    ring3_set_active(torus_id, slot);
    return rse_os_ring3_context(entry_out, stack_out);
}

extern "C" int64_t rse_os_syscall_dispatch(int64_t num,
                                           uint64_t arg1, uint64_t arg2,
                                           uint64_t arg3, uint64_t arg4,
                                           uint64_t arg5, uint64_t arg6) {
    if (!os::current_torus_context || !os::current_torus_context->dispatcher) {
        return -1;
    }
    return os::syscall((int)num, arg1, arg2, arg3, arg4, arg5, arg6);
}

extern "C" int rse_os_ring3_entry(uint64_t* entry_out) {
    if (!entry_out || !g_ring3_proc) {
        return 0;
    }
    if (g_ring3_proc->context.rip == 0) {
        return 0;
    }
    *entry_out = g_ring3_proc->context.rip;
    return 1;
}

extern "C" int rse_os_ring3_context(uint64_t* entry_out, uint64_t* stack_out) {
    if (!g_ring3_proc || (!entry_out && !stack_out)) {
        return 0;
    }
    if (g_ring3_proc->context.rip == 0) {
        return 0;
    }
    if (entry_out) {
        *entry_out = g_ring3_proc->context.rip;
    }
    if (stack_out) {
        uint64_t sp = g_ring3_proc->context.rsp;
        if (sp == 0) {
            sp = g_ring3_proc->memory.stack_pointer;
        }
        *stack_out = sp;
    }
    return 1;
}

static void braid_log_loads(TorusRuntime* runtimes) {
    serial_write("[RSE] torus load a=");
    serial_write_u64(runtimes[0].scheduler->getProcessCount());
    serial_write(" b=");
    serial_write_u64(runtimes[1].scheduler->getProcessCount());
    serial_write(" c=");
    serial_write_u64(runtimes[2].scheduler->getProcessCount());
    serial_write("\n");
}

[[maybe_unused]] static void braid_balance(TorusRuntime* runtimes) {
    uint32_t loads[kTorusCount] = {};
    for (uint32_t i = 0; i < kTorusCount; ++i) {
        loads[i] = runtimes[i].scheduler->getProcessCount();
    }

    uint32_t max_idx = 0;
    uint32_t min_idx = 0;
    for (uint32_t i = 1; i < kTorusCount; ++i) {
        if (loads[i] > loads[max_idx]) {
            max_idx = i;
        }
        if (loads[i] < loads[min_idx]) {
            min_idx = i;
        }
    }

    if (loads[max_idx] <= loads[min_idx] + 2) {
        return;
    }

    os::OSProcess* proc = runtimes[max_idx].scheduler->pickMigratableProcess();
    if (!proc) {
        return;
    }

    if (runtimes[min_idx].scheduler->receiveProcess(proc)) {
        serial_write("[RSE] braid migrate from ");
        serial_write_u64(max_idx);
        serial_write(" to ");
        serial_write_u64(min_idx);
        serial_write("\n");
    }
}

enum class OsBraidPhase {
    A_PROJECTS,
    B_PROJECTS,
    C_PROJECTS
};

static OsBraidPhase os_braid_phase = OsBraidPhase::A_PROJECTS;
static uint64_t os_braid_cycles;
static uint64_t os_last_migrate_cycle[kTorusCount];

static braided::Projection os_make_projection(uint32_t torus_id, const TorusRuntime& rt, uint64_t timestamp) {
    braided::Projection proj = {};
    proj.torus_id = torus_id;
    proj.timestamp = timestamp;
    proj.total_events_processed = rt.scheduler->getContextSwitches();
    proj.current_time = timestamp;
    proj.active_processes = rt.scheduler->getProcessCount();
    proj.pending_events = rt.scheduler->getReadyCount();
    proj.edge_count = rt.scheduler->getBlockedCount();
    TorusMetrics metrics = torus_metrics[torus_id];
    uint64_t block_cost = metrics.block_bytes ? (metrics.block_cycles / metrics.block_bytes) : 0;
    uint64_t net_cost = metrics.net_bytes ? (metrics.net_cycles / metrics.net_bytes) : 0;
    const uint32_t ready = proj.pending_events;
    const uint32_t blocked = proj.edge_count;
    const uint32_t active = proj.active_processes;
    for (size_t i = 0; i < braided::Projection::BOUNDARY_SIZE; ++i) {
        if (i < 256) {
            proj.boundary_states[i] = ready;
        } else if (i < 512) {
            proj.boundary_states[i] = blocked;
        } else {
            proj.boundary_states[i] = active;
        }
    }
    if (braided::Projection::BOUNDARY_SIZE >= 6) {
        proj.boundary_states[braided::Projection::BOUNDARY_SIZE - 1] = (uint32_t)(net_cost & 0xffffffffu);
        proj.boundary_states[braided::Projection::BOUNDARY_SIZE - 2] = (uint32_t)(block_cost & 0xffffffffu);
        proj.boundary_states[braided::Projection::BOUNDARY_SIZE - 3] = proj.active_processes;
        proj.boundary_states[braided::Projection::BOUNDARY_SIZE - 4] = proj.pending_events;
        proj.boundary_states[braided::Projection::BOUNDARY_SIZE - 5] = proj.edge_count;
        proj.boundary_states[braided::Projection::BOUNDARY_SIZE - 6] =
            (uint32_t)(proj.total_events_processed & 0xffffffffu);
    }
    proj.constraint_vector = {};
    proj.constraint_vector[0] = (int32_t)proj.active_processes;
    proj.constraint_vector[1] = (int32_t)proj.pending_events;
    proj.constraint_vector[2] = (int32_t)proj.edge_count;
    proj.constraint_vector[3] = (int32_t)(proj.total_events_processed & 0x7fffffff);
    proj.constraint_vector[4] = (int32_t)(proj.current_time & 0x7fffffff);
    proj.constraint_vector[5] = (int32_t)(block_cost & 0x7fffffff);
    proj.constraint_vector[6] = (int32_t)(net_cost & 0x7fffffff);
    proj.state_hash = proj.computeHash();
    return proj;
}

[[maybe_unused]] static uint64_t rse_projection_payload_hash(const braided::Projection& proj) {
    uint8_t buf[sizeof(braided::Projection)];
    uint32_t len = (uint32_t)proj.serialize(buf, sizeof(buf));
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < len; ++i) {
        hash ^= buf[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

#if RSE_NET_EXCHANGE
static void os_net_exchange(TorusRuntime* runtimes) {
    static uint64_t seq = 1;
    static bool inited = false;
    const uint32_t local_id = (RSE_TORUS_ID < kTorusCount) ? RSE_TORUS_ID : 0;

    if (!inited) {
        rsepx_init(local_id);
        inited = true;
    }

    braided::Projection proj = os_make_projection(local_id, runtimes[local_id], kernel_rdtsc());
    const uint64_t payload_hash = rse_projection_payload_hash(proj);
    uint8_t dst_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x00};
    uint32_t peer_id = (local_id == 0) ? 1 : 0;
    dst_mac[5] = (uint8_t)peer_id;

    serial_write("[RSE] net projection exchange start\n");
    serial_write("[RSE] net projection dst mac=");
    for (uint32_t i = 0; i < 6; ++i) {
        serial_write_u64(dst_mac[i]);
        if (i + 1 < 6) {
            serial_write(":");
        }
    }
    serial_write("\n");

    const uint64_t start = kernel_rdtsc();
    uint64_t last_send = 0;
    const uint64_t timeout_cycles = 5ULL * 1000ULL * 1000ULL * 1000ULL;
    const uint64_t resend_cycles = 200ULL * 1000ULL * 1000ULL;
    bool acked = false;
    bool received = false;
    while ((kernel_rdtsc() - start) < timeout_cycles) {
        const uint64_t now = kernel_rdtsc();
        if (!acked && (last_send == 0 || (now - last_send) > resend_cycles)) {
            if (rsepx_send_projection(proj, local_id, seq, dst_mac) == 0) {
                serial_write("[RSE] net projection sent seq=");
                serial_write_u64(seq);
                serial_write("\n");
            } else {
                serial_write("[RSE] net projection send failed\n");
            }
            last_send = now;
        }
        RsepxReceived recv{};
        int got = rsepx_poll(&recv);
        if (got <= 0) {
            continue;
        }
        if (recv.kind == RsepxReceived::Projection) {
            serial_write("[RSE] net projection recv torus=");
            serial_write_u64(recv.header.torus_id);
            serial_write(" seq=");
            serial_write_u64(recv.header.seq);
            serial_write("\n");
            rsepx_send_ack(recv.header.seq, recv.header.payload_hash, recv.src_mac);
            received = true;
        } else if (recv.kind == RsepxReceived::Ack) {
            if (recv.ack.seq == seq && recv.ack.payload_hash == payload_hash) {
                serial_write("[RSE] net projection ack seq=");
                serial_write_u64(seq);
                serial_write(" cycles=");
                serial_write_u64(kernel_rdtsc() - start);
                serial_write("\n");
                acked = true;
            }
        }
        if (acked && received) {
            break;
        }
    }

    if (!acked) {
        serial_write("[RSE] net projection ack timeout\n");
    }
    if (!received) {
        serial_write("[RSE] net projection recv timeout\n");
    }

    seq++;
}
#endif

#if RSE_SHM_EXCHANGE
extern "C" void* rse_ivshmem_base(uint64_t* size_out);

struct ShmRing {
    volatile uint64_t seq;
    volatile uint64_t payload_hash;
    volatile uint32_t payload_len;
    volatile uint32_t ready;
    uint8_t payload[sizeof(braided::Projection)];
};

struct ShmRegion {
    ShmRing ring[kTorusCount];
    volatile uint64_t ack[kTorusCount][kTorusCount];
};

static void os_shm_exchange(TorusRuntime* runtimes) {
    static ShmRegion* region = nullptr;
    static uint64_t seq = 1;
    static uint64_t last_seen[kTorusCount];
    const uint32_t local_id = (RSE_TORUS_ID < kTorusCount) ? RSE_TORUS_ID : 0;

    if (!region) {
        uint64_t size = 0;
        region = reinterpret_cast<ShmRegion*>(rse_ivshmem_base(&size));
        if (!region) {
            serial_write("[RSE] shm projection unavailable\n");
            return;
        }
        serial_write("[RSE] shm projection online\n");
    }

    braided::Projection proj = os_make_projection(local_id, runtimes[local_id], kernel_rdtsc());
    const uint64_t payload_hash = rse_projection_payload_hash(proj);
    const uint32_t payload_len = (uint32_t)sizeof(braided::Projection);

    serial_write("[RSE] shm projection exchange start\n");

    ShmRing* out = &region->ring[local_id];
    out->seq = seq;
    out->payload_hash = payload_hash;
    out->payload_len = payload_len;
    for (uint32_t i = 0; i < payload_len; ++i) {
        out->payload[i] = reinterpret_cast<const uint8_t*>(&proj)[i];
    }
    __asm__ volatile("mfence" ::: "memory");
    out->ready = 1;

    bool acked = false;
    bool received = false;
    uint64_t start = kernel_rdtsc();
    while (kernel_rdtsc() - start < 5ULL * 1000ULL * 1000ULL * 1000ULL) {
        bool all_acked = true;
        for (uint32_t peer = 0; peer < kTorusCount; ++peer) {
            if (peer == local_id) {
                continue;
            }
            if (region->ack[peer][local_id] != seq) {
                all_acked = false;
            }
        }
        if (!acked && all_acked) {
            serial_write("[RSE] shm projection acked seq=");
            serial_write_u64(seq);
            serial_write("\n");
            acked = true;
        }

        bool all_recv = true;
        for (uint32_t peer = 0; peer < kTorusCount; ++peer) {
            if (peer == local_id) {
                continue;
            }
            ShmRing* in = &region->ring[peer];
            if (!in->ready || in->seq == last_seen[peer]) {
                all_recv = false;
                continue;
            }
            __asm__ volatile("mfence" ::: "memory");
            braided::Projection peer_proj =
                braided::Projection::deserialize(in->payload, in->payload_len);
            if (peer_proj.verify()) {
                serial_write("[RSE] shm projection recv torus=");
                serial_write_u64(peer_proj.torus_id);
                serial_write(" seq=");
                serial_write_u64(in->seq);
                serial_write("\n");
            } else {
                serial_write("[RSE] shm projection recv invalid\n");
            }
            last_seen[peer] = in->seq;
            region->ack[local_id][peer] = in->seq;
            __asm__ volatile("mfence" ::: "memory");
        }
        if (!received && all_recv) {
            received = true;
        }
        if (acked && received) {
            break;
        }
    }

    if (!acked) {
        serial_write("[RSE] shm projection ack timeout\n");
    }
    if (!received) {
        serial_write("[RSE] shm projection recv timeout\n");
    }
    if (!acked || !received) {
        for (uint32_t peer = 0; peer < kTorusCount; ++peer) {
            if (peer == local_id) {
                continue;
            }
            ShmRing* in = &region->ring[peer];
            serial_write("[RSE] shm state peer=");
            serial_write_u64(peer);
            serial_write(" ready=");
            serial_write_u64(in->ready);
            serial_write(" seq=");
            serial_write_u64(in->seq);
            serial_write(" ack=");
            serial_write_u64(region->ack[peer][local_id]);
            serial_write("\n");
        }
    }
    seq++;
}
#endif

static void os_migrate_one(TorusRuntime* runtimes, uint32_t src, uint32_t dst) {
    if (os_last_migrate_cycle[src] == os_braid_cycles ||
        os_last_migrate_cycle[dst] == os_braid_cycles) {
        return;
    }
    os::OSProcess* proc = runtimes[src].scheduler->pickMigratableProcess();
    if (!proc) {
        return;
    }
    if (runtimes[dst].scheduler->receiveProcess(proc)) {
        os_last_migrate_cycle[src] = os_braid_cycles;
        os_last_migrate_cycle[dst] = os_braid_cycles;
        serial_write("[RSE] braid constraint migrate ");
        serial_write_u64(src);
        serial_write("->");
        serial_write_u64(dst);
        serial_write("\n");
    }
}

static void os_apply_constraints(TorusRuntime* runtimes, uint32_t src, uint32_t dst,
                                 const braided::Projection& proj) {
    uint32_t dst_load = runtimes[dst].scheduler->getProcessCount();
    uint32_t dst_ready = runtimes[dst].scheduler->getReadyCount();
    uint32_t dst_blocked = runtimes[dst].scheduler->getBlockedCount();
    uint32_t src_ready_est = proj.boundary_states[0];
    uint32_t src_blocked_est = proj.boundary_states[256];
    int32_t src_pressure = (int32_t)proj.pending_events - (int32_t)proj.edge_count;
    int32_t dst_pressure = (int32_t)dst_ready - (int32_t)dst_blocked;
    uint32_t dst_block_cost = 0;
    uint32_t dst_net_cost = 0;
    if (dst < kTorusCount) {
        TorusMetrics metrics = torus_metrics[dst];
        dst_block_cost = metrics.block_bytes ? (uint32_t)(metrics.block_cycles / metrics.block_bytes) : 0;
        dst_net_cost = metrics.net_bytes ? (uint32_t)(metrics.net_cycles / metrics.net_bytes) : 0;
    }
    uint32_t src_block_cost = (uint32_t)proj.constraint_vector[5];
    uint32_t src_net_cost = (uint32_t)proj.constraint_vector[6];
    if (dst_blocked > proj.edge_count + kBlockedSkew) {
        return;
    }
    if (proj.active_processes > dst_load + kLoadSkew) {
        os_migrate_one(runtimes, src, dst);
        return;
    }
    if (proj.pending_events > dst_ready + kReadySkew) {
        os_migrate_one(runtimes, src, dst);
        return;
    }
    if (src_block_cost > dst_block_cost + kCostSkew && dst_load + 1 < proj.active_processes) {
        os_migrate_one(runtimes, src, dst);
        return;
    }
    if (src_net_cost > dst_net_cost + kCostSkew && dst_load + 1 < proj.active_processes) {
        os_migrate_one(runtimes, src, dst);
        return;
    }
    if (src_ready_est > dst_ready + kBoundarySkew) {
        os_migrate_one(runtimes, src, dst);
        return;
    }
    if (src_blocked_est + kBoundarySkew < dst_blocked && dst_load > 2) {
        os_migrate_one(runtimes, dst, src);
        return;
    }
    if (src_pressure > dst_pressure + (int32_t)kPressureSkew) {
        os_migrate_one(runtimes, src, dst);
        return;
    }
    if (proj.edge_count + kBlockedSkew < dst_blocked && dst_load > 2) {
        os_migrate_one(runtimes, dst, src);
    }
}

static void os_braid_exchange(TorusRuntime* runtimes, uint64_t timestamp) {
    uint32_t src = 0;
    uint32_t dst_a = 1;
    uint32_t dst_b = 2;
    switch (os_braid_phase) {
        case OsBraidPhase::A_PROJECTS:
            src = 0;
            dst_a = 1;
            dst_b = 2;
            break;
        case OsBraidPhase::B_PROJECTS:
            src = 1;
            dst_a = 0;
            dst_b = 2;
            break;
        case OsBraidPhase::C_PROJECTS:
            src = 2;
            dst_a = 0;
            dst_b = 1;
            break;
    }

    braided::Projection proj = os_make_projection(src, runtimes[src], timestamp);
    if (!proj.verify()) {
        serial_write("[RSE] os braid projection invalid\n");
    }

    os_apply_constraints(runtimes, src, dst_a, proj);
    os_apply_constraints(runtimes, src, dst_b, proj);

    switch (os_braid_phase) {
        case OsBraidPhase::A_PROJECTS:
            os_braid_phase = OsBraidPhase::B_PROJECTS;
            break;
        case OsBraidPhase::B_PROJECTS:
            os_braid_phase = OsBraidPhase::C_PROJECTS;
            break;
        case OsBraidPhase::C_PROJECTS:
            os_braid_phase = OsBraidPhase::A_PROJECTS;
            os_braid_cycles++;
            break;
    }
}

extern "C" void rse_os_run(void) {
    if (g_runtimes_ready) {
        serial_write("[RSE] os runtime already initialized\n");
        return;
    }
    TorusRuntime* runtimes = g_runtimes;
    const bool has_block = (rse_block_init() == 0);
    const uint32_t block_size = has_block ? rse_block_size() : 0;
    const uint64_t block_total = has_block ? rse_block_total_blocks() : 0;
    const bool has_net = (rse_net_init() == 0);

    for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
        TorusRuntime& rt = runtimes[torus_id];
        rt.memfs = create_memfs(torus_id);
        rt.vfs = create_vfs(torus_id, rt.memfs);
        rt.blockfs = create_blockfs(torus_id);
        rt.dev_mgr = create_device_manager(torus_id);
        rt.console = nullptr;
        rt.fastio = nullptr;
        rt.scheduler = create_scheduler(torus_id);
        rt.dispatcher = create_dispatcher(torus_id);
        rt.phys_alloc = create_phys_alloc(torus_id);

        os::Device* console = os::create_console_device();
        os::Device* dev_null = os::create_null_device();
        os::Device* dev_zero = os::create_zero_device();
        rt.dev_mgr->registerDevice(console);
        rt.dev_mgr->registerDevice(dev_null);
        rt.dev_mgr->registerDevice(dev_zero);
        os::Device* dev_fast = os::create_fastpath_device("fast0");
        if (dev_fast) {
            rt.dev_mgr->registerDevice(dev_fast);
            rt.fastio = dev_fast;
        }
        if (has_block && block_size > 0) {
            os::Device* dev_blk = os::create_block_device("blk0", block_size);
            if (dev_blk) {
                rt.dev_mgr->registerDevice(dev_blk);
            }
            if (block_total > 0 && rt.blockfs) {
                if (rt.blockfs->mount(block_size, block_total)) {
                    rt.vfs->setBlockFS(rt.blockfs);
                } else {
                    serial_write("[RSE] BlockFS mount failed\n");
                }
            }
        }
        os::Device* dev_loop = os::create_loopback_device("loopback");
        if (dev_loop) {
            rt.dev_mgr->registerDevice(dev_loop);
        }
        if (has_net) {
            os::Device* dev_net = os::create_net_device("net0");
            if (dev_net) {
                rt.dev_mgr->registerDevice(dev_net);
            }
        }
        rt.vfs->setDeviceManager(rt.dev_mgr);
        rt.console = console;

        rt.ctx.scheduler = rt.scheduler;
        rt.ctx.dispatcher = rt.dispatcher;
        rt.ctx.vfs = rt.vfs;
        rt.ctx.phys_alloc = rt.phys_alloc;
        rt.ctx.next_pid = 1;

        os::OSProcess* init = create_process(torus_id, 0, rt.ctx.next_pid++, 0);
        init->initMemory(rt.phys_alloc);
        init->fd_table.bindStandardDevices(rt.console);
        rt.scheduler->addProcess(init);
        user_procs[torus_id][0] = init;
    }

    for (uint32_t i = 0; i < kExtraProcs; ++i) {
        TorusRuntime& rt = runtimes[0];
        os::OSProcess* extra = create_process(0, 1 + i, rt.ctx.next_pid++, 0);
        extra->initMemory(rt.phys_alloc);
        extra->fd_table.bindStandardDevices(rt.console);
        rt.scheduler->addProcess(extra);
        user_procs[0][1 + i] = extra;
    }

    for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
        TorusRuntime& rt = runtimes[torus_id];
        for (uint32_t slot = 0; slot < kRing3Slots; ++slot) {
            if (g_ring3_procs[torus_id][slot]) {
                continue;
            }
            g_ring3_procs[torus_id][slot] =
                create_ring3_process(torus_id, slot, rt.ctx.next_pid++, 0);
            g_ring3_procs[torus_id][slot]->initMemory(rt.phys_alloc);
            configure_ring3_memory(g_ring3_procs[torus_id][slot]);
            g_ring3_procs[torus_id][slot]->fd_table.bindStandardDevices(rt.console);
            rt.scheduler->addProcess(g_ring3_procs[torus_id][slot]);
            if (!install_init_elf(rt, g_ring3_procs[torus_id][slot])) {
                serial_write("[RSE] init elf install failed\n");
            }
            if (!load_init_elf_image(g_ring3_procs[torus_id][slot])) {
                serial_write("[RSE] init elf load failed\n");
            }
        }
        g_ring3_active[torus_id] = 0;
    }
    if (!g_ring3_proc) {
        g_ring3_proc = g_ring3_procs[0][0];
    }

    #if !RSE_ENABLE_USERMODE
    struct rse_syscalls sys = {
        .log = serial_write,
        .log_u64 = serial_write_u64,
        .rdtsc = kernel_rdtsc,
        .get_torus_id = rse_get_torus_id,
        .pipe_push = rse_pipe_push,
        .pipe_pop = rse_pipe_pop,
        .report_block = rse_report_block,
        .report_net = rse_report_net,
        .open = os_open_shim,
        .close = os_close_shim,
        .write = os_write_shim,
        .read = os_read_shim,
        .unlink = os_unlink_shim,
        .lseek = os_lseek_shim,
        .list = os_list_shim,
        .stat = os_stat_shim,
        .ps = os_ps_dump
    };

    for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
        for (uint32_t slot = 0; slot < (1 + kExtraProcs); ++slot) {
            if (!user_procs[torus_id][slot]) {
                continue;
            }
            UserProgramState* st = &user_states[torus_id][slot];
            st->phase = 0;
            st->ticks = 0;
            st->fd = -1;
            if (torus_id == 0 && slot == 0) {
                user_procs[torus_id][slot]->setUserEntry(user_program_fs, st, &sys);
            } else if (torus_id == 1 && slot == 0) {
                user_procs[torus_id][slot]->setUserEntry(user_program_net, st, &sys);
            } else {
                user_procs[torus_id][slot]->setUserEntry(user_program_compute, st, &sys);
            }
        }
    }

    for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
        serial_write("[RSE] torus init ");
        serial_write_u64(torus_id);
        serial_write("\n");
        current_torus_id = torus_id;
        os::current_torus_context = &runtimes[torus_id].ctx;
        runtimes[torus_id].scheduler->tick();
        init_main(&sys);
    }

    serial_write("[RSE] userspace run start\n");
    for (uint32_t step = 0; step < 48; ++step) {
        for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
            runtimes[torus_id].scheduler->tick();
        }
    }
    serial_write("[RSE] userspace run done\n");
    #endif

    serial_write("[RSE] braid scheduler start\n");
    braid_log_loads(runtimes);
    for (uint32_t step = 0; step < 6; ++step) {
        for (uint32_t torus_id = 0; torus_id < kTorusCount; ++torus_id) {
            runtimes[torus_id].scheduler->tick();
        }
        if ((step + 1) % 2 == 0) {
            os_braid_exchange(runtimes, kernel_rdtsc());
        }
    }
    braid_log_loads(runtimes);
    serial_write("[RSE] os braid cycles=");
    serial_write_u64(os_braid_cycles);
    serial_write("\n");
    serial_write("[RSE] braid scheduler done\n");

#if RSE_NET_EXCHANGE
    if (has_net) {
        os_net_exchange(runtimes);
    } else {
        serial_write("[RSE] net projection skipped (no net)\n");
    }
#endif

#if RSE_SHM_EXCHANGE
    os_shm_exchange(runtimes);
#endif

    g_runtimes_ready = 1;
}
