// Minimal Limine + UEFI kernel skeleton for RSE bootstrapping.
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <efi.h>
#include <efiprot.h>

#include "limine.h"
#include "rse_boot.h"
#include "rse_syscalls.h"

#ifndef RSE_ENABLE_USERMODE
#define RSE_ENABLE_USERMODE 0
#endif
#ifndef RSE_AUTO_EXIT
#define RSE_AUTO_EXIT 0
#endif

void *memcpy(void *dst, const void *src, size_t count);
void *memset(void *dst, int value, size_t count);
static void *uefi_alloc_pages(size_t bytes);
void serial_write(const char *s);
extern int rse_os_user_map(uint64_t code_vaddr, uint64_t stack_vaddr,
                           uint64_t *code_phys_out, uint64_t *stack_phys_out);
extern int rse_os_user_ranges(uint64_t *code_start, uint64_t *code_end,
                              uint64_t *data_start, uint64_t *data_end,
                              uint64_t *stack_start, uint64_t *stack_end);
extern uint64_t rse_os_user_translate(uint64_t vaddr);
extern int rse_os_prepare_ring3(uint32_t torus_id);
extern int64_t rse_os_syscall_dispatch(int64_t num,
                                       uint64_t arg1, uint64_t arg2,
                                       uint64_t arg3, uint64_t arg4,
                                       uint64_t arg5, uint64_t arg6);
extern int rse_os_ring3_entry(uint64_t *entry_out);
extern int rse_os_ring3_context(uint64_t *entry_out, uint64_t *stack_out);

#define RSE_SYS_EXEC 2u
#define RSE_SYS_EXIT 3u
#define RSE_SYS_WRITE 13u

__attribute__((used, section(".limine_reqs")))
LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_reqs")))
LIMINE_BASE_REVISION(2);

__attribute__((used, section(".limine_reqs")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_reqs")))
static volatile struct limine_bootloader_info_request bootloader_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_reqs")))
static volatile struct limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST,
    .revision = 0,
    .stack_size = 1024 * 1024
};

__attribute__((used, section(".limine_reqs")))
LIMINE_REQUESTS_END_MARKER;

static inline void hlt_loop(void) {
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void rse_poweroff(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    hlt_loop();
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static int serial_can_send(void) {
    return (inb(0x3F8 + 5) & 0x20) != 0;
}

static void serial_write_char(char c) {
    if (c == '\n') {
        serial_write_char('\r');
    }
    while (!serial_can_send()) {
    }
    outb(0x3F8, (uint8_t)c);
}

static void ui_console_write(const char *s);

void serial_write(const char *s) {
    if (!s) {
        return;
    }
    ui_console_write(s);
    while (*s) {
        serial_write_char(*s++);
    }
}

void serial_write_u64(uint64_t value) {
    char buf[21];
    int i = 0;
    if (value == 0) {
        serial_write("0");
        return;
    }
    while (value > 0 && i < (int)(sizeof(buf) - 1)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i-- > 0) {
        serial_write_char(buf[i]);
    }
}

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t gran;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

enum { GDT_ENTRY_COUNT = 8 };
static struct gdt_entry gdt_entries[GDT_ENTRY_COUNT];
static struct gdt_ptr gdt_descriptor;

enum {
    GDT_KERNEL_CODE = 0x08,
    GDT_KERNEL_DATA = 0x10,
    GDT_USER_CODE = 0x18,
    GDT_USER_DATA = 0x20,
    GDT_TSS = 0x28
};

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct tss64 g_tss;
static uint8_t g_user_kernel_stack[16384] __attribute__((aligned(16)));
static uint8_t *g_user_code_page;
static uint8_t *g_user_stack_page;
static uint64_t *g_user_pml4;
static uint64_t *g_user_pdpt;
static uint64_t *g_user_pd_kernel;
static uint64_t *g_user_pd_user;
static uint64_t *g_user_pt_user;
static uint64_t g_user_cr3;
static uint64_t g_saved_cr3;
static uint64_t g_saved_rbx;
static uint64_t g_saved_rbp;
static uint64_t g_saved_r12;
static uint64_t g_saved_r13;
static uint64_t g_saved_r14;
static uint64_t g_saved_r15;
static struct idt_entry g_idt[256];
static struct idt_ptr g_idt_desc;
static uint64_t g_user_mode_kernel_rsp;
static volatile int g_user_mode_exited;

static inline void read_idt(struct idt_ptr* out) {
    __asm__ volatile("sidt %0" : "=m"(*out));
}

static inline void load_idt(const struct idt_ptr* in) {
    __asm__ volatile("lidt %0" : : "m"(*in));
}

static inline uint64_t read_cr2(void) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline void write_cr3(uint64_t cr3) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static inline uint64_t read_rflags(void) {
    uint64_t flags = 0;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    return flags;
}

static inline void write_rflags(uint64_t flags) {
    __asm__ volatile("push %0; popfq" : : "r"(flags) : "memory");
}

static void gdt_set_entry(int idx, uint8_t access, uint8_t flags) {
    gdt_entries[idx].limit_low = 0;
    gdt_entries[idx].base_low = 0;
    gdt_entries[idx].base_mid = 0;
    gdt_entries[idx].access = access;
    gdt_entries[idx].gran = flags;
    gdt_entries[idx].base_high = 0;
}

static void gdt_set_tss_descriptor(int idx, uint64_t base, uint32_t limit) {
    gdt_entries[idx].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt_entries[idx].base_low = (uint16_t)(base & 0xFFFF);
    gdt_entries[idx].base_mid = (uint8_t)((base >> 16) & 0xFF);
    gdt_entries[idx].access = 0x89;
    gdt_entries[idx].gran = (uint8_t)((limit >> 16) & 0x0F);
    gdt_entries[idx].base_high = (uint8_t)((base >> 24) & 0xFF);

    uint8_t* high = (uint8_t*)&gdt_entries[idx + 1];
    uint64_t base_high = base >> 32;
    high[0] = (uint8_t)(base_high & 0xFF);
    high[1] = (uint8_t)((base_high >> 8) & 0xFF);
    high[2] = (uint8_t)((base_high >> 16) & 0xFF);
    high[3] = (uint8_t)((base_high >> 24) & 0xFF);
    high[4] = 0;
    high[5] = 0;
    high[6] = 0;
    high[7] = 0;
}

static void set_idt_entry(int vec, void (*handler)(void), uint8_t type_attr) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    g_idt[vec].offset_low = (uint16_t)(addr & 0xFFFF);
    g_idt[vec].selector = GDT_KERNEL_CODE;
    g_idt[vec].ist = 0;
    g_idt[vec].type_attr = type_attr;
    g_idt[vec].offset_mid = (uint16_t)((addr >> 16) & 0xFFFF);
    g_idt[vec].offset_high = (uint32_t)(addr >> 32);
    g_idt[vec].zero = 0;
}

struct int80_frame {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, cs, rflags, rsp, ss;
};

static bool build_user_page_table(uint64_t code_phys, uint64_t stack_phys);

#define USER_VADDR_BASE 0x40000000ull
#define USER_STACK_VADDR (USER_VADDR_BASE + 0x1000ull)
#define USER_STACK_TOP (USER_VADDR_BASE + 0x2000ull)

struct exc_frame {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

__attribute__((used)) static void user_mode_return_cont(void) {
}

__attribute__((used)) static void user_mode_fault_return_cont(void) {
}

__attribute__((naked)) static void user_mode_return(void) {
    __asm__ volatile(
        "movl $1, g_user_mode_exited(%rip)\n"
        "mov g_user_mode_kernel_rsp(%rip), %rsp\n"
        "mov g_saved_rbx(%rip), %rbx\n"
        "mov g_saved_rbp(%rip), %rbp\n"
        "mov g_saved_r12(%rip), %r12\n"
        "mov g_saved_r13(%rip), %r13\n"
        "mov g_saved_r14(%rip), %r14\n"
        "mov g_saved_r15(%rip), %r15\n"
        "jmp user_mode_return_cont\n");
}

__attribute__((naked)) static void user_mode_fault_return(void) {
    __asm__ volatile(
        "movl $-1, g_user_mode_exited(%rip)\n"
        "mov g_user_mode_kernel_rsp(%rip), %rsp\n"
        "mov g_saved_rbx(%rip), %rbx\n"
        "mov g_saved_rbp(%rip), %rbp\n"
        "mov g_saved_r12(%rip), %r12\n"
        "mov g_saved_r13(%rip), %r13\n"
        "mov g_saved_r14(%rip), %r14\n"
        "mov g_saved_r15(%rip), %r15\n"
        "jmp user_mode_fault_return_cont\n");
}

__attribute__((used)) static void int80_handler(struct int80_frame* frame) {
    if (!frame) {
        return;
    }
    uint64_t call = frame->rax;
    if (call == 0) {
        serial_write("[RSE] user syscall ping\n");
        frame->rax = 0;
        return;
    }
    if (call == 1) {
        frame->cs = GDT_KERNEL_CODE;
        frame->ss = GDT_KERNEL_DATA;
        frame->rip = (uint64_t)(uintptr_t)user_mode_return;
        frame->rsp = g_user_mode_kernel_rsp;
        return;
    }

    int64_t rc = rse_os_syscall_dispatch((int64_t)call,
                                         frame->rdi, frame->rsi, frame->rdx,
                                         frame->r10, frame->r8, frame->r9);
    frame->rax = (uint64_t)rc;
    if (call == RSE_SYS_EXIT) {
        frame->cs = GDT_KERNEL_CODE;
        frame->ss = GDT_KERNEL_DATA;
        frame->rip = (uint64_t)(uintptr_t)user_mode_return;
        frame->rsp = g_user_mode_kernel_rsp;
        return;
    }
    if (call == RSE_SYS_EXEC && rc == 0) {
        uint64_t entry = 0;
        uint64_t stack = 0;
        if (rse_os_ring3_context(&entry, &stack)) {
            uint64_t code_phys = 0;
            uint64_t stack_phys = 0;
            uint64_t stack_page = USER_STACK_VADDR;
            if (stack > 8) {
                stack_page = stack - 8u;
            }
            if (rse_os_user_map(entry, stack_page, &code_phys, &stack_phys)) {
                build_user_page_table(code_phys, stack_phys);
            }
            frame->rip = entry;
            frame->rsp = stack ? stack : USER_STACK_TOP;
        }
    }
}

static void exception_dump(const char *label, struct exc_frame *frame, uint64_t cr2) {
    if (!frame) {
        return;
    }
    serial_write("[RSE] ");
    serial_write(label);
    serial_write(" fault\n");
    serial_write("  rip=");
    serial_write_u64(frame->rip);
    serial_write(" cs=");
    serial_write_u64(frame->cs);
    serial_write(" err=");
    serial_write_u64(frame->error_code);
    if (cr2) {
        serial_write(" cr2=");
        serial_write_u64(cr2);
    }
    serial_write("\n");
}

static void exception_return_to_kernel(struct exc_frame *frame) {
    if (!frame) {
        return;
    }
    if ((frame->cs & 0x3) != 0x3) {
        hlt_loop();
        return;
    }
    frame->cs = GDT_KERNEL_CODE;
    frame->ss = GDT_KERNEL_DATA;
    frame->rip = (uint64_t)(uintptr_t)user_mode_fault_return;
    frame->rsp = g_user_mode_kernel_rsp;
}

__attribute__((used)) static void gp_handler(struct exc_frame *frame) {
    exception_dump("#GP", frame, 0);
    exception_return_to_kernel(frame);
}

__attribute__((used)) static void pf_handler(struct exc_frame *frame) {
    uint64_t cr2 = read_cr2();
    exception_dump("#PF", frame, cr2);
    exception_return_to_kernel(frame);
}

__attribute__((naked, unused)) static void int80_stub(void) {
    __asm__ volatile(
        "push %r15\n"
        "push %r14\n"
        "push %r13\n"
        "push %r12\n"
        "push %r11\n"
        "push %r10\n"
        "push %r9\n"
        "push %r8\n"
        "push %rbp\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %rcx\n"
        "push %rbx\n"
        "push %rax\n"
        "mov %rsp, %rdi\n"
        "call int80_handler\n"
        "pop %rax\n"
        "pop %rbx\n"
        "pop %rcx\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rbp\n"
        "pop %r8\n"
        "pop %r9\n"
        "pop %r10\n"
        "pop %r11\n"
        "pop %r12\n"
        "pop %r13\n"
        "pop %r14\n"
        "pop %r15\n"
        "iretq\n");
}

__attribute__((naked, unused)) static void gp_stub(void) {
    __asm__ volatile(
        "push %r15\n"
        "push %r14\n"
        "push %r13\n"
        "push %r12\n"
        "push %r11\n"
        "push %r10\n"
        "push %r9\n"
        "push %r8\n"
        "push %rbp\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %rcx\n"
        "push %rbx\n"
        "push %rax\n"
        "mov %rsp, %rdi\n"
        "call gp_handler\n"
        "pop %rax\n"
        "pop %rbx\n"
        "pop %rcx\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rbp\n"
        "pop %r8\n"
        "pop %r9\n"
        "pop %r10\n"
        "pop %r11\n"
        "pop %r12\n"
        "pop %r13\n"
        "pop %r14\n"
        "pop %r15\n"
        "add $8, %rsp\n"
        "iretq\n");
}

__attribute__((naked, unused)) static void pf_stub(void) {
    __asm__ volatile(
        "push %r15\n"
        "push %r14\n"
        "push %r13\n"
        "push %r12\n"
        "push %r11\n"
        "push %r10\n"
        "push %r9\n"
        "push %r8\n"
        "push %rbp\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %rcx\n"
        "push %rbx\n"
        "push %rax\n"
        "mov %rsp, %rdi\n"
        "call pf_handler\n"
        "pop %rax\n"
        "pop %rbx\n"
        "pop %rcx\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rbp\n"
        "pop %r8\n"
        "pop %r9\n"
        "pop %r10\n"
        "pop %r11\n"
        "pop %r12\n"
        "pop %r13\n"
        "pop %r14\n"
        "pop %r15\n"
        "add $8, %rsp\n"
        "iretq\n");
}

__attribute__((naked, unused)) static void ignore_stub(void) {
    __asm__ volatile("iretq\n");
}

__attribute__((naked, unused)) static void ignore_err_stub(void) {
    __asm__ volatile("add $8, %rsp\n"
                     "iretq\n");
}

__attribute__((naked, unused)) static void irq_stub(void) {
    __asm__ volatile(
        "push %rax\n"
        "mov $0x20, %al\n"
        "outb %al, $0x20\n"
        "outb %al, $0xA0\n"
        "pop %rax\n"
        "iretq\n");
}

#define PTE_PRESENT 0x1ull
#define PTE_RW 0x2ull
#define PTE_USER 0x4ull
#define PTE_PS 0x80ull
#define PTE_NX (1ull << 63)
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ull

static void map_user_pt_entry(uint64_t vaddr, uint64_t phys, uint64_t flags) {
    uint64_t idx = (vaddr >> 12) & 0x1FFu;
    g_user_pt_user[idx] = (phys & PTE_ADDR_MASK) | flags;
}

static void map_user_range(uint64_t start, uint64_t end, uint64_t flags) {
    if (start >= end) {
        return;
    }
    uint64_t v = start & ~(0xFFFULL);
    uint64_t v_end = (end + 0xFFFULL) & ~(0xFFFULL);
    const uint64_t user_max = USER_VADDR_BASE + 0x200000ull;
    for (; v < v_end; v += 0x1000ULL) {
        if (v < USER_VADDR_BASE || v >= user_max) {
            continue;
        }
        uint64_t phys = rse_os_user_translate(v);
        if (!phys) {
            continue;
        }
        map_user_pt_entry(v, phys, flags);
    }
}

static bool build_user_page_table(uint64_t code_phys, uint64_t stack_phys) {
    if (!g_user_pml4) {
        g_user_pml4 = (uint64_t *)uefi_alloc_pages(4096u);
    }
    if (!g_user_pdpt) {
        g_user_pdpt = (uint64_t *)uefi_alloc_pages(4096u);
    }
    if (!g_user_pd_kernel) {
        g_user_pd_kernel = (uint64_t *)uefi_alloc_pages(4096u);
    }
    if (!g_user_pd_user) {
        g_user_pd_user = (uint64_t *)uefi_alloc_pages(4096u);
    }
    if (!g_user_pt_user) {
        g_user_pt_user = (uint64_t *)uefi_alloc_pages(4096u);
    }
    if (!g_user_pml4 || !g_user_pdpt || !g_user_pd_kernel || !g_user_pd_user ||
        !g_user_pt_user) {
        return false;
    }

    memset(g_user_pml4, 0, 4096u);
    memset(g_user_pdpt, 0, 4096u);
    memset(g_user_pd_kernel, 0, 4096u);
    memset(g_user_pd_user, 0, 4096u);
    memset(g_user_pt_user, 0, 4096u);

    g_user_pml4[0] = ((uint64_t)(uintptr_t)g_user_pdpt & PTE_ADDR_MASK) |
                     PTE_PRESENT | PTE_RW | PTE_USER;
    g_user_pdpt[0] = ((uint64_t)(uintptr_t)g_user_pd_kernel & PTE_ADDR_MASK) |
                     PTE_PRESENT | PTE_RW;
    g_user_pdpt[1] = ((uint64_t)(uintptr_t)g_user_pd_user & PTE_ADDR_MASK) |
                     PTE_PRESENT | PTE_RW | PTE_USER;

    for (uint64_t i = 0; i < 512; ++i) {
        uint64_t addr = i * 0x200000ull;
        g_user_pd_kernel[i] = (addr & PTE_ADDR_MASK) | PTE_PRESENT | PTE_RW | PTE_PS;
    }

    uint64_t user_pd_idx = (USER_VADDR_BASE >> 21) & 0x1FFu;
    g_user_pd_user[user_pd_idx] =
        ((uint64_t)(uintptr_t)g_user_pt_user & PTE_ADDR_MASK) |
        PTE_PRESENT | PTE_RW | PTE_USER;

    uint64_t code_start = 0;
    uint64_t code_end = 0;
    uint64_t data_start = 0;
    uint64_t data_end = 0;
    uint64_t stack_start = 0;
    uint64_t stack_end = 0;
    if (rse_os_user_ranges(&code_start, &code_end, &data_start, &data_end,
                           &stack_start, &stack_end)) {
        map_user_range(code_start, code_end, PTE_PRESENT | PTE_USER);
        map_user_range(data_start, data_end, PTE_PRESENT | PTE_USER | PTE_RW | PTE_NX);
        map_user_range(stack_start, stack_end, PTE_PRESENT | PTE_USER | PTE_RW | PTE_NX);
    }

    uint64_t code_idx = (USER_VADDR_BASE >> 12) & 0x1FFu;
    uint64_t stack_idx = (USER_STACK_VADDR >> 12) & 0x1FFu;

    g_user_pt_user[code_idx] = (code_phys & PTE_ADDR_MASK) |
                               PTE_PRESENT | PTE_USER;
    g_user_pt_user[stack_idx] = (stack_phys & PTE_ADDR_MASK) |
                                PTE_PRESENT | PTE_RW | PTE_USER | PTE_NX;

    g_user_cr3 = (uint64_t)(uintptr_t)g_user_pml4 & PTE_ADDR_MASK;
    return true;
}

__attribute__((unused)) static void init_idt(void) {
    memset(g_idt, 0, sizeof(g_idt));
    for (int i = 0; i < 256; ++i) {
        set_idt_entry(i, ignore_stub, 0x8E);
    }
    set_idt_entry(0x08, ignore_err_stub, 0x8E);
    set_idt_entry(0x0A, ignore_err_stub, 0x8E);
    set_idt_entry(0x0B, ignore_err_stub, 0x8E);
    set_idt_entry(0x0C, ignore_err_stub, 0x8E);
    for (int vec = 0x20; vec <= 0x2F; ++vec) {
        set_idt_entry(vec, irq_stub, 0x8E);
    }
    set_idt_entry(0x80, int80_stub, 0xEE);
    set_idt_entry(0x0D, gp_stub, 0x8E);
    set_idt_entry(0x0E, pf_stub, 0x8E);
    set_idt_entry(0x11, ignore_err_stub, 0x8E);
    set_idt_entry(0x1E, ignore_err_stub, 0x8E);
    g_idt_desc.limit = (uint16_t)(sizeof(g_idt) - 1);
    g_idt_desc.base = (uint64_t)(uintptr_t)&g_idt[0];
    __asm__ volatile("lidt %0" : : "m"(g_idt_desc));
}

static void init_tss(void) {
    memset(&g_tss, 0, sizeof(g_tss));
    g_tss.rsp0 = (uint64_t)(uintptr_t)g_user_kernel_stack + sizeof(g_user_kernel_stack);
    g_tss.iomap_base = sizeof(g_tss);
    gdt_set_tss_descriptor(5, (uint64_t)(uintptr_t)&g_tss, sizeof(g_tss) - 1);
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS));
}

static uint16_t read_cs(void) {
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void init_gdt_user_segments(void) {
    uint16_t cs = read_cs();
    uint16_t cs_index = (uint16_t)(cs >> 3);

    gdt_set_entry(0, 0x00, 0x00);
    gdt_set_entry(1, 0x9A, 0x20);  // Kernel code, long mode
    gdt_set_entry(2, 0x92, 0x00);  // Kernel data
    gdt_set_entry(3, 0xFA, 0x20);  // User code, long mode
    gdt_set_entry(4, 0xF2, 0x00);  // User data

    gdt_descriptor.limit = (uint16_t)(sizeof(gdt_entries) - 1);
    gdt_descriptor.base = (uint64_t)(uintptr_t)&gdt_entries[0];

    if (cs_index > 0 && cs_index < GDT_ENTRY_COUNT &&
        cs_index != (GDT_KERNEL_CODE >> 3) &&
        cs_index != (GDT_USER_CODE >> 3)) {
        gdt_set_entry((int)cs_index, 0x9A, 0x20);
    }

    __asm__ volatile("lgdt %0" : : "m"(gdt_descriptor));

    __asm__ volatile(
        "pushq %[cs]\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : [cs] "i"(GDT_KERNEL_CODE)
        : "rax", "memory");

    uint16_t data_sel = GDT_KERNEL_DATA;
    __asm__ volatile(
        "mov %0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        :
        : "r"(data_sel)
        : "ax");

    init_tss();
    serial_write("[RSE] GDT user segments installed\n");
}

static inline uint64_t rdtsc(void) {
    uint32_t lo;
    uint32_t hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

uint64_t kernel_rdtsc(void) {
    return rdtsc();
}

__attribute__((naked, unused)) static void user_mode_entry(void) {
    __asm__ volatile(
        "mov $0, %rax\n"
        "int $0x80\n"
        "mov $1, %rax\n"
        "int $0x80\n"
        "hlt\n");
}

static bool setup_user_pages(uint64_t *entry_out, uint64_t *stack_out) {
    static const uint8_t user_code_ping[] = {
        0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00,
        0xCD, 0x80,
        0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,
        0xCD, 0x80,
        0xF4
    };
    __attribute__((unused)) static const uint8_t user_code_syscall[] = {
        0x48, 0xC7, 0xC0, RSE_SYS_WRITE, 0x00, 0x00, 0x00,
        0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x35, 0x1A, 0x00, 0x00, 0x00,
        0x48, 0xC7, 0xC2, 0x10, 0x00, 0x00, 0x00,
        0xCD, 0x80,
        0x48, 0xC7, 0xC0, RSE_SYS_EXIT, 0x00, 0x00, 0x00,
        0x48, 0xC7, 0xC7, 0x00, 0x00, 0x00, 0x00,
        0xCD, 0x80,
        0xF4,
        'u','s','e','r',' ','s','y','s','c','a','l','l',' ','o','k','\n'
    };
    static const uint8_t user_code_exec[] = {
        0x48, 0xC7, 0xC0, RSE_SYS_EXEC, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x3D, 0x15, 0x00, 0x00, 0x00,
        0x48, 0x31, 0xF6,
        0x48, 0x31, 0xD2,
        0xCD, 0x80,
        0x48, 0xC7, 0xC0, RSE_SYS_EXIT, 0x00, 0x00, 0x00,
        0x48, 0x31, 0xFF,
        0xCD, 0x80,
        0xF4,
        '/', 'r', 'i', 'n', 'g', '3', '.', 'e', 'l', 'f', 0x00
    };

    serial_write("[RSE] user setup start\n");
    if (!entry_out || !stack_out) {
        return false;
    }

    const uint8_t* user_code = user_code_ping;
    size_t user_code_len = sizeof(user_code_ping);
    if (rse_os_prepare_ring3(0)) {
        user_code = user_code_exec;
        user_code_len = sizeof(user_code_exec);
        serial_write("[RSE] user setup ring3 ready\n");
    } else {
        serial_write("[RSE] user setup ring3 unavailable\n");
    }

    uint64_t os_code_phys = 0;
    uint64_t os_stack_phys = 0;
    if (rse_os_user_map(USER_VADDR_BASE, USER_STACK_VADDR, &os_code_phys, &os_stack_phys)) {
        g_user_code_page = (uint8_t *)(uintptr_t)os_code_phys;
        g_user_stack_page = (uint8_t *)(uintptr_t)os_stack_phys;
        serial_write("[RSE] user setup os map ok\n");
    } else {
        serial_write("[RSE] user setup os map failed\n");
    }

    if (!g_user_code_page) {
        g_user_code_page = (uint8_t *)uefi_alloc_pages(4096u);
    }
    if (!g_user_stack_page) {
        g_user_stack_page = (uint8_t *)uefi_alloc_pages(4096u);
    }

    if (!g_user_code_page || !g_user_stack_page) {
        serial_write("[RSE] user pages alloc failed\n");
        return false;
    }

    memset(g_user_code_page, 0, 4096u);
    memcpy(g_user_code_page, user_code, user_code_len);
    memset(g_user_stack_page, 0, 4096u);

    uint64_t code_phys = (uint64_t)(uintptr_t)g_user_code_page;
    uint64_t stack_phys = (uint64_t)(uintptr_t)g_user_stack_page;
    if (!build_user_page_table(code_phys, stack_phys)) {
        serial_write("[RSE] user page table build failed\n");
        return false;
    }

    *entry_out = USER_VADDR_BASE;
    *stack_out = USER_STACK_TOP;
    return true;
}

__attribute__((noinline, unused)) static void enter_user_mode(uint64_t entry, uint64_t user_stack) {
    uint64_t rflags = 0;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    rflags &= ~0x200;
    uint64_t cs = GDT_USER_CODE | 0x3;
    uint64_t ss = GDT_USER_DATA | 0x3;
    __asm__ volatile(
        "pushq %0\n"
        "pushq %1\n"
        "pushq %2\n"
        "pushq %3\n"
        "pushq %4\n"
        "iretq\n"
        :
        : "r"(ss), "r"(user_stack), "r"(rflags), "r"(cs), "r"(entry)
        : "memory");
}

__attribute__((unused)) static void run_user_mode_smoke(void) {
    uint64_t entry = 0;
    uint64_t user_stack = 0;
    if (!setup_user_pages(&entry, &user_stack)) {
        serial_write("[RSE] user mode setup failed\n");
        return;
    }
    g_user_mode_exited = 0;
    serial_write("[RSE] user mode smoke begin\n");
    {
        uint64_t kernel_rsp = 0;
        __asm__ volatile("mov %%rsp, %0" : "=r"(kernel_rsp));
        g_user_mode_kernel_rsp = kernel_rsp - 8u;
    }
    __asm__ volatile(
        "mov %%rbx, %0\n"
        "mov %%rbp, %1\n"
        "mov %%r12, %2\n"
        "mov %%r13, %3\n"
        "mov %%r14, %4\n"
        "mov %%r15, %5\n"
        : "=m"(g_saved_rbx),
          "=m"(g_saved_rbp),
          "=m"(g_saved_r12),
          "=m"(g_saved_r13),
          "=m"(g_saved_r14),
          "=m"(g_saved_r15)
        :
        : "memory");
    g_saved_cr3 = read_cr3();
    write_cr3(g_user_cr3);
    enter_user_mode(entry, user_stack);
    write_cr3(g_saved_cr3);
    if (g_user_mode_exited == 1) {
        serial_write("[RSE] user mode smoke ok\n");
    } else if (g_user_mode_exited < 0) {
        serial_write("[RSE] user mode smoke fault\n");
    } else {
        serial_write("[RSE] user mode smoke exit missing\n");
    }
}

#if RSE_ENABLE_USERMODE
static void run_user_mode_smoke_guarded(void) {
    struct idt_ptr saved = {};
    uint64_t flags = read_rflags();
    __asm__ volatile("cli");
    read_idt(&saved);
    init_idt();
    run_user_mode_smoke();
    load_idt(&saved);
    write_rflags(flags);
}
#endif

static uint64_t xorshift64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

struct event {
    uint64_t value;
    uint32_t state;
    uint32_t pad;
};

#define EVENT_COUNT 100000U
#define EVENT_ITERS 4U
#define MEM_BYTES (4u * 1024u * 1024u)

static struct event events[EVENT_COUNT];
static uint8_t mem_a[MEM_BYTES];
static uint8_t mem_b[MEM_BYTES];

#define RAMFS_MAX_FILES 128U
#define RAMFS_NAME_MAX  32U
#define RAMFS_FILE_SIZE 4096U

#define KFD_MAX 64U

#define O_RDONLY 0x0000u
#define O_WRONLY 0x0001u
#define O_RDWR   0x0002u
#define O_CREAT  0x0040u
#define O_TRUNC  0x0200u
#define O_APPEND 0x0400u

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define PCI_STATUS_CAP_LIST 0x10
#define PCI_CAP_ID_VNDR 0x09

#define VIRTIO_PCI_VENDOR 0x1AF4
#define VIRTIO_PCI_DEVICE_BLK_LEGACY 0x1001
#define VIRTIO_PCI_DEVICE_BLK_TRANSITIONAL 0x1042
#define VIRTIO_PCI_DEVICE_NET_LEGACY 0x1000
#define VIRTIO_PCI_DEVICE_NET_TRANSITIONAL 0x1041

#define VIRTIO_PCI_HOST_FEATURES 0x0
#define VIRTIO_PCI_GUEST_FEATURES 0x4
#define VIRTIO_PCI_QUEUE_PFN 0x8
#define VIRTIO_PCI_QUEUE_NUM 0xC
#define VIRTIO_PCI_QUEUE_SEL 0xE
#define VIRTIO_PCI_QUEUE_NOTIFY 0x10
#define VIRTIO_PCI_STATUS 0x12
#define VIRTIO_PCI_ISR 0x13
#define VIRTIO_PCI_CONFIG 0x14
#define VIRTIO_PCI_GUEST_PAGE_SIZE 0x28

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG 3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4

#define VIRTIO_STATUS_ACK 0x01
#define VIRTIO_STATUS_DRIVER 0x02
#define VIRTIO_STATUS_DRIVER_OK 0x04
#define VIRTIO_STATUS_FEATURES_OK 0x08
#define VIRTIO_STATUS_FAILED 0x80
#define VIRTIO_MSI_NO_VECTOR 0xFFFF

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2

#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1

#define VIRTQ_MAX 256
#define VIRTIO_NET_QUEUE_RX 0
#define VIRTIO_NET_QUEUE_TX 1
#define VIRTIO_NET_MAX_Q 256
#define VIRTIO_NET_BUF_SIZE 2048

#define VIRTIO_NET_HDR_BASE_SIZE 10
#define VIRTIO_NET_HDR_MRG_SIZE 12
#define VIRTIO_NET_F_MAC (1u << 5)
#define VIRTIO_NET_F_MRG_RXBUF (1u << 15)
#define VIRTIO_F_VERSION_1 (1u << 0)

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
};

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

struct virtio_net_hdr_mrg {
    struct virtio_net_hdr base;
    uint16_t num_buffers;
} __attribute__((packed));

struct virtio_pci_cap {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t id;
    uint8_t padding[2];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed));

struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_avail;
    uint64_t queue_used;
} __attribute__((packed));

static uint8_t virtq_area_static[4096 * 4] __attribute__((aligned(4096)));
static uint8_t *virtq_area = virtq_area_static;
static size_t virtq_area_len = sizeof(virtq_area_static);
static struct virtq_desc *virtq_desc_table;
static volatile struct virtq_avail *virtq_avail_ring;
static volatile struct virtq_used *virtq_used_ring;
static struct virtio_blk_req virtio_req_static __attribute__((aligned(8)));
static uint8_t virtio_status_static;
static struct virtio_blk_req *virtio_req_ptr = &virtio_req_static;
static volatile uint8_t *virtio_status_ptr = &virtio_status_static;
static uint8_t *virtio_blk_dma_buf;
static uint32_t virtio_io_base;
static uint16_t virtio_used_idx;
static uint16_t virtq_size;

static uint8_t virtio_net_rx_area_static[4096 * 4] __attribute__((aligned(4096)));
static uint8_t virtio_net_tx_area_static[4096 * 2] __attribute__((aligned(4096)));
static uint8_t *virtio_net_rx_area = virtio_net_rx_area_static;
static size_t virtio_net_rx_area_len = sizeof(virtio_net_rx_area_static);
static uint8_t *virtio_net_tx_area = virtio_net_tx_area_static;
static size_t virtio_net_tx_area_len = sizeof(virtio_net_tx_area_static);
static struct virtq_desc *net_rx_desc;
static volatile struct virtq_avail *net_rx_avail;
static volatile struct virtq_used *net_rx_used;
static struct virtq_desc *net_tx_desc;
static volatile struct virtq_avail *net_tx_avail;
static volatile struct virtq_used *net_tx_used;
static uint16_t net_rx_used_idx;
static uint16_t net_tx_used_idx;
static uint16_t net_rx_qsz;
static uint16_t net_tx_qsz;
static uint16_t net_tx_slots;
static uint32_t virtio_net_io_base;
static uint8_t net_rx_bufs_static[VIRTIO_NET_MAX_Q][VIRTIO_NET_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t net_tx_bufs_static[VIRTIO_NET_MAX_Q][VIRTIO_NET_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t *net_rx_bufs = (uint8_t *)net_rx_bufs_static;
static uint8_t *net_tx_bufs = (uint8_t *)net_tx_bufs_static;
static struct virtio_net_hdr_mrg net_tx_hdrs_static[VIRTIO_NET_MAX_Q] __attribute__((aligned(16)));
static struct virtio_net_hdr_mrg *net_tx_hdrs = net_tx_hdrs_static;
static uint16_t virtio_net_hdr_len = VIRTIO_NET_HDR_BASE_SIZE;
static uint8_t virtio_net_mrg_rxbuf;
static uint8_t virtio_net_mac[6];
static int virtio_net_mac_valid;

static volatile struct virtio_pci_common_cfg *virtio_blk_common;
static volatile uint8_t *virtio_blk_isr;
static volatile uint8_t *virtio_blk_notify;
static volatile uint8_t *virtio_blk_device;
static uint32_t virtio_blk_notify_mult;
static uint16_t virtio_blk_notify_off;
static uint8_t virtio_blk_use_modern;

static volatile struct virtio_pci_common_cfg *virtio_net_common;
static volatile uint8_t *virtio_net_isr;
static volatile uint8_t *virtio_net_notify;
static volatile uint8_t *virtio_net_device;
static uint32_t virtio_net_notify_mult;
static uint16_t virtio_net_notify_off_rx;
static uint16_t virtio_net_notify_off_tx;
static uint8_t virtio_net_use_modern;


struct ramfs_file {
    char name[RAMFS_NAME_MAX];
    uint8_t data[RAMFS_FILE_SIZE];
    uint32_t size;
    uint8_t in_use;
};

static struct ramfs_file ramfs_files[RAMFS_MAX_FILES];

struct kernel_fd {
    int in_use;
    int file_idx;
    uint32_t offset;
    uint32_t flags;
};

static struct kernel_fd kfd_table[KFD_MAX];

static EFI_SYSTEM_TABLE *get_system_table(struct rse_boot_info *boot_info) {
    if (!boot_info || boot_info->magic != RSE_BOOT_MAGIC) {
        return NULL;
    }
    return (EFI_SYSTEM_TABLE *)(uintptr_t)boot_info->system_table;
}

static EFI_BLOCK_IO_PROTOCOL *uefi_find_raw_block(EFI_SYSTEM_TABLE *st);

static struct rse_boot_info *g_boot_info;
static struct limine_framebuffer *g_framebuffer;
static struct limine_framebuffer g_uefi_framebuffer;
static EFI_BLOCK_IO_PROTOCOL *g_block_io;
static UINTN g_block_size;
static EFI_SIMPLE_NETWORK_PROTOCOL *g_net;
static const EFI_GUID g_net_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
static EFI_SIMPLE_POINTER_PROTOCOL *g_pointer;
static const EFI_GUID g_pointer_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
static int virtio_net_init(void);
static int net_init_uefi(void);
static int virtio_net_send(const void *buf, uint32_t len);
static int virtio_net_recv(void *buf, uint32_t len);

struct rse_bench_metrics {
    uint64_t compute_ops;
    uint64_t compute_cycles;
    uint64_t compute_cycles_per_op;
    uint64_t memory_bytes;
    uint64_t memory_cycles;
    uint64_t memory_cycles_per_byte;
    uint64_t ramfs_ops;
    uint64_t ramfs_cycles;
    uint64_t ramfs_cycles_per_op;
    uint64_t uefi_fs_ops;
    uint64_t uefi_fs_cycles;
    uint64_t uefi_fs_cycles_per_op;
    uint64_t uefi_blk_bytes;
    uint64_t uefi_blk_write_cycles;
    uint64_t uefi_blk_read_cycles;
    uint64_t uefi_blk_write_cycles_per_byte;
    uint64_t uefi_blk_read_cycles_per_byte;
    uint64_t virtio_blk_bytes;
    uint64_t virtio_blk_write_cycles;
    uint64_t virtio_blk_read_cycles;
    uint64_t virtio_blk_write_cycles_per_byte;
    uint64_t virtio_blk_read_cycles_per_byte;
    uint64_t net_arp_bytes;
    uint64_t net_arp_cycles;
    uint64_t udp_rx;
    uint64_t udp_udp;
    uint64_t udp_http;
    uint64_t udp_cycles;
    uint64_t http_requests;
    uint64_t http_cycles;
    uint64_t http_cycles_per_req;
    uint8_t virtio_blk_present;
    uint8_t metrics_valid;
};

static struct rse_bench_metrics g_metrics;
static int g_os_initialized;

enum ui_action {
    UI_ACT_NONE = 0,
    UI_ACT_BENCH = 1,
    UI_ACT_NET = 2,
    UI_ACT_RESET = 3
};

struct ui_icon {
    const char *label;
    enum ui_action action;
    size_t x;
    size_t y;
    size_t w;
    size_t h;
};

static struct ui_icon g_icons[3];
static int g_ui_hover = -1;
static size_t g_cursor_x = 16;
static size_t g_cursor_y = 16;

static const uint32_t UI_BG = 0x00101820;
static const uint32_t UI_BAR = 0x00203040;
static const uint32_t UI_PANEL = 0x0018242f;
static const uint32_t UI_PANEL_ALT = 0x00141a22;
static const uint32_t UI_ACCENT = 0x0033ccff;
static const uint32_t UI_TEXT = 0x00f5f7ff;
static const uint32_t UI_MUTED = 0x00a0b4c8;
static const uint32_t UI_OK = 0x0066f0a8;
static const uint32_t UI_WARN = 0x00ffb347;
static const uint32_t UI_TICK_USEC = 10000;
static const uint32_t UI_DBLCLICK_TICKS = 25;
static const int64_t UI_POINTER_DIV = 4;
enum { UI_CONSOLE_LINES = 9, UI_CONSOLE_COLS = 48 };

static char g_console[UI_CONSOLE_LINES][UI_CONSOLE_COLS + 1];
static int g_console_line;
static int g_console_col;
static int g_console_count;
static int g_console_inited;

enum net_backend {
    NET_BACKEND_NONE = 0,
    NET_BACKEND_VIRTIO = 1,
    NET_BACKEND_UEFI = 2
};

static void *uefi_alloc_pages(size_t bytes) {
    EFI_SYSTEM_TABLE *st = get_system_table(g_boot_info);
    if (!st || !st->BootServices || bytes == 0) {
        return NULL;
    }
    UINTN pages = (bytes + 4095u) / 4096u;
    EFI_PHYSICAL_ADDRESS addr = 0;
    EFI_STATUS status = st->BootServices->AllocatePages(
        AllocateAnyPages, EfiBootServicesData, pages, &addr);
    if (EFI_ERROR(status) || addr == 0) {
        return NULL;
    }
    memset((void *)(uintptr_t)addr, 0, pages * 4096u);
    return (void *)(uintptr_t)addr;
}

static enum net_backend g_net_backend = NET_BACKEND_NONE;
static uint8_t net_ip_addr[4] = {10, 0, 2, 15};
static uint16_t net_udp_port = 40000;
static uint16_t net_http_port = 8080;
static uint8_t net_mac_addr[6];
static int net_mac_valid;

#ifndef RSE_NET_RAW
#define RSE_NET_RAW 0
#endif

#define NET_QUEUE_DEPTH 32
#define NET_PAYLOAD_MAX 1500
struct net_payload {
    uint32_t len;
    uint8_t data[NET_PAYLOAD_MAX];
};
static struct net_payload net_queue[NET_QUEUE_DEPTH];
static uint32_t net_queue_head;
static uint32_t net_queue_tail;
static uint32_t net_queue_count;

static int net_backend_write(const void *buf, uint32_t len);
static int net_backend_read(void *buf, uint32_t len);
static void net_poll_rx(uint32_t budget);
static int net_udp_send(const uint8_t *payload, uint32_t len);
static uint32_t net_queue_pop(uint8_t *buf, uint32_t max_len);

int rse_block_init(void) {
    if (g_block_io) {
        return 0;
    }
    EFI_SYSTEM_TABLE *st = get_system_table(g_boot_info);
    if (!st) {
        serial_write("[RSE] UEFI block unavailable (no system table)\n");
        return -1;
    }
    EFI_BLOCK_IO_PROTOCOL *blk = uefi_find_raw_block(st);
    if (!blk) {
        serial_write("[RSE] UEFI block unavailable (no raw device)\n");
        return -1;
    }
    g_block_io = blk;
    g_block_size = blk->Media->BlockSize;
    return 0;
}

uint32_t rse_block_size(void) {
    if (!g_block_io) {
        return 0;
    }
    return (uint32_t)g_block_size;
}

uint64_t rse_block_total_blocks(void) {
    if (!g_block_io) {
        if (rse_block_init() != 0) {
            return 0;
        }
    }
    if (!g_block_io || !g_block_io->Media) {
        return 0;
    }
    return (uint64_t)g_block_io->Media->LastBlock + 1;
}

int rse_block_read(uint64_t lba, void *buf, uint32_t blocks) {
    if (!g_block_io) {
        if (rse_block_init() != 0) {
            return -1;
        }
    }
    if (!buf || blocks == 0) {
        return -1;
    }
    UINTN bytes = (UINTN)blocks * g_block_size;
    EFI_STATUS st = g_block_io->ReadBlocks(g_block_io, g_block_io->Media->MediaId,
                                           (EFI_LBA)lba, bytes, buf);
    return st == EFI_SUCCESS ? 0 : -1;
}

int rse_block_write(uint64_t lba, const void *buf, uint32_t blocks) {
    if (!g_block_io) {
        if (rse_block_init() != 0) {
            return -1;
        }
    }
    if (!buf || blocks == 0) {
        return -1;
    }
    UINTN bytes = (UINTN)blocks * g_block_size;
    EFI_STATUS st = g_block_io->WriteBlocks(g_block_io, g_block_io->Media->MediaId,
                                            (EFI_LBA)lba, bytes, (void *)buf);
    return st == EFI_SUCCESS ? 0 : -1;
}

static int net_init_uefi(void) {
    if (g_net_backend == NET_BACKEND_UEFI && g_net) {
        return 0;
    }
    EFI_SYSTEM_TABLE *st = get_system_table(g_boot_info);
    if (!st || !st->BootServices) {
        serial_write("[RSE] UEFI net unavailable (no system table)\n");
        return -1;
    }
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = st->BootServices->LocateHandleBuffer(ByProtocol,
                                                             (EFI_GUID *)&g_net_guid,
                                                             NULL,
                                                             &handle_count,
                                                             &handles);
    if (EFI_ERROR(status) || handle_count == 0) {
        serial_write("[RSE] UEFI net unavailable (no handles)\n");
        return -1;
    }
    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_SIMPLE_NETWORK_PROTOCOL *snp = NULL;
        status = st->BootServices->HandleProtocol(handles[i],
                                                  (EFI_GUID *)&g_net_guid,
                                                  (void **)&snp);
        if (!EFI_ERROR(status) && snp) {
            g_net = snp;
            break;
        }
    }
    if (!g_net) {
        serial_write("[RSE] UEFI net unavailable (protocol not found)\n");
        return -1;
    }
    if (g_net->Mode && g_net->Mode->State == EfiSimpleNetworkStopped) {
        status = g_net->Start(g_net);
        if (EFI_ERROR(status)) {
            serial_write("[RSE] UEFI net start failed\n");
            g_net = NULL;
            return -1;
        }
    }
    if (g_net->Mode && g_net->Mode->State == EfiSimpleNetworkStarted) {
        status = g_net->Initialize(g_net, 0, 0);
        if (EFI_ERROR(status)) {
            serial_write("[RSE] UEFI net init failed\n");
            g_net = NULL;
            return -1;
        }
    }
    serial_write("[RSE] UEFI net online\n");
    g_net_backend = NET_BACKEND_UEFI;
    return 0;
}

int rse_net_init(void) {
    if (g_net_backend != NET_BACKEND_NONE) {
        return 0;
    }
    if (virtio_net_init() == 0) {
        g_net_backend = NET_BACKEND_VIRTIO;
        serial_write("[RSE] virtio-net online\n");
        return 0;
    }
    return net_init_uefi();
}

int rse_net_write(const void *buf, uint32_t len) {
    if (!buf || len == 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_NONE && rse_net_init() != 0) {
        return -1;
    }
#if RSE_NET_RAW
    return net_backend_write(buf, len);
#else
    return net_udp_send((const uint8_t *)buf, len);
#endif
}

int rse_net_read(void *buf, uint32_t len) {
    if (!buf || len == 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_NONE && rse_net_init() != 0) {
        return -1;
    }
#if RSE_NET_RAW
    return net_backend_read(buf, len);
#else
    uint32_t got = net_queue_pop((uint8_t *)buf, len);
    if (got > 0) {
        return (int)got;
    }
    net_poll_rx(8);
    got = net_queue_pop((uint8_t *)buf, len);
    return (int)got;
#endif
}

int rse_net_get_mac(uint8_t mac_out[6]) {
    if (!mac_out) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_NONE && rse_net_init() != 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_VIRTIO) {
        if (!virtio_net_mac_valid) {
            return -1;
        }
        for (uint32_t i = 0; i < 6; ++i) {
            mac_out[i] = virtio_net_mac[i];
        }
        return 0;
    }
    if (!g_net || !g_net->Mode) {
        return -1;
    }
    const UINT32 addr_size = g_net->Mode->HwAddressSize;
    if (addr_size < 6) {
        return -1;
    }
    for (uint32_t i = 0; i < 6; ++i) {
        mac_out[i] = g_net->Mode->CurrentAddress.Addr[i];
    }
    return 0;
}

static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *uefi_find_writable_fs(EFI_SYSTEM_TABLE *st) {
    if (!st || !st->BootServices) {
        return NULL;
    }
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID blk_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = st->BootServices->LocateHandleBuffer(
        ByProtocol, &fs_guid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handle_count == 0) {
        return NULL;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *found = NULL;
    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        EFI_STATUS fs_status = st->BootServices->HandleProtocol(
            handles[i], &fs_guid, (void **)&fs);
        if (EFI_ERROR(fs_status) || !fs) {
            continue;
        }

        EFI_BLOCK_IO_PROTOCOL *blk = NULL;
        EFI_STATUS blk_status = st->BootServices->HandleProtocol(
            handles[i], &blk_guid, (void **)&blk);
        if (EFI_ERROR(blk_status) || !blk || !blk->Media) {
            continue;
        }
        if (blk->Media->ReadOnly) {
            continue;
        }

        found = fs;
        break;
    }

    st->BootServices->FreePool(handles);
    return found;
}

static EFI_BLOCK_IO_PROTOCOL *uefi_find_raw_block(EFI_SYSTEM_TABLE *st) {
    if (!st || !st->BootServices) {
        return NULL;
    }
    EFI_GUID blk_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = st->BootServices->LocateHandleBuffer(
        ByProtocol, &blk_guid, NULL, &handle_count, &handles);
    if (EFI_ERROR(status) || handle_count == 0) {
        return NULL;
    }

    EFI_BLOCK_IO_PROTOCOL *found = NULL;
    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_BLOCK_IO_PROTOCOL *blk = NULL;
        EFI_STATUS blk_status = st->BootServices->HandleProtocol(
            handles[i], &blk_guid, (void **)&blk);
        if (EFI_ERROR(blk_status) || !blk || !blk->Media) {
            continue;
        }
        if (!blk->Media->MediaPresent || blk->Media->ReadOnly) {
            continue;
        }
        if (blk->Media->LogicalPartition) {
            continue;
        }
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        EFI_STATUS fs_status = st->BootServices->HandleProtocol(
            handles[i], &fs_guid, (void **)&fs);
        if (!EFI_ERROR(fs_status) && fs) {
            continue;
        }
        found = blk;
        break;
    }

    st->BootServices->FreePool(handles);
    return found;
}

static void format_filename16(CHAR16 *buf, uint32_t index) {
    buf[0] = L'\\';
    buf[1] = L'f';
    buf[2] = L'i';
    buf[3] = L'l';
    buf[4] = L'e';
    buf[5] = (CHAR16)(L'0' + ((index / 1000) % 10));
    buf[6] = (CHAR16)(L'0' + ((index / 100) % 10));
    buf[7] = (CHAR16)(L'0' + ((index / 10) % 10));
    buf[8] = (CHAR16)(L'0' + (index % 10));
    buf[9] = L'.';
    buf[10] = L'b';
    buf[11] = L'i';
    buf[12] = L'n';
    buf[13] = L'\0';
}

static void ramfs_reset(void) {
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        ramfs_files[i].in_use = 0;
        ramfs_files[i].size = 0;
    }
}

static void ramfs_copy_name(char *dst, const char *src) {
    uint32_t i = 0;
    for (; src[i] && i < (RAMFS_NAME_MAX - 1); ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int ramfs_name_equal(const char *a, const char *b) {
    uint32_t i = 0;
    while (a[i] || b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static int ramfs_find(const char *name) {
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (ramfs_files[i].in_use && ramfs_name_equal(ramfs_files[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

static int ramfs_create(const char *name) {
    int existing = ramfs_find(name);
    if (existing >= 0) {
        return existing;
    }
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (!ramfs_files[i].in_use) {
            ramfs_files[i].in_use = 1;
            ramfs_files[i].size = 0;
            ramfs_copy_name(ramfs_files[i].name, name);
            return (int)i;
        }
    }
    return -1;
}

static uint32_t ramfs_write(int idx, const uint8_t *data, uint32_t len) {
    if (idx < 0 || (uint32_t)idx >= RAMFS_MAX_FILES) {
        return 0;
    }
    struct ramfs_file *file = &ramfs_files[idx];
    if (!file->in_use) {
        return 0;
    }
    if (len > RAMFS_FILE_SIZE) {
        len = RAMFS_FILE_SIZE;
    }
    memcpy(file->data, data, len);
    file->size = len;
    return len;
}

static uint32_t ramfs_read(int idx, uint8_t *out, uint32_t len) {
    if (idx < 0 || (uint32_t)idx >= RAMFS_MAX_FILES) {
        return 0;
    }
    struct ramfs_file *file = &ramfs_files[idx];
    if (!file->in_use) {
        return 0;
    }
    if (len > file->size) {
        len = file->size;
    }
    memcpy(out, file->data, len);
    return len;
}

static void ramfs_delete(int idx) {
    if (idx < 0 || (uint32_t)idx >= RAMFS_MAX_FILES) {
        return;
    }
    ramfs_files[idx].in_use = 0;
    ramfs_files[idx].size = 0;
}

static void ramfs_truncate(int idx) {
    if (idx < 0 || (uint32_t)idx >= RAMFS_MAX_FILES) {
        return;
    }
    ramfs_files[idx].size = 0;
}

static uint32_t ramfs_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < RAMFS_MAX_FILES; ++i) {
        if (ramfs_files[i].in_use) {
            ++count;
        }
    }
    return count;
}

static void kfd_reset(void) {
    for (uint32_t i = 0; i < KFD_MAX; ++i) {
        kfd_table[i].in_use = 0;
        kfd_table[i].file_idx = -1;
        kfd_table[i].offset = 0;
        kfd_table[i].flags = 0;
    }
    for (uint32_t i = 0; i < 3 && i < KFD_MAX; ++i) {
        kfd_table[i].in_use = 1;
    }
}

static int __attribute__((unused)) ksys_open(const char *name, uint32_t flags) {
    if (!name) {
        return -1;
    }
    int idx = ramfs_find(name);
    if (idx < 0) {
        if (flags & O_CREAT) {
            idx = ramfs_create(name);
        }
    }
    if (idx < 0) {
        return -1;
    }
    if (flags & O_TRUNC) {
        ramfs_truncate(idx);
    }
    for (uint32_t fd = 3; fd < KFD_MAX; ++fd) {
        if (!kfd_table[fd].in_use) {
            kfd_table[fd].in_use = 1;
            kfd_table[fd].file_idx = idx;
            kfd_table[fd].offset = (flags & O_APPEND) ? ramfs_files[idx].size : 0;
            kfd_table[fd].flags = flags;
            return (int)fd;
        }
    }
    return -1;
}

static int __attribute__((unused)) ksys_close(int fd) {
    if (fd < 0 || (uint32_t)fd >= KFD_MAX) {
        return -1;
    }
    if (fd < 3) {
        return 0;
    }
    kfd_table[fd].in_use = 0;
    kfd_table[fd].file_idx = -1;
    kfd_table[fd].offset = 0;
    kfd_table[fd].flags = 0;
    return 0;
}

static int __attribute__((unused)) ksys_write(int fd, const uint8_t *buf, uint32_t len) {
    if (fd < 0 || (uint32_t)fd >= KFD_MAX || !kfd_table[fd].in_use) {
        return -1;
    }
    int idx = kfd_table[fd].file_idx;
    if (idx < 0) {
        return -1;
    }
    uint32_t written = ramfs_write(idx, buf, len);
    kfd_table[fd].offset += written;
    return (int)written;
}

static int __attribute__((unused)) ksys_read(int fd, uint8_t *buf, uint32_t len) {
    if (fd < 0 || (uint32_t)fd >= KFD_MAX || !kfd_table[fd].in_use) {
        return -1;
    }
    int idx = kfd_table[fd].file_idx;
    if (idx < 0) {
        return -1;
    }
    uint32_t read = ramfs_read(idx, buf, len);
    kfd_table[fd].offset += read;
    return (int)read;
}

static int __attribute__((unused)) ksys_unlink(const char *name) {
    int idx = ramfs_find(name);
    if (idx < 0) {
        return -1;
    }
    ramfs_delete(idx);
    return 0;
}

static uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(1u << 31) |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

static uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, slot, func, offset);
    uint32_t shift = (offset & 3) * 8;
    return (uint8_t)((value >> shift) & 0xFFu);
}

static uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, slot, func, offset);
    return (uint16_t)((value >> ((offset & 2) * 8)) & 0xFFFF);
}

static void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t addr = (uint32_t)(1u << 31) |
                    ((uint32_t)bus << 16) |
                    ((uint32_t)slot << 11) |
                    ((uint32_t)func << 8) |
                    (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, addr);
    uint32_t current = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 2) * 8;
    current &= ~(0xFFFFu << shift);
    current |= ((uint32_t)value << shift);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, current);
}

static uint64_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar, uint8_t *is_io) {
    uint32_t bar_low = pci_config_read32(bus, slot, func, (uint8_t)(0x10 + bar * 4));
    if (bar_low & 0x1) {
        if (is_io) {
            *is_io = 1;
        }
        return (uint64_t)(bar_low & ~0x3u);
    }
    if (is_io) {
        *is_io = 0;
    }
    uint32_t type = (bar_low >> 1) & 0x3u;
    uint64_t addr = (uint64_t)(bar_low & ~0xFu);
    if (type == 0x2) {
        uint32_t bar_high = pci_config_read32(bus, slot, func, (uint8_t)(0x10 + (bar + 1) * 4));
        addr |= ((uint64_t)bar_high << 32);
    }
    return addr;
}

static int virtio_pci_find_caps(uint8_t bus, uint8_t slot, uint8_t func,
                                volatile struct virtio_pci_common_cfg **common_out,
                                volatile uint8_t **notify_out,
                                uint32_t *notify_mult_out,
                                volatile uint8_t **isr_out,
                                volatile uint8_t **device_out) {
    uint16_t status = pci_config_read16(bus, slot, func, 0x06);
    (void)status;
    uint8_t cap_ptr = pci_config_read8(bus, slot, func, 0x34);
    volatile struct virtio_pci_common_cfg *common = NULL;
    volatile uint8_t *notify = NULL;
    volatile uint8_t *isr = NULL;
    volatile uint8_t *device = NULL;
    uint32_t notify_mult = 0;

    while (cap_ptr != 0) {
        uint8_t cap_id = pci_config_read8(bus, slot, func, cap_ptr);
        uint8_t cap_next = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 1));
        if (cap_id == PCI_CAP_ID_VNDR) {
            struct virtio_pci_cap cap;
            cap.cap_vndr = cap_id;
            cap.cap_next = cap_next;
            cap.cap_len = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 2));
            cap.cfg_type = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 3));
            cap.bar = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 4));
            cap.id = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 5));
            cap.padding[0] = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 6));
            cap.padding[1] = pci_config_read8(bus, slot, func, (uint8_t)(cap_ptr + 7));
            cap.offset = pci_config_read32(bus, slot, func, (uint8_t)(cap_ptr + 8));
            cap.length = pci_config_read32(bus, slot, func, (uint8_t)(cap_ptr + 12));

            uint8_t is_io = 0;
            uint64_t bar_base = pci_read_bar(bus, slot, func, cap.bar, &is_io);
            if (!is_io && bar_base != 0) {
                volatile uint8_t *base = (volatile uint8_t *)(uintptr_t)(bar_base + cap.offset);
                switch (cap.cfg_type) {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                    common = (volatile struct virtio_pci_common_cfg *)base;
                    break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    notify = base;
                    notify_mult = pci_config_read32(bus, slot, func, (uint8_t)(cap_ptr + 16));
                    if (notify_mult == 0) {
                        notify_mult = 4;
                    }
                    break;
                case VIRTIO_PCI_CAP_ISR_CFG:
                    isr = base;
                    break;
                case VIRTIO_PCI_CAP_DEVICE_CFG:
                    device = base;
                    break;
                default:
                    break;
                }
            }
        }
        cap_ptr = cap_next;
    }

    if (!common || !notify || !isr) {
        return -1;
    }
    *common_out = common;
    *notify_out = notify;
    *notify_mult_out = notify_mult;
    *isr_out = isr;
    *device_out = device;
    return 0;
}

static int virtio_find_blk_legacy(void) {
    for (uint8_t bus = 0; bus < 32; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    continue;
                }
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor != VIRTIO_PCI_VENDOR) {
                    continue;
                }
                if (device != VIRTIO_PCI_DEVICE_BLK_LEGACY &&
                    device != VIRTIO_PCI_DEVICE_BLK_TRANSITIONAL) {
                    continue;
                }

                uint16_t command = pci_config_read16(bus, slot, func, 0x04);
                command |= 0x0005;
                pci_config_write16(bus, slot, func, 0x04, command);

                uint32_t bar0 = pci_config_read32(bus, slot, func, 0x10);
                if ((bar0 & 0x1) == 0) {
                    continue;
                }
                virtio_io_base = bar0 & ~0x3u;
                return 0;
            }
        }
    }
    return -1;
}

static int virtio_find_blk_modern(void) {
    for (uint8_t bus = 0; bus < 32; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    continue;
                }
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor != VIRTIO_PCI_VENDOR) {
                    continue;
                }
                if (device != VIRTIO_PCI_DEVICE_BLK_TRANSITIONAL &&
                    device != VIRTIO_PCI_DEVICE_BLK_LEGACY) {
                    continue;
                }

                uint16_t command = pci_config_read16(bus, slot, func, 0x04);
                command |= 0x0006;
                pci_config_write16(bus, slot, func, 0x04, command);

                if (virtio_pci_find_caps(bus, slot, func,
                                         &virtio_blk_common,
                                         &virtio_blk_notify,
                                         &virtio_blk_notify_mult,
                                         &virtio_blk_isr,
                                         &virtio_blk_device) == 0) {
                    virtio_blk_notify_off = 0;
                    return 0;
                }
                serial_write("[RSE] virtio-blk modern caps missing bus=");
                serial_write_u64(bus);
                serial_write(" slot=");
                serial_write_u64(slot);
                serial_write(" func=");
                serial_write_u64(func);
                serial_write("\n");
            }
        }
    }
    return -1;
}

static void virtio_reset(void) {
    outb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS), 0);
}

static int virtio_setup_queue(void) {
    outw((uint16_t)(virtio_io_base + VIRTIO_PCI_QUEUE_SEL), 0);
    uint16_t qsz = inw((uint16_t)(virtio_io_base + VIRTIO_PCI_QUEUE_NUM));
    if (qsz == 0) {
        return -1;
    }
    if (qsz > VIRTQ_MAX) {
        serial_write("[RSE] virtio queue too large\n");
        serial_write("[RSE] virtio queue size=");
        serial_write_u64(qsz);
        serial_write("\n");
        return -1;
    }
    virtq_size = qsz;

    virtio_used_idx = 0;

    if (!virtq_area || virtq_area_len == 0) {
        return -1;
    }
    memset(virtq_area, 0, virtq_area_len);
    uintptr_t base = (uintptr_t)virtq_area;
    virtq_desc_table = (struct virtq_desc *)base;

    size_t desc_size = sizeof(struct virtq_desc) * virtq_size;
    uintptr_t avail_addr = base + desc_size;
    avail_addr = (avail_addr + 1) & ~((uintptr_t)1);
    virtq_avail_ring = (volatile struct virtq_avail *)avail_addr;

    size_t avail_size = sizeof(uint16_t) * 2 + sizeof(uint16_t) * virtq_size;
    uintptr_t used_addr = avail_addr + avail_size;
    used_addr = (used_addr + 3) & ~((uintptr_t)3);
    virtq_used_ring = (volatile struct virtq_used *)used_addr;

    size_t used_size = sizeof(uint16_t) * 2 + sizeof(struct virtq_used_elem) * virtq_size;
    uintptr_t end_addr = used_addr + used_size;
    if (end_addr > base + virtq_area_len) {
        serial_write("[RSE] virtio queue memory too small\n");
        return -1;
    }

    virtq_avail_ring->idx = 0;
    virtq_avail_ring->flags = 0;
    virtq_used_ring->idx = 0;
    virtq_used_ring->flags = 0;
    virtio_used_idx = virtq_used_ring->idx;

    uintptr_t queue_addr = base;
    if (queue_addr == 0) {
        return -1;
    }

    uint32_t pfn = (uint32_t)(queue_addr >> 12);
    serial_write("[RSE] virtio queue pfn=");
    serial_write_u64(pfn);
    serial_write(" qsz=");
    serial_write_u64(virtq_size);
    serial_write(" base=");
    serial_write_u64(queue_addr);
    serial_write(" avail=");
    serial_write_u64((uint64_t)(uintptr_t)virtq_avail_ring);
    serial_write(" used=");
    serial_write_u64((uint64_t)(uintptr_t)virtq_used_ring);
    serial_write("\n");
    outl((uint16_t)(virtio_io_base + VIRTIO_PCI_QUEUE_PFN), pfn);
    return 0;
}

static int virtio_init_legacy(void) {
    if (virtio_find_blk_legacy() != 0) {
        return -1;
    }
    serial_write("[RSE] virtio io=");
    serial_write_u64(virtio_io_base);
    serial_write("\n");
    uint32_t host_features = inl((uint16_t)(virtio_io_base + VIRTIO_PCI_HOST_FEATURES));
    serial_write("[RSE] virtio host features=");
    serial_write_u64(host_features);
    serial_write("\n");
    virtio_reset();
    outb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS), VIRTIO_STATUS_ACK);
    outb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS), VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    outl((uint16_t)(virtio_io_base + VIRTIO_PCI_GUEST_FEATURES), 0);
    outl((uint16_t)(virtio_io_base + VIRTIO_PCI_GUEST_PAGE_SIZE), 4096);
    outb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS),
         VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    uint64_t isr = inb((uint16_t)(virtio_io_base + VIRTIO_PCI_ISR));
    serial_write("[RSE] virtio isr=");
    serial_write_u64(isr);
    serial_write("\n");
    if (virtq_area == virtq_area_static) {
        void *dma_area = uefi_alloc_pages(4096u * 8u);
        if (dma_area) {
            virtq_area = (uint8_t *)dma_area;
            virtq_area_len = 4096u * 8u;
        }
    }
    if (virtio_req_ptr == &virtio_req_static) {
        void *req_page = uefi_alloc_pages(4096u);
        if (req_page) {
            virtio_req_ptr = (struct virtio_blk_req *)req_page;
            virtio_status_ptr = (volatile uint8_t *)((uint8_t *)req_page + 128);
        }
    }
    if (!virtio_blk_dma_buf) {
        void *data_page = uefi_alloc_pages(4096u);
        if (data_page) {
            virtio_blk_dma_buf = (uint8_t *)data_page;
        }
    }
    if (virtio_setup_queue() != 0) {
        outb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS), VIRTIO_STATUS_FAILED);
        return -1;
    }
    outb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS),
         VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
             VIRTIO_STATUS_DRIVER_OK);
    uint8_t status = inb((uint16_t)(virtio_io_base + VIRTIO_PCI_STATUS));
    serial_write("[RSE] virtio status=");
    serial_write_u64(status);
    serial_write("\n");
    uint32_t capacity_lo = inl((uint16_t)(virtio_io_base + VIRTIO_PCI_CONFIG));
    uint32_t capacity_hi = inl((uint16_t)(virtio_io_base + VIRTIO_PCI_CONFIG + 4));
    serial_write("[RSE] virtio-blk capacity=");
    serial_write_u64(((uint64_t)capacity_hi << 32) | capacity_lo);
    serial_write("\n");
    return 0;
}

static int virtio_setup_queue_modern(uint16_t qsz) {
    if (qsz == 0 || qsz > VIRTQ_MAX) {
        return -1;
    }
    virtq_size = qsz;
    virtio_used_idx = 0;
    if (!virtq_area || virtq_area_len == 0) {
        return -1;
    }
    memset(virtq_area, 0, virtq_area_len);
    uintptr_t base = (uintptr_t)virtq_area;
    virtq_desc_table = (struct virtq_desc *)base;

    size_t desc_size = sizeof(struct virtq_desc) * virtq_size;
    uintptr_t avail_addr = base + desc_size;
    avail_addr = (avail_addr + 1) & ~((uintptr_t)1);
    virtq_avail_ring = (volatile struct virtq_avail *)avail_addr;

    size_t avail_size = sizeof(uint16_t) * 2 + sizeof(uint16_t) * virtq_size;
    uintptr_t used_addr = avail_addr + avail_size;
    used_addr = (used_addr + 3) & ~((uintptr_t)3);
    virtq_used_ring = (volatile struct virtq_used *)used_addr;

    size_t used_size = sizeof(uint16_t) * 2 + sizeof(struct virtq_used_elem) * virtq_size;
    uintptr_t end_addr = used_addr + used_size;
    if (end_addr > base + virtq_area_len) {
        serial_write("[RSE] virtio queue memory too small\n");
        return -1;
    }

    virtq_avail_ring->idx = 0;
    virtq_avail_ring->flags = 0;
    virtq_used_ring->idx = 0;
    virtq_used_ring->flags = 0;
    virtio_used_idx = virtq_used_ring->idx;
    return 0;
}

static int virtio_init_modern(void) {
    if (virtio_find_blk_modern() != 0 || !virtio_blk_common) {
        return -1;
    }
    virtio_blk_common->device_status = 0;
    __asm__ volatile("mfence" ::: "memory");
    virtio_blk_common->device_status = VIRTIO_STATUS_ACK;
    virtio_blk_common->device_status |= VIRTIO_STATUS_DRIVER;

    virtio_blk_common->driver_feature_select = 0;
    virtio_blk_common->driver_feature = 0;
    virtio_blk_common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    if ((virtio_blk_common->device_status & VIRTIO_STATUS_FEATURES_OK) == 0) {
        return -1;
    }

    if (virtq_area == virtq_area_static) {
        void *dma_area = uefi_alloc_pages(4096u * 8u);
        if (dma_area) {
            virtq_area = (uint8_t *)dma_area;
            virtq_area_len = 4096u * 8u;
        }
    }
    if (virtio_req_ptr == &virtio_req_static) {
        void *req_page = uefi_alloc_pages(4096u);
        if (req_page) {
            virtio_req_ptr = (struct virtio_blk_req *)req_page;
            virtio_status_ptr = (volatile uint8_t *)((uint8_t *)req_page + 128);
        }
    }
    if (!virtio_blk_dma_buf) {
        void *data_page = uefi_alloc_pages(4096u);
        if (data_page) {
            virtio_blk_dma_buf = (uint8_t *)data_page;
        }
    }

    virtio_blk_common->queue_select = 0;
    uint16_t qsz = virtio_blk_common->queue_size;
    if (virtio_setup_queue_modern(qsz) != 0) {
        return -1;
    }
    virtio_blk_common->queue_size = qsz;
    virtio_blk_common->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
    virtio_blk_common->queue_desc = (uint64_t)(uintptr_t)virtq_desc_table;
    virtio_blk_common->queue_avail = (uint64_t)(uintptr_t)virtq_avail_ring;
    virtio_blk_common->queue_used = (uint64_t)(uintptr_t)virtq_used_ring;
    virtio_blk_common->queue_enable = 1;
    virtio_blk_notify_off = virtio_blk_common->queue_notify_off;

    virtio_blk_common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    if (virtio_blk_device) {
        uint64_t cap = 0;
        for (uint32_t i = 0; i < 8; ++i) {
            cap |= ((uint64_t)virtio_blk_device[i]) << (i * 8);
        }
        serial_write("[RSE] virtio-blk capacity=");
        serial_write_u64(cap);
        serial_write("\n");
    }
    return (virtio_blk_common->device_status & VIRTIO_STATUS_DRIVER_OK) ? 0 : -1;
}

static int virtio_init(void) {
    virtio_blk_use_modern = 0;
    if (virtio_init_modern() == 0) {
        virtio_blk_use_modern = 1;
        serial_write("[RSE] virtio-blk modern online\n");
        return 0;
    }
    if (virtio_init_legacy() == 0) {
        virtio_blk_use_modern = 0;
        return 0;
    }
    return -1;
}

static int virtio_blk_rw(uint64_t sector, void *buf, uint32_t bytes, uint32_t type) {
    if (!virtio_blk_use_modern && virtio_io_base == 0) {
        return -1;
    }
    if (virtio_blk_use_modern && !virtio_blk_common) {
        return -1;
    }
    if (sector == 0 && type == VIRTIO_BLK_T_OUT) {
        serial_write("[RSE] virtio desc req=");
        serial_write_u64((uint64_t)(uintptr_t)virtio_req_ptr);
        serial_write(" data=");
        serial_write_u64((uint64_t)(uintptr_t)buf);
        serial_write(" status=");
        serial_write_u64((uint64_t)(uintptr_t)virtio_status_ptr);
        serial_write("\n");
    }
    virtio_req_ptr->type = type;
    virtio_req_ptr->reserved = 0;
    virtio_req_ptr->sector = sector;
    *virtio_status_ptr = 0xFF;

    void *data_buf = buf;
    if (virtio_blk_dma_buf && bytes <= 4096u) {
        if (type == VIRTIO_BLK_T_OUT) {
            memcpy(virtio_blk_dma_buf, buf, bytes);
        }
        data_buf = virtio_blk_dma_buf;
    }

    virtq_desc_table[0].addr = (uint64_t)(uintptr_t)virtio_req_ptr;
    virtq_desc_table[0].len = sizeof(*virtio_req_ptr);
    virtq_desc_table[0].flags = VIRTQ_DESC_F_NEXT;
    virtq_desc_table[0].next = 1;

    virtq_desc_table[1].addr = (uint64_t)(uintptr_t)data_buf;
    virtq_desc_table[1].len = bytes;
    virtq_desc_table[1].flags = VIRTQ_DESC_F_NEXT | (type == VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0);
    virtq_desc_table[1].next = 2;

    virtq_desc_table[2].addr = (uint64_t)(uintptr_t)virtio_status_ptr;
    virtq_desc_table[2].len = sizeof(*virtio_status_ptr);
    virtq_desc_table[2].flags = VIRTQ_DESC_F_WRITE;
    virtq_desc_table[2].next = 0;

    __asm__ volatile("mfence" ::: "memory");
    uint16_t idx = virtq_avail_ring->idx;
    virtq_avail_ring->ring[idx % virtq_size] = 0;
    __asm__ volatile("mfence" ::: "memory");
    virtq_avail_ring->idx = idx + 1;
    __asm__ volatile("mfence" ::: "memory");

    serial_write("[RSE] virtio-blk notify idx=");
    serial_write_u64(idx);
    serial_write(" used=");
    serial_write_u64(virtq_used_ring->idx);
    serial_write("\n");
    if (virtio_blk_use_modern) {
        volatile uint16_t *notify = (volatile uint16_t *)(virtio_blk_notify +
            (uint32_t)virtio_blk_notify_off * virtio_blk_notify_mult);
        *notify = 0;
    } else {
        outw((uint16_t)(virtio_io_base + VIRTIO_PCI_QUEUE_NOTIFY), 0);
    }

    uint64_t spin = 0;
    while (virtq_used_ring->idx == virtio_used_idx) {
        if (virtio_blk_use_modern) {
            if (virtio_blk_isr) {
                (void)*virtio_blk_isr;
            }
        } else {
            (void)inb((uint16_t)(virtio_io_base + VIRTIO_PCI_ISR));
        }
        if (++spin > 50000000ULL) {
            serial_write("[RSE] virtio-blk timeout\n");
            return -1;
        }
    }
    virtio_used_idx = virtq_used_ring->idx;
    if (*virtio_status_ptr != 0) {
        return -1;
    }
    if (type == VIRTIO_BLK_T_IN && data_buf == virtio_blk_dma_buf) {
        memcpy(buf, virtio_blk_dma_buf, bytes);
    }
    return 0;
}

static int virtio_net_find_legacy(void) {
    for (uint8_t bus = 0; bus < 32; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    continue;
                }
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor != VIRTIO_PCI_VENDOR) {
                    continue;
                }
                if (device != VIRTIO_PCI_DEVICE_NET_LEGACY &&
                    device != VIRTIO_PCI_DEVICE_NET_TRANSITIONAL) {
                    continue;
                }

                uint16_t command = pci_config_read16(bus, slot, func, 0x04);
                command |= 0x0005;
                pci_config_write16(bus, slot, func, 0x04, command);

                uint32_t bar0 = pci_config_read32(bus, slot, func, 0x10);
                if ((bar0 & 0x1) == 0) {
                    continue;
                }
                virtio_net_io_base = bar0 & ~0x3u;
                return 0;
            }
        }
    }
    return -1;
}

static void virtio_net_reset(void) {
    outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS), 0);
}

static int virtio_net_setup_queue(uint16_t queue_sel,
                                  uint8_t *area,
                                  size_t area_len,
                                  struct virtq_desc **desc_out,
                                  volatile struct virtq_avail **avail_out,
                                  volatile struct virtq_used **used_out,
                                  uint16_t *qsz_out) {
    memset(area, 0, area_len);
    outw((uint16_t)(virtio_net_io_base + VIRTIO_PCI_QUEUE_SEL), queue_sel);
    uint16_t qsz = inw((uint16_t)(virtio_net_io_base + VIRTIO_PCI_QUEUE_NUM));
    if (qsz == 0 || qsz > VIRTIO_NET_MAX_Q) {
        return -1;
    }

    uintptr_t base = (uintptr_t)area;
    size_t desc_size = sizeof(struct virtq_desc) * qsz;
    uintptr_t avail_addr = base + desc_size;
    avail_addr = (avail_addr + 1) & ~((uintptr_t)1);
    size_t avail_size = sizeof(uint16_t) * 2 + sizeof(uint16_t) * qsz;
    uintptr_t used_addr = avail_addr + avail_size;
    used_addr = (used_addr + 3) & ~((uintptr_t)3);
    size_t used_size = sizeof(uint16_t) * 2 + sizeof(struct virtq_used_elem) * qsz;

    if (used_addr + used_size > base + area_len) {
        return -1;
    }

    *desc_out = (struct virtq_desc *)base;
    *avail_out = (volatile struct virtq_avail *)avail_addr;
    *used_out = (volatile struct virtq_used *)used_addr;
    (*avail_out)->idx = 0;
    (*avail_out)->flags = 0;
    (*used_out)->idx = 0;
    (*used_out)->flags = 0;

    uint32_t pfn = (uint32_t)(base >> 12);
    outl((uint16_t)(virtio_net_io_base + VIRTIO_PCI_QUEUE_PFN), pfn);
    *qsz_out = qsz;
    return 0;
}

static int virtio_net_setup_queue_modern(uint16_t queue_sel,
                                         uint8_t *area,
                                         size_t area_len,
                                         struct virtq_desc **desc_out,
                                         volatile struct virtq_avail **avail_out,
                                         volatile struct virtq_used **used_out,
                                         uint16_t *qsz_out,
                                         uint16_t *notify_off_out) {
    if (!virtio_net_common) {
        return -1;
    }
    memset(area, 0, area_len);
    virtio_net_common->queue_select = queue_sel;
    uint16_t qsz = virtio_net_common->queue_size;
    if (qsz == 0 || qsz > VIRTIO_NET_MAX_Q) {
        return -1;
    }

    uintptr_t base = (uintptr_t)area;
    size_t desc_size = sizeof(struct virtq_desc) * qsz;
    uintptr_t avail_addr = base + desc_size;
    avail_addr = (avail_addr + 1) & ~((uintptr_t)1);
    size_t avail_size = sizeof(uint16_t) * 2 + sizeof(uint16_t) * qsz;
    uintptr_t used_addr = avail_addr + avail_size;
    used_addr = (used_addr + 3) & ~((uintptr_t)3);
    size_t used_size = sizeof(uint16_t) * 2 + sizeof(struct virtq_used_elem) * qsz;

    if (used_addr + used_size > base + area_len) {
        return -1;
    }

    *desc_out = (struct virtq_desc *)base;
    *avail_out = (volatile struct virtq_avail *)avail_addr;
    *used_out = (volatile struct virtq_used *)used_addr;
    (*avail_out)->idx = 0;
    (*avail_out)->flags = 0;
    (*used_out)->idx = 0;
    (*used_out)->flags = 0;

    virtio_net_common->queue_size = qsz;
    virtio_net_common->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
    virtio_net_common->queue_desc = (uint64_t)(uintptr_t)*desc_out;
    virtio_net_common->queue_avail = (uint64_t)(uintptr_t)*avail_out;
    virtio_net_common->queue_used = (uint64_t)(uintptr_t)*used_out;
    virtio_net_common->queue_enable = 1;
    *notify_off_out = virtio_net_common->queue_notify_off;
    *qsz_out = qsz;
    return 0;
}

static int virtio_net_find_modern(void) {
    for (uint8_t bus = 0; bus < 32; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    continue;
                }
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor != VIRTIO_PCI_VENDOR) {
                    continue;
                }
                if (device != VIRTIO_PCI_DEVICE_NET_TRANSITIONAL &&
                    device != VIRTIO_PCI_DEVICE_NET_LEGACY) {
                    continue;
                }

                uint16_t command = pci_config_read16(bus, slot, func, 0x04);
                command |= 0x0006;
                pci_config_write16(bus, slot, func, 0x04, command);

                if (virtio_pci_find_caps(bus, slot, func,
                                         &virtio_net_common,
                                         &virtio_net_notify,
                                         &virtio_net_notify_mult,
                                         &virtio_net_isr,
                                         &virtio_net_device) == 0) {
                    return 0;
                }
                serial_write("[RSE] virtio-net modern caps missing bus=");
                serial_write_u64(bus);
                serial_write(" slot=");
                serial_write_u64(slot);
                serial_write(" func=");
                serial_write_u64(func);
                serial_write("\n");
            }
        }
    }
    return -1;
}

static int virtio_net_init_modern(void) {
    if (virtio_net_use_modern && virtio_net_common) {
        return 0;
    }
    if (virtio_net_find_modern() != 0) {
        return -1;
    }

    virtio_net_common->device_status = 0;
    __asm__ volatile("mfence" ::: "memory");
    virtio_net_common->device_status = VIRTIO_STATUS_ACK;
    virtio_net_common->device_status |= VIRTIO_STATUS_DRIVER;
    virtio_net_common->device_feature_select = 0;
    uint32_t host_features_lo = virtio_net_common->device_feature;
    virtio_net_common->device_feature_select = 1;
    uint32_t host_features_hi = virtio_net_common->device_feature;
    virtio_net_mrg_rxbuf = 0;
    virtio_net_hdr_len = VIRTIO_NET_HDR_BASE_SIZE;
    uint32_t driver_features_lo = host_features_lo & VIRTIO_NET_F_MAC;
    if (host_features_lo & VIRTIO_NET_F_MRG_RXBUF) {
        driver_features_lo |= VIRTIO_NET_F_MRG_RXBUF;
        virtio_net_mrg_rxbuf = 1;
        virtio_net_hdr_len = VIRTIO_NET_HDR_MRG_SIZE;
    }
    uint32_t driver_features_hi = host_features_hi & VIRTIO_F_VERSION_1;
    if ((host_features_hi & VIRTIO_F_VERSION_1) == 0) {
        return -1;
    }
    virtio_net_common->driver_feature_select = 0;
    virtio_net_common->driver_feature = driver_features_lo;
    virtio_net_common->driver_feature_select = 1;
    virtio_net_common->driver_feature = driver_features_hi;
    virtio_net_common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    if ((virtio_net_common->device_status & VIRTIO_STATUS_FEATURES_OK) == 0) {
        return -1;
    }

    if (virtio_net_device) {
        for (uint32_t i = 0; i < sizeof(virtio_net_mac); ++i) {
            virtio_net_mac[i] = virtio_net_device[i];
        }
        virtio_net_mac_valid = 1;
        serial_write("[RSE] virtio-net mac=");
        for (uint32_t i = 0; i < sizeof(virtio_net_mac); ++i) {
            serial_write_u64(virtio_net_mac[i]);
            if (i + 1 < sizeof(virtio_net_mac)) {
                serial_write(":");
            }
        }
        serial_write("\n");
    }
    if (virtio_net_mrg_rxbuf) {
        serial_write("[RSE] virtio-net mergeable rxbuf on\n");
    }

    if (virtio_net_rx_area == virtio_net_rx_area_static) {
        void *rx_area = uefi_alloc_pages(4096u * 8u);
        if (rx_area) {
            virtio_net_rx_area = (uint8_t *)rx_area;
            virtio_net_rx_area_len = 4096u * 8u;
        }
    }
    if (virtio_net_tx_area == virtio_net_tx_area_static) {
        void *tx_area = uefi_alloc_pages(4096u * 4u);
        if (tx_area) {
            virtio_net_tx_area = (uint8_t *)tx_area;
            virtio_net_tx_area_len = 4096u * 4u;
        }
    }
    if (net_rx_bufs == (uint8_t *)net_rx_bufs_static) {
        void *rx_bufs = uefi_alloc_pages(VIRTIO_NET_MAX_Q * VIRTIO_NET_BUF_SIZE);
        if (rx_bufs) {
            net_rx_bufs = (uint8_t *)rx_bufs;
        }
    }
    if (net_tx_bufs == (uint8_t *)net_tx_bufs_static) {
        void *tx_bufs = uefi_alloc_pages(VIRTIO_NET_MAX_Q * VIRTIO_NET_BUF_SIZE);
        if (tx_bufs) {
            net_tx_bufs = (uint8_t *)tx_bufs;
        }
    }
    if (net_tx_hdrs == net_tx_hdrs_static) {
        void *tx_hdrs = uefi_alloc_pages(4096u);
        if (tx_hdrs) {
            net_tx_hdrs = (struct virtio_net_hdr_mrg *)tx_hdrs;
        }
    }

    if (virtio_net_setup_queue_modern(VIRTIO_NET_QUEUE_RX, virtio_net_rx_area,
                                      virtio_net_rx_area_len,
                                      &net_rx_desc, &net_rx_avail, &net_rx_used,
                                      &net_rx_qsz, &virtio_net_notify_off_rx) != 0) {
        return -1;
    }
    if (virtio_net_setup_queue_modern(VIRTIO_NET_QUEUE_TX, virtio_net_tx_area,
                                      virtio_net_tx_area_len,
                                      &net_tx_desc, &net_tx_avail, &net_tx_used,
                                      &net_tx_qsz, &virtio_net_notify_off_tx) != 0) {
        return -1;
    }

    net_rx_used_idx = 0;
    net_tx_used_idx = 0;

    for (uint16_t i = 0; i < net_rx_qsz; ++i) {
        net_rx_desc[i].addr = (uint64_t)(uintptr_t)(net_rx_bufs + (size_t)i * VIRTIO_NET_BUF_SIZE);
        net_rx_desc[i].len = VIRTIO_NET_BUF_SIZE;
        net_rx_desc[i].flags = VIRTQ_DESC_F_WRITE;
        net_rx_desc[i].next = 0;
        net_rx_avail->ring[i] = i;
    }
    __asm__ volatile("mfence" ::: "memory");
    net_rx_avail->idx = net_rx_qsz;
    volatile uint16_t *notify = (volatile uint16_t *)(virtio_net_notify +
        (uint32_t)virtio_net_notify_off_rx * virtio_net_notify_mult);
    *notify = VIRTIO_NET_QUEUE_RX;

    net_tx_slots = (uint16_t)(net_tx_qsz / 2);
    if (net_tx_slots == 0) {
        return -1;
    }
    memset(net_tx_hdrs, 0, sizeof(*net_tx_hdrs) * (size_t)net_tx_slots);
    memset(net_tx_bufs, 0, VIRTIO_NET_BUF_SIZE * (size_t)net_tx_slots);

    virtio_net_common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    return (virtio_net_common->device_status & VIRTIO_STATUS_DRIVER_OK) ? 0 : -1;
}

static int virtio_net_init_legacy(void) {
    if (virtio_net_io_base != 0) {
        return 0;
    }
    if (virtio_net_find_legacy() != 0) {
        return -1;
    }

    virtio_net_reset();
    outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS), VIRTIO_STATUS_ACK);
    outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS),
         VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
    uint32_t host_features = inl((uint16_t)(virtio_net_io_base + VIRTIO_PCI_HOST_FEATURES));
    virtio_net_mrg_rxbuf = 0;
    virtio_net_hdr_len = VIRTIO_NET_HDR_BASE_SIZE;
    uint32_t guest_features = host_features & VIRTIO_NET_F_MAC;
    if (host_features & VIRTIO_NET_F_MRG_RXBUF) {
        guest_features |= VIRTIO_NET_F_MRG_RXBUF;
        virtio_net_mrg_rxbuf = 1;
        virtio_net_hdr_len = VIRTIO_NET_HDR_MRG_SIZE;
    }
    outl((uint16_t)(virtio_net_io_base + VIRTIO_PCI_GUEST_FEATURES), guest_features);
    outl((uint16_t)(virtio_net_io_base + VIRTIO_PCI_GUEST_PAGE_SIZE), 4096);
    for (uint32_t i = 0; i < sizeof(virtio_net_mac); ++i) {
        virtio_net_mac[i] = inb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_CONFIG + i));
    }
    virtio_net_mac_valid = 1;
    outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS),
         VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    serial_write("[RSE] virtio-net mac=");
    for (uint32_t i = 0; i < sizeof(virtio_net_mac); ++i) {
        serial_write_u64(virtio_net_mac[i]);
        if (i + 1 < sizeof(virtio_net_mac)) {
            serial_write(":");
        }
    }
    serial_write("\n");
    if (virtio_net_mrg_rxbuf) {
        serial_write("[RSE] virtio-net mergeable rxbuf on\n");
    }

    if (virtio_net_rx_area == virtio_net_rx_area_static) {
        void *rx_area = uefi_alloc_pages(4096u * 8u);
        if (rx_area) {
            virtio_net_rx_area = (uint8_t *)rx_area;
            virtio_net_rx_area_len = 4096u * 8u;
        }
    }
    if (virtio_net_tx_area == virtio_net_tx_area_static) {
        void *tx_area = uefi_alloc_pages(4096u * 4u);
        if (tx_area) {
            virtio_net_tx_area = (uint8_t *)tx_area;
            virtio_net_tx_area_len = 4096u * 4u;
        }
    }
    if (net_rx_bufs == (uint8_t *)net_rx_bufs_static) {
        void *rx_bufs = uefi_alloc_pages(VIRTIO_NET_MAX_Q * VIRTIO_NET_BUF_SIZE);
        if (rx_bufs) {
            net_rx_bufs = (uint8_t *)rx_bufs;
        }
    }
    if (net_tx_bufs == (uint8_t *)net_tx_bufs_static) {
        void *tx_bufs = uefi_alloc_pages(VIRTIO_NET_MAX_Q * VIRTIO_NET_BUF_SIZE);
        if (tx_bufs) {
            net_tx_bufs = (uint8_t *)tx_bufs;
        }
    }
    if (net_tx_hdrs == net_tx_hdrs_static) {
        void *tx_hdrs = uefi_alloc_pages(4096u);
        if (tx_hdrs) {
            net_tx_hdrs = (struct virtio_net_hdr_mrg *)tx_hdrs;
        }
    }

    if (virtio_net_setup_queue(VIRTIO_NET_QUEUE_RX, virtio_net_rx_area,
                               virtio_net_rx_area_len,
                               &net_rx_desc, &net_rx_avail, &net_rx_used,
                               &net_rx_qsz) != 0) {
        outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS), VIRTIO_STATUS_FAILED);
        return -1;
    }
    if (virtio_net_setup_queue(VIRTIO_NET_QUEUE_TX, virtio_net_tx_area,
                               virtio_net_tx_area_len,
                               &net_tx_desc, &net_tx_avail, &net_tx_used,
                               &net_tx_qsz) != 0) {
        outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS), VIRTIO_STATUS_FAILED);
        return -1;
    }

    net_rx_used_idx = 0;
    net_tx_used_idx = 0;

    for (uint16_t i = 0; i < net_rx_qsz; ++i) {
        net_rx_desc[i].addr = (uint64_t)(uintptr_t)(net_rx_bufs + (size_t)i * VIRTIO_NET_BUF_SIZE);
        net_rx_desc[i].len = VIRTIO_NET_BUF_SIZE;
        net_rx_desc[i].flags = VIRTQ_DESC_F_WRITE;
        net_rx_desc[i].next = 0;
        net_rx_avail->ring[i] = i;
    }
    __asm__ volatile("mfence" ::: "memory");
    net_rx_avail->idx = net_rx_qsz;
    outw((uint16_t)(virtio_net_io_base + VIRTIO_PCI_QUEUE_NOTIFY), VIRTIO_NET_QUEUE_RX);

    net_tx_slots = (uint16_t)(net_tx_qsz / 2);
    if (net_tx_slots == 0) {
        return -1;
    }
    memset(net_tx_hdrs, 0, sizeof(*net_tx_hdrs) * (size_t)net_tx_slots);
    memset(net_tx_bufs, 0, VIRTIO_NET_BUF_SIZE * (size_t)net_tx_slots);

    outb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS),
         VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
             VIRTIO_STATUS_DRIVER_OK);

    uint8_t status = inb((uint16_t)(virtio_net_io_base + VIRTIO_PCI_STATUS));
    return (status & VIRTIO_STATUS_DRIVER_OK) ? 0 : -1;
}

static int virtio_net_init(void) {
    virtio_net_use_modern = 0;
    if (virtio_net_init_modern() == 0) {
        virtio_net_use_modern = 1;
        serial_write("[RSE] virtio-net modern online\n");
        return 0;
    }
    if (virtio_net_init_legacy() == 0) {
        virtio_net_use_modern = 0;
        return 0;
    }
    return -1;
}

static void virtio_net_notify_tx(void) {
    if (virtio_net_use_modern) {
        volatile uint16_t *notify = (volatile uint16_t *)(virtio_net_notify +
            (uint32_t)virtio_net_notify_off_tx * virtio_net_notify_mult);
        *notify = VIRTIO_NET_QUEUE_TX;
    } else {
        outw((uint16_t)(virtio_net_io_base + VIRTIO_PCI_QUEUE_NOTIFY), VIRTIO_NET_QUEUE_TX);
    }
}

static int virtio_net_send(const void *buf, uint32_t len) {
    static int tx_stall_logged = 0;
    if (!buf || len == 0) {
        return -1;
    }
    if (!virtio_net_use_modern && virtio_net_io_base == 0) {
        return -1;
    }
    if (virtio_net_use_modern && !virtio_net_common) {
        return -1;
    }
    if (len > VIRTIO_NET_BUF_SIZE) {
        return -1;
    }
    if (net_tx_slots == 0) {
        return -1;
    }

    uint16_t avail_idx = net_tx_avail->idx;
    uint16_t used_idx = net_tx_used->idx;
    for (uint32_t i = 0; i < 100000; ++i) {
        if ((uint16_t)(avail_idx - used_idx) < net_tx_slots) {
            break;
        }
        used_idx = net_tx_used->idx;
    }
    net_tx_used_idx = used_idx;
    if ((uint16_t)(avail_idx - used_idx) >= net_tx_slots) {
        if (!tx_stall_logged) {
            serial_write("[RSE] virtio-net tx queue full idx=");
            serial_write_u64(net_tx_used->idx);
            serial_write("\n");
            tx_stall_logged = 1;
        }
        return -1;
    }

    uint16_t slot = (uint16_t)(avail_idx % net_tx_slots);
    uint16_t desc_idx = (uint16_t)(slot * 2);
    uint8_t *tx_buf = net_tx_bufs + (size_t)slot * VIRTIO_NET_BUF_SIZE;
    uint8_t *tx_hdr = (uint8_t *)&net_tx_hdrs[slot];
    memcpy(tx_buf, buf, len);
    memset(tx_hdr, 0, virtio_net_hdr_len);

    net_tx_desc[desc_idx].addr = (uint64_t)(uintptr_t)tx_hdr;
    net_tx_desc[desc_idx].len = virtio_net_hdr_len;
    net_tx_desc[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    net_tx_desc[desc_idx].next = (uint16_t)(desc_idx + 1);
    net_tx_desc[desc_idx + 1].addr = (uint64_t)(uintptr_t)tx_buf;
    net_tx_desc[desc_idx + 1].len = len;
    net_tx_desc[desc_idx + 1].flags = 0;
    net_tx_desc[desc_idx + 1].next = 0;

    __asm__ volatile("mfence" ::: "memory");
    net_tx_avail->ring[avail_idx % net_tx_qsz] = desc_idx;
    net_tx_avail->idx = avail_idx + 1;
    __asm__ volatile("mfence" ::: "memory");
    virtio_net_notify_tx();
    return (int)len;
}

static int virtio_net_recv(void *buf, uint32_t len) {
    static int rx_logged = 0;
    if (!buf || len == 0) {
        return -1;
    }
    if (!virtio_net_use_modern && virtio_net_io_base == 0) {
        return -1;
    }
    if (virtio_net_use_modern && !virtio_net_common) {
        return -1;
    }
    if (net_rx_used->idx == net_rx_used_idx) {
        return 0;
    }
    __asm__ volatile("mfence" ::: "memory");
    struct virtq_used_elem elem = net_rx_used->ring[net_rx_used_idx % net_rx_qsz];
    net_rx_used_idx++;
    if (!rx_logged) {
        serial_write("[RSE] virtio-net rx used idx=");
        serial_write_u64(net_rx_used_idx);
        serial_write(" len=");
        serial_write_u64(elem.len);
        serial_write("\n");
        rx_logged = 1;
    }
    if (elem.id >= net_rx_qsz) {
        return -1;
    }
    uint32_t data_len = 0;
    if (elem.len > virtio_net_hdr_len) {
        data_len = elem.len - virtio_net_hdr_len;
    }
    if (data_len > len) {
        data_len = len;
    }
    memcpy(buf, net_rx_bufs + (size_t)elem.id * VIRTIO_NET_BUF_SIZE + virtio_net_hdr_len, data_len);

    uint16_t idx = net_rx_avail->idx;
    net_rx_avail->ring[idx % net_rx_qsz] = (uint16_t)elem.id;
    net_rx_avail->idx = idx + 1;
    __asm__ volatile("mfence" ::: "memory");
    if (virtio_net_use_modern) {
        volatile uint16_t *notify = (volatile uint16_t *)(virtio_net_notify +
            (uint32_t)virtio_net_notify_off_rx * virtio_net_notify_mult);
        *notify = VIRTIO_NET_QUEUE_RX;
    } else {
        outw((uint16_t)(virtio_net_io_base + VIRTIO_PCI_QUEUE_NOTIFY), VIRTIO_NET_QUEUE_RX);
    }
    return (int)data_len;
}

void *rse_ivshmem_base(uint64_t *size_out) {
    static void *base = NULL;
    static uint64_t size = 0;
    if (base) {
        if (size_out) {
            *size_out = size;
        }
        return base;
    }

    for (uint8_t bus = 0; bus < 32; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x0);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                if (vendor == 0xFFFF) {
                    continue;
                }
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor != 0x1AF4 || device != 0x1110) {
                    continue;
                }

                uint16_t command = pci_config_read16(bus, slot, func, 0x04);
                command |= 0x0006;
                pci_config_write16(bus, slot, func, 0x04, command);

                uint8_t is_io = 0;
                uint64_t bar2 = pci_read_bar(bus, slot, func, 2, &is_io);
                if (bar2 && !is_io) {
                    base = (void *)(uintptr_t)bar2;
                    size = 0;
                    if (size_out) {
                        *size_out = size;
                    }
                    return base;
                }
            }
        }
    }
    if (size_out) {
        *size_out = 0;
    }
    return NULL;
}

static uint16_t net_htons(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint16_t net_checksum(const uint8_t *data, uint32_t len) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i + 1 < len; i += 2) {
        sum += (uint16_t)((data[i] << 8) | data[i + 1]);
    }
    if (len & 1) {
        sum += (uint16_t)(data[len - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static void net_queue_push(const uint8_t *data, uint32_t len) {
    if (!data || len == 0) {
        return;
    }
    if (len > NET_PAYLOAD_MAX) {
        len = NET_PAYLOAD_MAX;
    }
    if (net_queue_count >= NET_QUEUE_DEPTH) {
        return;
    }
    struct net_payload *slot = &net_queue[net_queue_head];
    slot->len = len;
    memcpy(slot->data, data, len);
    net_queue_head = (net_queue_head + 1) % NET_QUEUE_DEPTH;
    net_queue_count++;
}

static uint32_t net_queue_pop(uint8_t *buf, uint32_t max_len) {
    if (!buf || max_len == 0 || net_queue_count == 0) {
        return 0;
    }
    struct net_payload *slot = &net_queue[net_queue_tail];
    uint32_t len = slot->len;
    if (len > max_len) {
        len = max_len;
    }
    memcpy(buf, slot->data, len);
    net_queue_tail = (net_queue_tail + 1) % NET_QUEUE_DEPTH;
    net_queue_count--;
    return len;
}

static int net_ensure_mac(void) {
    if (net_mac_valid) {
        return 0;
    }
    if (rse_net_get_mac(net_mac_addr) != 0) {
        return -1;
    }
    net_mac_valid = 1;
    return 0;
}

struct __attribute__((packed)) net_eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
};

struct __attribute__((packed)) net_ipv4_hdr {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint8_t src[4];
    uint8_t dst[4];
};

struct __attribute__((packed)) net_udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
};

struct __attribute__((packed)) net_arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
};

static int net_backend_write(const void *buf, uint32_t len) {
    if (!buf || len == 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_NONE && rse_net_init() != 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_VIRTIO) {
        int rc = virtio_net_send(buf, len);
        if (rc >= 0) {
            return rc;
        }
        if (net_init_uefi() == 0 && g_net_backend == NET_BACKEND_UEFI && g_net) {
            EFI_STATUS status = g_net->Transmit(g_net, 0, len, (void *)buf,
                                                NULL, NULL, NULL);
            return EFI_ERROR(status) ? -1 : (int)len;
        }
        return -1;
    }
    EFI_STATUS status = g_net->Transmit(g_net, 0, len, (void *)buf,
                                        NULL, NULL, NULL);
    return EFI_ERROR(status) ? -1 : (int)len;
}

static int net_backend_read(void *buf, uint32_t len) {
    if (!buf || len == 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_NONE && rse_net_init() != 0) {
        return -1;
    }
    if (g_net_backend == NET_BACKEND_VIRTIO) {
        return virtio_net_recv(buf, len);
    }
    UINTN header_size = 0;
    UINTN buf_size = len;
    EFI_MAC_ADDRESS src;
    EFI_MAC_ADDRESS dst;
    UINT16 protocol = 0;
    EFI_STATUS status = g_net->Receive(g_net, &header_size, &buf_size, buf,
                                       &src, &dst, &protocol);
    if (status == EFI_NOT_READY) {
        return 0;
    }
    if (EFI_ERROR(status)) {
        return -1;
    }
    return (int)buf_size;
}

static void net_send_arp_reply(const struct net_eth_hdr *rx_eth, const struct net_arp_pkt *rx_arp) {
    if (!rx_eth || !rx_arp) {
        return;
    }
    if (net_ensure_mac() != 0) {
        return;
    }
    uint8_t frame[64];
    struct net_eth_hdr eth = {};
    struct net_arp_pkt arp = {};
    for (uint32_t i = 0; i < 6; ++i) {
        eth.dst[i] = rx_eth->src[i];
        eth.src[i] = net_mac_addr[i];
        arp.sha[i] = net_mac_addr[i];
        arp.tha[i] = rx_arp->sha[i];
    }
    eth.ethertype = net_htons(0x0806);
    arp.htype = net_htons(0x0001);
    arp.ptype = net_htons(0x0800);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = net_htons(0x0002);
    for (uint32_t i = 0; i < 4; ++i) {
        arp.spa[i] = net_ip_addr[i];
        arp.tpa[i] = rx_arp->spa[i];
    }
    uint32_t offset = 0;
    memcpy(frame + offset, &eth, sizeof(eth));
    offset += sizeof(eth);
    memcpy(frame + offset, &arp, sizeof(arp));
    offset += sizeof(arp);
    if (offset < 60) {
        memset(frame + offset, 0, 60 - offset);
        offset = 60;
    }
    net_backend_write(frame, offset);
}

static void net_send_udp(const uint8_t dst_mac[6], const uint8_t dst_ip[4],
                         uint16_t dst_port, uint16_t src_port,
                         const uint8_t *payload, uint32_t len) {
    if (!dst_mac || !dst_ip || !payload || len == 0) {
        return;
    }
    if (net_ensure_mac() != 0) {
        return;
    }
    uint8_t frame[64 + NET_PAYLOAD_MAX];
    uint32_t offset = 0;
    struct net_eth_hdr eth = {};
    struct net_ipv4_hdr ip = {};
    struct net_udp_hdr udp = {};

    for (uint32_t i = 0; i < 6; ++i) {
        eth.dst[i] = dst_mac[i];
        eth.src[i] = net_mac_addr[i];
    }
    eth.ethertype = net_htons(0x0800);

    ip.ver_ihl = 0x45;
    ip.tos = 0;
    uint16_t udp_len = (uint16_t)(sizeof(struct net_udp_hdr) + len);
    ip.total_len = net_htons((uint16_t)(sizeof(struct net_ipv4_hdr) + udp_len));
    ip.id = net_htons(0x1234);
    ip.frag_off = net_htons(0x4000);
    ip.ttl = 64;
    ip.proto = 17;
    ip.checksum = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        ip.src[i] = net_ip_addr[i];
        ip.dst[i] = dst_ip[i];
    }
    ip.checksum = net_checksum((const uint8_t *)&ip, sizeof(ip));

    udp.src_port = net_htons(src_port);
    udp.dst_port = net_htons(dst_port);
    udp.len = net_htons(udp_len);
    udp.checksum = 0;

    memcpy(frame + offset, &eth, sizeof(eth));
    offset += sizeof(eth);
    memcpy(frame + offset, &ip, sizeof(ip));
    offset += sizeof(ip);
    memcpy(frame + offset, &udp, sizeof(udp));
    offset += sizeof(udp);
    memcpy(frame + offset, payload, len);
    offset += len;

    if (offset < 60) {
        memset(frame + offset, 0, 60 - offset);
        offset = 60;
    }
    net_backend_write(frame, offset);
}

static int net_payload_starts_with(const uint8_t *payload, uint32_t len, const char *prefix) {
    if (!payload || !prefix) {
        return 0;
    }
    for (uint32_t i = 0; prefix[i] != '\0'; ++i) {
        if (i >= len || payload[i] != (uint8_t)prefix[i]) {
            return 0;
        }
    }
    return 1;
}

static int net_is_http_request(const uint8_t *payload, uint32_t len) {
    return net_payload_starts_with(payload, len, "GET ") ||
           net_payload_starts_with(payload, len, "HEAD ") ||
           net_payload_starts_with(payload, len, "POST ") ||
           net_payload_starts_with(payload, len, "PUT ") ||
           net_payload_starts_with(payload, len, "DELETE ") ||
           net_payload_starts_with(payload, len, "OPTIONS ");
}

static int net_server_handle_frame(const uint8_t *frame, uint32_t len,
                                   uint32_t *udp_count, uint32_t *http_count) {
    if (!frame || len < sizeof(struct net_eth_hdr)) {
        return 0;
    }
    const struct net_eth_hdr *eth = (const struct net_eth_hdr *)frame;
    uint16_t ethertype = net_htons(eth->ethertype);
    if (ethertype == 0x0806) {
        if (len < sizeof(struct net_eth_hdr) + sizeof(struct net_arp_pkt)) {
            return 0;
        }
        const struct net_arp_pkt *arp = (const struct net_arp_pkt *)(frame + sizeof(*eth));
        if (net_htons(arp->oper) == 0x0001 &&
            arp->tpa[0] == net_ip_addr[0] &&
            arp->tpa[1] == net_ip_addr[1] &&
            arp->tpa[2] == net_ip_addr[2] &&
            arp->tpa[3] == net_ip_addr[3]) {
            net_send_arp_reply(eth, arp);
        }
        return 1;
    }
    if (ethertype != 0x0800) {
        return 0;
    }
    if (len < sizeof(struct net_eth_hdr) + sizeof(struct net_ipv4_hdr)) {
        return 0;
    }
    const uint8_t *payload = frame + sizeof(struct net_eth_hdr);
    uint32_t payload_len = len - sizeof(struct net_eth_hdr);
    const struct net_ipv4_hdr *ip = (const struct net_ipv4_hdr *)payload;
    if ((ip->ver_ihl >> 4) != 4) {
        return 0;
    }
    uint32_t ihl = (uint32_t)(ip->ver_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct net_ipv4_hdr) || payload_len < ihl + sizeof(struct net_udp_hdr)) {
        return 0;
    }
    if (ip->dst[0] != net_ip_addr[0] ||
        ip->dst[1] != net_ip_addr[1] ||
        ip->dst[2] != net_ip_addr[2] ||
        ip->dst[3] != net_ip_addr[3]) {
        return 0;
    }
    if (ip->proto != 17) {
        return 0;
    }
    const struct net_udp_hdr *udp = (const struct net_udp_hdr *)(payload + ihl);
    uint16_t dst_port = net_htons(udp->dst_port);
    uint16_t src_port = net_htons(udp->src_port);
    uint16_t udp_len = net_htons(udp->len);
    if (udp_len < sizeof(struct net_udp_hdr)) {
        return 0;
    }
    uint32_t udp_payload_len = udp_len - sizeof(struct net_udp_hdr);
    if (ihl + sizeof(struct net_udp_hdr) + udp_payload_len > payload_len) {
        return 0;
    }
    const uint8_t *udp_payload = payload + ihl + sizeof(struct net_udp_hdr);
    if (dst_port == net_udp_port) {
        net_send_udp(eth->src, ip->src, src_port, dst_port, udp_payload, udp_payload_len);
        if (udp_count) {
            (*udp_count)++;
        }
        return 1;
    }
    if (dst_port == net_http_port) {
        if (net_is_http_request(udp_payload, udp_payload_len)) {
            static const char http_resp[] =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "RSE HTTP OK\n";
            net_send_udp(eth->src, ip->src, src_port, dst_port,
                         (const uint8_t *)http_resp, (uint32_t)(sizeof(http_resp) - 1));
            if (http_count) {
                (*http_count)++;
            }
        }
        return 1;
    }
    return 0;
}

static void net_handle_ipv4(const struct net_eth_hdr *eth, const uint8_t *payload, uint32_t len) {
    if (len < sizeof(struct net_ipv4_hdr)) {
        return;
    }
    const struct net_ipv4_hdr *ip = (const struct net_ipv4_hdr *)payload;
    if ((ip->ver_ihl >> 4) != 4) {
        return;
    }
    uint32_t ihl = (uint32_t)(ip->ver_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct net_ipv4_hdr) || len < ihl + sizeof(struct net_udp_hdr)) {
        return;
    }
    if (ip->dst[0] != net_ip_addr[0] ||
        ip->dst[1] != net_ip_addr[1] ||
        ip->dst[2] != net_ip_addr[2] ||
        ip->dst[3] != net_ip_addr[3]) {
        return;
    }
    if (ip->proto != 17) {
        return;
    }
    const struct net_udp_hdr *udp = (const struct net_udp_hdr *)(payload + ihl);
    uint16_t dst_port = net_htons(udp->dst_port);
    uint16_t src_port = net_htons(udp->src_port);
    if (dst_port != net_udp_port) {
        return;
    }
    uint16_t udp_len = net_htons(udp->len);
    if (udp_len < sizeof(struct net_udp_hdr)) {
        return;
    }
    uint32_t payload_len = udp_len - sizeof(struct net_udp_hdr);
    if (ihl + sizeof(struct net_udp_hdr) + payload_len > len) {
        return;
    }
    const uint8_t *udp_payload = payload + ihl + sizeof(struct net_udp_hdr);
    net_queue_push(udp_payload, payload_len);

    if (eth) {
        net_send_udp(eth->src, ip->src, src_port, dst_port, udp_payload, payload_len);
    }
}

static void net_handle_frame(const uint8_t *frame, uint32_t len) {
    if (!frame || len < sizeof(struct net_eth_hdr)) {
        return;
    }
    const struct net_eth_hdr *eth = (const struct net_eth_hdr *)frame;
    uint16_t ethertype = net_htons(eth->ethertype);
    const uint8_t *payload = frame + sizeof(struct net_eth_hdr);
    uint32_t payload_len = len - sizeof(struct net_eth_hdr);
    if (ethertype == 0x0806 && payload_len >= sizeof(struct net_arp_pkt)) {
        const struct net_arp_pkt *arp = (const struct net_arp_pkt *)payload;
        if (net_htons(arp->oper) == 0x0001 &&
            arp->tpa[0] == net_ip_addr[0] &&
            arp->tpa[1] == net_ip_addr[1] &&
            arp->tpa[2] == net_ip_addr[2] &&
            arp->tpa[3] == net_ip_addr[3]) {
            net_send_arp_reply(eth, arp);
        }
        return;
    }
    if (ethertype == 0x0800) {
        net_handle_ipv4(eth, payload, payload_len);
    }
}

static void net_poll_rx(uint32_t budget) {
    if (budget == 0) {
        return;
    }
    uint8_t rx_buf[2048];
    for (uint32_t i = 0; i < budget; ++i) {
        int got = net_backend_read(rx_buf, sizeof(rx_buf));
        if (got <= 0) {
            return;
        }
        net_handle_frame(rx_buf, (uint32_t)got);
    }
}

static int net_udp_send(const uint8_t *payload, uint32_t len) {
    if (!payload || len == 0) {
        return -1;
    }
    if (net_ensure_mac() != 0) {
        return -1;
    }
    net_queue_push(payload, len);
    net_send_udp(net_mac_addr, net_ip_addr, net_udp_port, net_udp_port, payload, len);
    return (int)len;
}

static int bench_net_arp(void) {
    if (!virtio_net_use_modern && virtio_net_io_base == 0) {
        return -1;
    }
    if (virtio_net_use_modern && !virtio_net_common) {
        return -1;
    }
    if (!virtio_net_mac_valid) {
        return -1;
    }

    struct __attribute__((packed)) eth_hdr {
        uint8_t dst[6];
        uint8_t src[6];
        uint16_t ethertype;
    } eth;
    struct __attribute__((packed)) arp_pkt {
        uint16_t htype;
        uint16_t ptype;
        uint8_t hlen;
        uint8_t plen;
        uint16_t oper;
        uint8_t sha[6];
        uint8_t spa[4];
        uint8_t tha[6];
        uint8_t tpa[4];
    } arp;

    for (uint32_t i = 0; i < 6; ++i) {
        eth.dst[i] = 0xFF;
        eth.src[i] = virtio_net_mac[i];
        arp.sha[i] = virtio_net_mac[i];
        arp.tha[i] = 0;
    }
    eth.ethertype = net_htons(0x0806);
    arp.htype = net_htons(0x0001);
    arp.ptype = net_htons(0x0800);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = net_htons(0x0001);
    arp.spa[0] = 10;
    arp.spa[1] = 0;
    arp.spa[2] = 2;
    arp.spa[3] = 15;
    arp.tpa[0] = 10;
    arp.tpa[1] = 0;
    arp.tpa[2] = 2;
    arp.tpa[3] = 2;

    uint8_t frame[64];
    uint32_t offset = 0;
    memcpy(frame + offset, &eth, sizeof(eth));
    offset += sizeof(eth);
    memcpy(frame + offset, &arp, sizeof(arp));
    offset += sizeof(arp);

    if (offset < 60) {
        memset(frame + offset, 0, 60 - offset);
        offset = 60;
    }
    if (virtio_net_send(frame, offset) < 0) {
        return -1;
    }

    uint64_t start = rdtsc();
    uint32_t rx_len = 0;
    for (uint32_t i = 0; i < 200000; ++i) {
        uint8_t rx_buf[256];
        int got = virtio_net_recv(rx_buf, sizeof(rx_buf));
        if (got <= 0) {
            continue;
        }
        if ((uint32_t)got < sizeof(eth) + sizeof(arp)) {
            continue;
        }
        struct eth_hdr *rx_eth = (struct eth_hdr *)rx_buf;
        if (net_htons(rx_eth->ethertype) != 0x0806) {
            continue;
        }
        struct arp_pkt *rx_arp = (struct arp_pkt *)(rx_buf + sizeof(eth));
        if (net_htons(rx_arp->oper) != 0x0002) {
            continue;
        }
        if (rx_arp->tpa[0] != arp.spa[0] ||
            rx_arp->tpa[1] != arp.spa[1] ||
            rx_arp->tpa[2] != arp.spa[2] ||
            rx_arp->tpa[3] != arp.spa[3]) {
            continue;
        }
        rx_len = (uint32_t)got;
        break;
    }
    uint64_t end = rdtsc();
    g_metrics.net_arp_bytes = rx_len;
    g_metrics.net_arp_cycles = end - start;
    serial_write("[RSE] net arp bytes=");
    serial_write_u64(rx_len);
    serial_write(" cycles=");
    serial_write_u64(end - start);
    serial_write("\n");
    return rx_len ? 0 : -1;
}

static void bench_udp_http_server(void) {
    serial_write("[RSE] udp/http server benchmark start\n");
    if (g_net_backend == NET_BACKEND_NONE && rse_net_init() != 0) {
        g_metrics.udp_rx = 0;
        g_metrics.udp_udp = 0;
        g_metrics.udp_http = 0;
        g_metrics.udp_cycles = 0;
        serial_write("[RSE] udp/http server skipped (net unavailable)\n");
        return;
    }
    uint64_t start = rdtsc();
    uint32_t rx = 0;
    uint32_t udp = 0;
    uint32_t http = 0;
    uint32_t idle = 0;
    uint8_t rx_buf[2048];
    for (uint32_t i = 0; i < 200000; ++i) {
        int got = net_backend_read(rx_buf, sizeof(rx_buf));
        if (got <= 0) {
            idle++;
            if (idle > 50000 && rx == 0) {
                break;
            }
            continue;
        }
        idle = 0;
        if (net_server_handle_frame(rx_buf, (uint32_t)got, &udp, &http)) {
            rx++;
        }
        if (udp + http >= 1000) {
            break;
        }
    }
    uint64_t end = rdtsc();
    g_metrics.udp_rx = rx;
    g_metrics.udp_udp = udp;
    g_metrics.udp_http = http;
    g_metrics.udp_cycles = end - start;
    serial_write("[RSE] udp/http server rx=");
    serial_write_u64(rx);
    serial_write(" udp=");
    serial_write_u64(udp);
    serial_write(" http=");
    serial_write_u64(http);
    serial_write(" cycles=");
    serial_write_u64(end - start);
    serial_write("\n");
}

static void format_filename(char *buf, uint32_t index) {
    buf[0] = 'f';
    buf[1] = 'i';
    buf[2] = 'l';
    buf[3] = 'e';
    buf[4] = (char)('0' + ((index / 1000) % 10));
    buf[5] = (char)('0' + ((index / 100) % 10));
    buf[6] = (char)('0' + ((index / 10) % 10));
    buf[7] = (char)('0' + (index % 10));
    buf[8] = '\0';
}

static void bench_compute(void) {
    serial_write("[RSE] compute benchmark start\n");
    uint64_t seed = 0x123456789abcdef0ULL;
    for (uint32_t i = 0; i < EVENT_COUNT; ++i) {
        events[i].value = xorshift64(&seed);
        events[i].state = (uint32_t)(events[i].value & 0xFFFF);
    }

    uint64_t start = rdtsc();
    uint64_t acc = 0;
    for (uint32_t iter = 0; iter < EVENT_ITERS; ++iter) {
        for (uint32_t i = 0; i < EVENT_COUNT; ++i) {
            uint64_t v = events[i].value;
            if (v & 1) {
                v ^= (v << 13);
            } else {
                v += (v >> 3);
            }
            events[i].value = v;
            events[i].state ^= (uint32_t)(v & 0xFFFFFFFFu);
            acc += v;
        }
    }
    uint64_t end = rdtsc();
    uint64_t cycles = end - start;
    uint64_t ops = (uint64_t)EVENT_COUNT * EVENT_ITERS;
    uint64_t cycles_per_op = ops ? (cycles / ops) : 0;

    g_metrics.compute_ops = ops;
    g_metrics.compute_cycles = cycles;
    g_metrics.compute_cycles_per_op = cycles_per_op;

    serial_write("[RSE] compute ops=");
    serial_write_u64(ops);
    serial_write(" cycles=");
    serial_write_u64(cycles);
    serial_write(" cycles/op=");
    serial_write_u64(cycles_per_op);
    serial_write(" checksum=");
    serial_write_u64(acc);
    serial_write("\n");
}

static void bench_memory(void) {
    serial_write("[RSE] memory benchmark start\n");
    for (uint32_t i = 0; i < MEM_BYTES; ++i) {
        mem_a[i] = (uint8_t)(i * 31u);
        mem_b[i] = 0;
    }

    const uint32_t passes = 8;
    uint64_t start = rdtsc();
    for (uint32_t p = 0; p < passes; ++p) {
        memcpy(mem_b, mem_a, MEM_BYTES);
        memcpy(mem_a, mem_b, MEM_BYTES);
    }
    uint64_t end = rdtsc();
    uint64_t bytes = (uint64_t)MEM_BYTES * passes * 2u;
    uint64_t cycles = end - start;
    uint64_t cycles_per_byte = bytes ? (cycles / bytes) : 0;

    g_metrics.memory_bytes = bytes;
    g_metrics.memory_cycles = cycles;
    g_metrics.memory_cycles_per_byte = cycles_per_byte;

    serial_write("[RSE] memory bytes=");
    serial_write_u64(bytes);
    serial_write(" cycles=");
    serial_write_u64(cycles);
    serial_write(" cycles/byte=");
    serial_write_u64(cycles_per_byte);
    serial_write("\n");
}

static void bench_files(void) {
    serial_write("[RSE] ramfs benchmark start\n");
    ramfs_reset();
    uint8_t tmp[RAMFS_FILE_SIZE];
    for (uint32_t i = 0; i < RAMFS_FILE_SIZE; ++i) {
        tmp[i] = (uint8_t)(i ^ 0x5a);
    }

    const uint32_t file_count = 96;
    char name[RAMFS_NAME_MAX];
    uint64_t start = rdtsc();
    for (uint32_t i = 0; i < file_count; ++i) {
        format_filename(name, i);
        int idx = ramfs_create(name);
        ramfs_write(idx, tmp, 1024);
    }

    uint64_t checksum = 0;
    for (uint32_t i = 0; i < file_count; ++i) {
        format_filename(name, i);
        int idx = ramfs_find(name);
        uint32_t read = ramfs_read(idx, tmp, 1024);
        for (uint32_t j = 0; j < read; ++j) {
            checksum += tmp[j];
        }
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        format_filename(name, i);
        int idx = ramfs_find(name);
        if (idx >= 0) {
            ramfs_delete(idx);
        }
    }
    uint64_t end = rdtsc();

    uint64_t ops = (uint64_t)file_count * 3u;
    uint64_t cycles = end - start;
    uint64_t cycles_per_op = ops ? (cycles / ops) : 0;

    g_metrics.ramfs_ops = ops;
    g_metrics.ramfs_cycles = cycles;
    g_metrics.ramfs_cycles_per_op = cycles_per_op;

    serial_write("[RSE] ramfs ops=");
    serial_write_u64(ops);
    serial_write(" cycles=");
    serial_write_u64(cycles);
    serial_write(" cycles/op=");
    serial_write_u64(cycles_per_op);
    serial_write(" checksum=");
    serial_write_u64(checksum);
    serial_write(" files=");
    serial_write_u64(ramfs_count());
    serial_write("\n");
}

static void init_workloads(void) {
    if (g_os_initialized) {
        serial_write("[RSE] init workloads skipped (already ready)\n");
        return;
    }
    serial_write("[RSE] init workloads start\n");
    kfd_reset();
    ramfs_reset();
    extern void rse_braid_smoke(void);
    rse_braid_smoke();
    extern void rse_os_run(void);
    rse_os_run();
    g_os_initialized = 1;
}

static void bench_http_loopback(void) {
    serial_write("[RSE] http loopback benchmark start\n");
    const char *req = "GET / HTTP/1.1\r\nHost: rse\r\n\r\n";
    const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    uint64_t requests = 50000;
    uint64_t start = rdtsc();
    uint64_t bytes = 0;
    for (uint64_t i = 0; i < requests; ++i) {
        for (uint32_t j = 0; req[j]; ++j) {
            bytes += (uint8_t)req[j];
        }
        for (uint32_t j = 0; resp[j]; ++j) {
            bytes += (uint8_t)resp[j];
        }
    }
    uint64_t end = rdtsc();
    uint64_t cycles = end - start;
    uint64_t cycles_per_req = requests ? (cycles / requests) : 0;

    g_metrics.http_requests = requests;
    g_metrics.http_cycles = cycles;
    g_metrics.http_cycles_per_req = cycles_per_req;

    serial_write("[RSE] http requests=");
    serial_write_u64(requests);
    serial_write(" cycles=");
    serial_write_u64(cycles);
    serial_write(" cycles/req=");
    serial_write_u64(cycles_per_req);
    serial_write(" checksum=");
    serial_write_u64(bytes);
    serial_write("\n");
}

static void bench_uefi_fs(struct rse_boot_info *boot_info) {
    EFI_SYSTEM_TABLE *st = get_system_table(boot_info);
    if (!st || !st->BootServices) {
        serial_write("[RSE] UEFI FS unavailable (no system table)\n");
        return;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = uefi_find_writable_fs(st);
    if (!fs) {
        serial_write("[RSE] UEFI FS unavailable (no writable FS)\n");
        return;
    }

    EFI_FILE_PROTOCOL *root = NULL;
    EFI_STATUS status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status) || !root) {
        serial_write("[RSE] UEFI FS open volume failed\n");
        return;
    }

    serial_write("[RSE] UEFI FS benchmark start\n");
    uint8_t buf[RAMFS_FILE_SIZE];
    for (uint32_t i = 0; i < RAMFS_FILE_SIZE; ++i) {
        buf[i] = (uint8_t)(i ^ 0xa5);
    }

    const uint32_t file_count = 48;
    CHAR16 name[16];
    uint64_t checksum = 0;
    uint64_t start = rdtsc();

    for (uint32_t i = 0; i < file_count; ++i) {
        EFI_FILE_PROTOCOL *file = NULL;
        format_filename16(name, i);
        status = root->Open(root, &file, name,
                            EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                            0);
        if (EFI_ERROR(status) || !file) {
            continue;
        }
        UINTN write_size = 2048;
        file->Write(file, &write_size, buf);
        file->Close(file);
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        EFI_FILE_PROTOCOL *file = NULL;
        format_filename16(name, i);
        status = root->Open(root, &file, name, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status) || !file) {
            continue;
        }
        UINTN read_size = 2048;
        file->Read(file, &read_size, buf);
        for (uint32_t j = 0; j < read_size; ++j) {
            checksum += buf[j];
        }
        file->Close(file);
    }

    for (uint32_t i = 0; i < file_count; ++i) {
        EFI_FILE_PROTOCOL *file = NULL;
        format_filename16(name, i);
        status = root->Open(root, &file, name, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
        if (EFI_ERROR(status) || !file) {
            continue;
        }
        file->Delete(file);
    }

    uint64_t end = rdtsc();
    uint64_t ops = (uint64_t)file_count * 3u;
    uint64_t cycles = end - start;
    uint64_t cycles_per_op = ops ? (cycles / ops) : 0;

    root->Close(root);

    g_metrics.uefi_fs_ops = ops;
    g_metrics.uefi_fs_cycles = cycles;
    g_metrics.uefi_fs_cycles_per_op = cycles_per_op;

    serial_write("[RSE] UEFI FS ops=");
    serial_write_u64(ops);
    serial_write(" cycles=");
    serial_write_u64(cycles);
    serial_write(" cycles/op=");
    serial_write_u64(cycles_per_op);
    serial_write(" checksum=");
    serial_write_u64(checksum);
    serial_write("\n");
}

static void bench_uefi_block(struct rse_boot_info *boot_info) {
    EFI_SYSTEM_TABLE *st = get_system_table(boot_info);
    if (!st || !st->BootServices) {
        serial_write("[RSE] UEFI block unavailable (no system table)\n");
        return;
    }

    EFI_BLOCK_IO_PROTOCOL *blk = uefi_find_raw_block(st);
    if (!blk || !blk->Media) {
        serial_write("[RSE] UEFI block unavailable (no raw device)\n");
        return;
    }

    UINTN block_size = blk->Media->BlockSize;
    UINTN io_align = blk->Media->IoAlign;
    if (io_align == 0) {
        io_align = 1;
    }

    UINT64 max_blocks = blk->Media->LastBlock + 1;
    UINTN blocks = 1024;
    if (blocks > max_blocks) {
        blocks = (UINTN)max_blocks;
    }
    if (blocks == 0) {
        serial_write("[RSE] UEFI block device empty\n");
        return;
    }

    UINTN bytes = block_size * blocks;
    UINTN alloc_size = bytes + io_align;
    void *raw = NULL;
    EFI_STATUS alloc_status = st->BootServices->AllocatePool(
        EfiLoaderData, alloc_size, &raw);
    if (EFI_ERROR(alloc_status) || !raw) {
        serial_write("[RSE] UEFI block alloc failed\n");
        return;
    }
    UINTN raw_addr = (UINTN)raw;
    void *buf = (void *)((raw_addr + io_align - 1) & ~(io_align - 1));

    uint8_t *data = (uint8_t *)buf;
    for (UINTN i = 0; i < bytes; ++i) {
        data[i] = (uint8_t)(i ^ 0x3c);
    }

    serial_write("[RSE] UEFI block benchmark start\n");
    uint64_t start = rdtsc();
    EFI_STATUS status = blk->WriteBlocks(
        blk, blk->Media->MediaId, 0, bytes, buf);
    uint64_t mid = rdtsc();
    if (EFI_ERROR(status)) {
        serial_write("[RSE] UEFI block write failed\n");
        st->BootServices->FreePool(raw);
        return;
    }

    for (UINTN i = 0; i < bytes; ++i) {
        data[i] = 0;
    }
    status = blk->ReadBlocks(
        blk, blk->Media->MediaId, 0, bytes, buf);
    uint64_t end = rdtsc();
    if (EFI_ERROR(status)) {
        serial_write("[RSE] UEFI block read failed\n");
        st->BootServices->FreePool(raw);
        return;
    }

    uint64_t checksum = 0;
    for (UINTN i = 0; i < bytes; ++i) {
        checksum += data[i];
    }

    uint64_t write_cycles = mid - start;
    uint64_t read_cycles = end - mid;
    uint64_t cycles_per_byte_write = bytes ? (write_cycles / bytes) : 0;
    uint64_t cycles_per_byte_read = bytes ? (read_cycles / bytes) : 0;

    g_metrics.uefi_blk_bytes = bytes;
    g_metrics.uefi_blk_write_cycles = write_cycles;
    g_metrics.uefi_blk_read_cycles = read_cycles;
    g_metrics.uefi_blk_write_cycles_per_byte = cycles_per_byte_write;
    g_metrics.uefi_blk_read_cycles_per_byte = cycles_per_byte_read;

    serial_write("[RSE] UEFI block bytes=");
    serial_write_u64(bytes);
    serial_write(" write cycles=");
    serial_write_u64(write_cycles);
    serial_write(" write cycles/byte=");
    serial_write_u64(cycles_per_byte_write);
    serial_write(" read cycles=");
    serial_write_u64(read_cycles);
    serial_write(" read cycles/byte=");
    serial_write_u64(cycles_per_byte_read);
    serial_write(" checksum=");
    serial_write_u64(checksum);
    serial_write("\n");

    st->BootServices->FreePool(raw);
}

static void bench_virtio_block(void) {
    if (virtio_init() != 0) {
        g_metrics.virtio_blk_present = 0;
        g_metrics.virtio_blk_bytes = 0;
        serial_write("[RSE] virtio-blk not found\n");
        return;
    }

    static uint8_t buf[512];
    for (uint32_t i = 0; i < sizeof(buf); ++i) {
        buf[i] = (uint8_t)(i ^ 0x7c);
    }

    serial_write("[RSE] virtio-blk benchmark start\n");
    uint64_t start = rdtsc();
    if (virtio_blk_rw(0, buf, sizeof(buf), VIRTIO_BLK_T_OUT) != 0) {
        serial_write("[RSE] virtio-blk write failed\n");
        return;
    }
    uint64_t mid = rdtsc();
    for (uint32_t i = 0; i < sizeof(buf); ++i) {
        buf[i] = 0;
    }
    if (virtio_blk_rw(0, buf, sizeof(buf), VIRTIO_BLK_T_IN) != 0) {
        serial_write("[RSE] virtio-blk read failed\n");
        return;
    }
    uint64_t end = rdtsc();

    uint64_t checksum = 0;
    for (uint32_t i = 0; i < sizeof(buf); ++i) {
        checksum += buf[i];
    }

    uint64_t write_cycles = mid - start;
    uint64_t read_cycles = end - mid;
    uint64_t bytes = sizeof(buf);
    uint64_t cycles_per_byte_write = bytes ? (write_cycles / bytes) : 0;
    uint64_t cycles_per_byte_read = bytes ? (read_cycles / bytes) : 0;

    g_metrics.virtio_blk_present = 1;
    g_metrics.virtio_blk_bytes = bytes;
    g_metrics.virtio_blk_write_cycles = write_cycles;
    g_metrics.virtio_blk_read_cycles = read_cycles;
    g_metrics.virtio_blk_write_cycles_per_byte = cycles_per_byte_write;
    g_metrics.virtio_blk_read_cycles_per_byte = cycles_per_byte_read;

    serial_write("[RSE] virtio-blk bytes=");
    serial_write_u64(bytes);
    serial_write(" write cycles=");
    serial_write_u64(write_cycles);
    serial_write(" write cycles/byte=");
    serial_write_u64(cycles_per_byte_write);
    serial_write(" read cycles=");
    serial_write_u64(read_cycles);
    serial_write(" read cycles/byte=");
    serial_write_u64(cycles_per_byte_read);
    serial_write(" checksum=");
    serial_write_u64(checksum);
    serial_write("\n");
}

static void run_benchmarks(struct rse_boot_info *boot_info, int do_init) {
    memset(&g_metrics, 0, sizeof(g_metrics));
    serial_write("[RSE] benchmarks begin\n");
    if (do_init) {
        init_workloads();
    } else {
        serial_write("[RSE] benchmarks: skipping workload init\n");
    }
#if RSE_ENABLE_USERMODE
    if (g_os_initialized) {
        serial_write("[RSE] user mode exec smoke start\n");
        run_user_mode_smoke_guarded();
        serial_write("[RSE] user mode exec smoke done\n");
    }
#endif
    bench_compute();
    bench_memory();
    bench_files();
    bench_uefi_fs(boot_info);
    bench_uefi_block(boot_info);
    bench_virtio_block();
    bench_net_arp();
    bench_udp_http_server();
    bench_http_loopback();
    serial_write("[RSE] benchmarks end\n");
    g_metrics.metrics_valid = 1;
}

void *memcpy(void *dst, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < count; ++i) {
        d[i] = s[i];
    }
    return dst;
}

void *memset(void *dst, int value, size_t count) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < count; ++i) {
        d[i] = (uint8_t)value;
    }
    return dst;
}

void *__memcpy_chk(void *dst, const void *src, size_t count, size_t dstlen) {
    (void)dstlen;
    return memcpy(dst, src, count);
}

void *__memset_chk(void *dst, int value, size_t count, size_t dstlen) {
    (void)dstlen;
    return memset(dst, value, count);
}

static void fb_clear(struct limine_framebuffer *fb, uint32_t color) {
    uint32_t *pixels = (uint32_t *)fb->address;
    size_t pitch_pixels = fb->pitch / 4;
    for (size_t y = 0; y < fb->height; ++y) {
        uint32_t *row = pixels + y * pitch_pixels;
        for (size_t x = 0; x < fb->width; ++x) {
            row[x] = color;
        }
    }
}

static void fb_fill_rect(struct limine_framebuffer *fb, size_t x, size_t y,
                          size_t w, size_t h, uint32_t color) {
    if (!fb || !fb->address || w == 0 || h == 0) {
        return;
    }
    if (x >= fb->width || y >= fb->height) {
        return;
    }
    size_t max_w = fb->width - x;
    size_t max_h = fb->height - y;
    if (w > max_w) {
        w = max_w;
    }
    if (h > max_h) {
        h = max_h;
    }
    uint32_t *pixels = (uint32_t *)fb->address;
    size_t pitch_pixels = fb->pitch / 4;
    for (size_t py = 0; py < h; ++py) {
        uint32_t *row = pixels + (y + py) * pitch_pixels + x;
        for (size_t px = 0; px < w; ++px) {
            row[px] = color;
        }
    }
}

static void fb_draw_rect(struct limine_framebuffer *fb, size_t x, size_t y,
                         size_t w, size_t h, uint32_t color) {
    if (!fb || !fb->address || w < 2 || h < 2) {
        return;
    }
    fb_fill_rect(fb, x, y, w, 1, color);
    fb_fill_rect(fb, x, y + h - 1, w, 1, color);
    fb_fill_rect(fb, x, y, 1, h, color);
    fb_fill_rect(fb, x + w - 1, y, 1, h, color);
}

static void __attribute__((unused)) fb_draw_bar(struct limine_framebuffer *fb, uint32_t color) {
    uint32_t *pixels = (uint32_t *)fb->address;
    size_t pitch_pixels = fb->pitch / 4;
    size_t height = fb->height > 32 ? 32 : fb->height;
    for (size_t y = 0; y < height; ++y) {
        uint32_t *row = pixels + y * pitch_pixels;
        for (size_t x = 0; x < fb->width; ++x) {
            row[x] = color;
        }
    }
}

static const uint8_t font_5x7[96][7] = {
    [' ' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['-' - 32] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    ['.' - 32] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04},
    ['/' - 32] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},
    [':' - 32] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    ['0' - 32] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    ['1' - 32] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['2' - 32] = {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},
    ['3' - 32] = {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    ['4' - 32] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    ['5' - 32] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    ['6' - 32] = {0x0E, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x0E},
    ['7' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    ['8' - 32] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    ['9' - 32] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E},
    ['A' - 32] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['B' - 32] = {0x1E, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x00},
    ['C' - 32] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    ['D' - 32] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    ['E' - 32] = {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x1F, 0x00},
    ['F' - 32] = {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x00},
    ['G' - 32] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    ['H' - 32] = {0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
    ['I' - 32] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F},
    ['J' - 32] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
    ['K' - 32] = {0x11, 0x12, 0x1C, 0x12, 0x11, 0x11, 0x00},
    ['L' - 32] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    ['M' - 32] = {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11},
    ['N' - 32] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    ['O' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['P' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    ['Q' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    ['R' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    ['S' - 32] = {0x0F, 0x10, 0x0E, 0x01, 0x01, 0x1E, 0x00},
    ['T' - 32] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    ['U' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['V' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    ['W' - 32] = {0x11, 0x11, 0x11, 0x11, 0x15, 0x1B, 0x11},
    ['X' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    ['Y' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    ['Z' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
};

static const uint8_t *fb_glyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if ((unsigned char)c < 32 || (unsigned char)c > 126) {
        c = ' ';
    }
    return font_5x7[c - 32];
}

static void fb_draw_text(struct limine_framebuffer *fb, size_t x, size_t y,
                         const char *text, uint32_t color) {
    if (!fb || !fb->address || !text) {
        return;
    }
    uint32_t *pixels = (uint32_t *)fb->address;
    size_t pitch_pixels = fb->pitch / 4;
    size_t cursor_x = x;
    size_t cursor_y = y;

    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y += 10;
            continue;
        }
        const uint8_t *glyph = fb_glyph(*p);
        for (size_t row = 0; row < 7; ++row) {
            size_t py = cursor_y + row;
            if (py >= fb->height) {
                continue;
            }
            uint32_t *row_pixels = pixels + py * pitch_pixels;
            uint8_t bits = glyph[row];
            for (size_t col = 0; col < 5; ++col) {
                size_t px = cursor_x + col;
                if (px >= fb->width) {
                    continue;
                }
                if (bits & (1u << (4 - col))) {
                    row_pixels[px] = color;
                }
            }
        }
        cursor_x += 6;
    }
}

static size_t fb_cstr_len(const char *s) {
    size_t len = 0;
    if (!s) {
        return 0;
    }
    while (s[len]) {
        len++;
    }
    return len;
}

static void fb_draw_u64(struct limine_framebuffer *fb, size_t x, size_t y,
                        uint64_t value, uint32_t color) {
    char buf[21];
    int i = 0;
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        fb_draw_text(fb, x, y, buf, color);
        return;
    }
    while (value > 0 && i < (int)(sizeof(buf) - 1)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (int j = 0; j < i / 2; ++j) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    buf[i] = '\0';
    fb_draw_text(fb, x, y, buf, color);
}

static void fb_draw_label_u64(struct limine_framebuffer *fb, size_t x, size_t y,
                              const char *label, uint64_t value,
                              uint32_t label_color, uint32_t value_color) {
    fb_draw_text(fb, x, y, label, label_color);
    size_t offset = fb_cstr_len(label) * 6 + 6;
    fb_draw_u64(fb, x + offset, y, value, value_color);
}

static void fb_draw_panel(struct limine_framebuffer *fb, size_t x, size_t y,
                          size_t w, size_t h, uint32_t fill, uint32_t border) {
    fb_fill_rect(fb, x, y, w, h, fill);
    fb_draw_rect(fb, x, y, w, h, border);
}

static void ui_console_init(void) {
    for (size_t i = 0; i < UI_CONSOLE_LINES; ++i) {
        memset(g_console[i], 0, UI_CONSOLE_COLS + 1);
    }
    g_console_line = 0;
    g_console_col = 0;
    g_console_count = 1;
    g_console_inited = 1;
}

static void ui_console_newline(void) {
    if (!g_console_inited) {
        ui_console_init();
    }
    g_console[g_console_line][g_console_col] = '\0';
    g_console_line = (g_console_line + 1) % (int)UI_CONSOLE_LINES;
    g_console_col = 0;
    if (g_console_count < (int)UI_CONSOLE_LINES) {
        g_console_count++;
    }
    memset(g_console[g_console_line], 0, UI_CONSOLE_COLS + 1);
}

static void ui_console_putc(char c) {
    if (!g_console_inited) {
        ui_console_init();
    }
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        ui_console_newline();
        return;
    }
    if (g_console_col >= (int)UI_CONSOLE_COLS) {
        ui_console_newline();
    }
    g_console[g_console_line][g_console_col++] = c;
    if (g_console_col < (int)UI_CONSOLE_COLS) {
        g_console[g_console_line][g_console_col] = '\0';
    }
}

static void ui_console_write(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        ui_console_putc(*s++);
    }
}

static void fb_draw_console(struct limine_framebuffer *fb, size_t x, size_t y,
                            size_t w, size_t h) {
    if (w < 60 || h < 30) {
        return;
    }
    fb_fill_rect(fb, x, y, w, h, 0x00111820);
    fb_draw_rect(fb, x, y, w, h, 0x00304455);
    fb_draw_text(fb, x + 8, y + 6, "CONSOLE", UI_MUTED);

    size_t max_lines = (h > 20) ? (h - 18) / 10 : 0;
    size_t max_cols = (w / 6);
    if (max_lines == 0 || max_cols == 0) {
        return;
    }
    int lines_to_show = g_console_count;
    if (lines_to_show > (int)max_lines) {
        lines_to_show = (int)max_lines;
    }
    int start = g_console_line - lines_to_show + 1;
    while (start < 0) {
        start += (int)UI_CONSOLE_LINES;
    }
    for (int i = 0; i < lines_to_show; ++i) {
        int idx = (start + i) % (int)UI_CONSOLE_LINES;
        char buf[UI_CONSOLE_COLS + 1];
        size_t col = 0;
        while (col < UI_CONSOLE_COLS && col < max_cols && g_console[idx][col]) {
            buf[col] = g_console[idx][col];
            col++;
        }
        buf[col] = '\0';
        fb_draw_text(fb, x + 8, y + 18 + (size_t)i * 10, buf, UI_TEXT);
    }
}

static void ui_layout_icons(size_t panel_x, size_t panel_y, size_t panel_w, size_t panel_h) {
    size_t icon_h = 60;
    size_t icon_w = 70;
    size_t spacing = 10;
    size_t row_y = panel_y + panel_h - icon_h - 12;
    size_t total_w = icon_w * 3 + spacing * 2;
    size_t start_x = panel_x + (panel_w > total_w ? (panel_w - total_w) / 2 : 6);
    if (row_y + icon_h > panel_y + panel_h) {
        row_y = panel_y + 12;
    }

    g_icons[0] = (struct ui_icon){ "BENCH", UI_ACT_BENCH, start_x, row_y, icon_w, icon_h };
    g_icons[1] = (struct ui_icon){ "NET", UI_ACT_NET, start_x + icon_w + spacing, row_y, icon_w, icon_h };
    g_icons[2] = (struct ui_icon){ "RESET", UI_ACT_RESET, start_x + (icon_w + spacing) * 2, row_y, icon_w, icon_h };
}

static int ui_hit_test(size_t x, size_t y) {
    for (int i = 0; i < 3; ++i) {
        if (x >= g_icons[i].x && x < g_icons[i].x + g_icons[i].w &&
            y >= g_icons[i].y && y < g_icons[i].y + g_icons[i].h) {
            return i;
        }
    }
    return -1;
}

static void ui_draw_icons(struct limine_framebuffer *fb) {
    for (int i = 0; i < 3; ++i) {
        uint32_t fill = (i == g_ui_hover) ? 0x00283a4c : 0x00181f28;
        uint32_t border = (i == g_ui_hover) ? UI_ACCENT : 0x00304455;
        fb_fill_rect(fb, g_icons[i].x, g_icons[i].y, g_icons[i].w, g_icons[i].h, fill);
        fb_draw_rect(fb, g_icons[i].x, g_icons[i].y, g_icons[i].w, g_icons[i].h, border);
        size_t label_len = fb_cstr_len(g_icons[i].label);
        size_t label_x = g_icons[i].x + (g_icons[i].w > (label_len * 6) ? (g_icons[i].w - label_len * 6) / 2 : 2);
        size_t label_y = g_icons[i].y + g_icons[i].h - 12;
        fb_draw_text(fb, label_x, label_y, g_icons[i].label, UI_TEXT);
    }
}

static void fb_draw_cursor(struct limine_framebuffer *fb, size_t x, size_t y) {
    fb_fill_rect(fb, x, y, 6, 2, UI_ACCENT);
    fb_fill_rect(fb, x, y, 2, 6, UI_ACCENT);
}

static void fb_draw_dashboard(struct limine_framebuffer *fb) {
    if (!fb || !fb->address) {
        return;
    }
    size_t margin = fb->width < 800 ? 10 : 16;
    size_t bar_h = 36;
    fb_clear(fb, UI_BG);
    fb_fill_rect(fb, 0, 0, fb->width, bar_h, UI_BAR);
    fb_draw_text(fb, margin, 12, "RSE CONTROL DECK", UI_TEXT);
    fb_draw_text(fb, fb->width > (margin + 24) ? fb->width - margin - 24 : margin,
                 12, g_metrics.metrics_valid ? "LIVE" : "BOOT", UI_ACCENT);

    size_t panel_y = bar_h + margin;
    if (fb->height <= panel_y + margin + 40) {
        return;
    }
    size_t panel_h = fb->height - panel_y - margin;
    size_t panel_w = (fb->width - margin * 3) / 2;
    if (panel_w < 220) {
        panel_w = fb->width - margin * 2;
    }
    size_t left_x = margin;
    size_t right_x = (panel_w < fb->width - margin * 2) ? (margin * 2 + panel_w) : margin;

    fb_draw_panel(fb, left_x, panel_y, panel_w, panel_h, UI_PANEL_ALT, UI_ACCENT);
    if (right_x != left_x) {
        fb_draw_panel(fb, right_x, panel_y, panel_w, panel_h, UI_PANEL, UI_ACCENT);
    }

    size_t line = panel_y + 10;
    size_t line_step = 12;
    fb_draw_text(fb, left_x + 12, line, "SYSTEM", UI_ACCENT);
    line += 16;
    fb_draw_text(fb, left_x + 12, line, "BOOT: OK", UI_TEXT);
    line += line_step;
    fb_draw_text(fb, left_x + 12, line, "USERS: 3 TASKS", UI_TEXT);
    line += line_step;
    fb_draw_text(fb, left_x + 12, line,
                 g_metrics.net_arp_bytes ? "NET RX: OK" : "NET RX: ---",
                 g_metrics.net_arp_bytes ? UI_OK : UI_WARN);
    line += line_step;
    fb_draw_label_u64(fb, left_x + 12, line, "PROOF RX:", g_metrics.udp_rx, UI_MUTED, UI_TEXT);
    line += line_step;
    fb_draw_label_u64(fb, left_x + 12, line, "ARP RX:", g_metrics.net_arp_bytes, UI_MUTED, UI_TEXT);
    line += line_step;
    size_t mid = left_x + panel_w / 2;
    fb_draw_label_u64(fb, left_x + 12, line, "UDP:", g_metrics.udp_udp, UI_MUTED, UI_TEXT);
    if (mid > left_x + 12) {
        fb_draw_label_u64(fb, mid, line, "HTTP:", g_metrics.udp_http, UI_MUTED, UI_TEXT);
    }
    ui_layout_icons(left_x, panel_y, panel_w, panel_h);
    size_t console_top = line + line_step + 6;
    size_t console_bottom = (g_icons[0].y > 10) ? g_icons[0].y - 10 : console_top;
    if (console_bottom > console_top + 24) {
        fb_draw_console(fb, left_x + 12, console_top,
                        panel_w > 24 ? panel_w - 24 : panel_w,
                        console_bottom - console_top);
    }
    ui_draw_icons(fb);

    if (right_x == left_x) {
        return;
    }

    line = panel_y + 10;
    fb_draw_text(fb, right_x + 12, line, "BENCHMARKS", UI_ACCENT);
    line += 16;
    if (!g_metrics.metrics_valid) {
        fb_draw_text(fb, right_x + 12, line, "BENCHMARKS PENDING", UI_WARN);
        return;
    }

    fb_draw_label_u64(fb, right_x + 12, line, "CPU CYC/OP:", g_metrics.compute_cycles_per_op, UI_MUTED, UI_TEXT);
    line += line_step;
    fb_draw_label_u64(fb, right_x + 12, line, "MEM CYC/B:", g_metrics.memory_cycles_per_byte, UI_MUTED, UI_TEXT);
    line += line_step;
    fb_draw_label_u64(fb, right_x + 12, line, "RAMFS CYC/OP:", g_metrics.ramfs_cycles_per_op, UI_MUTED, UI_TEXT);
    line += line_step;
    fb_draw_label_u64(fb, right_x + 12, line, "UEFI FS CYC/OP:", g_metrics.uefi_fs_cycles_per_op, UI_MUTED, UI_TEXT);
    line += line_step;
    fb_draw_label_u64(fb, right_x + 12, line, "UEFI W CYC/B:", g_metrics.uefi_blk_write_cycles_per_byte, UI_MUTED, UI_TEXT);
    line += line_step;
    fb_draw_label_u64(fb, right_x + 12, line, "UEFI R CYC/B:", g_metrics.uefi_blk_read_cycles_per_byte, UI_MUTED, UI_TEXT);
    line += line_step;
    if (g_metrics.virtio_blk_present) {
        fb_draw_label_u64(fb, right_x + 12, line, "VBLK W CYC/B:", g_metrics.virtio_blk_write_cycles_per_byte, UI_MUTED, UI_TEXT);
        line += line_step;
        fb_draw_label_u64(fb, right_x + 12, line, "VBLK R CYC/B:", g_metrics.virtio_blk_read_cycles_per_byte, UI_MUTED, UI_TEXT);
        line += line_step;
    } else {
        fb_draw_text(fb, right_x + 12, line, "VBLK: NONE", UI_WARN);
        line += line_step;
    }
    fb_draw_label_u64(fb, right_x + 12, line, "HTTP CYC/REQ:", g_metrics.http_cycles_per_req, UI_MUTED, UI_TEXT);
}

static int64_t ui_scale_delta(int64_t delta) {
    if (delta == 0) {
        return 0;
    }
    int64_t scaled = delta / UI_POINTER_DIV;
    if (scaled == 0) {
        scaled = (delta > 0) ? 1 : -1;
    }
    return scaled;
}

static void ui_center_cursor_on_icon(int index) {
    if (index < 0 || index >= 3) {
        return;
    }
    g_cursor_x = g_icons[index].x + g_icons[index].w / 2;
    g_cursor_y = g_icons[index].y + g_icons[index].h / 2;
}

static void uefi_stall(EFI_SYSTEM_TABLE *st, UINTN usec) {
    if (st && st->BootServices && st->BootServices->Stall) {
        st->BootServices->Stall(usec);
        return;
    }
    for (volatile UINTN i = 0; i < usec * 10; ++i) {
        __asm__ volatile("pause");
    }
}

static int uefi_pointer_init(struct rse_boot_info *boot_info) {
    EFI_SYSTEM_TABLE *st = get_system_table(boot_info);
    if (!st || !st->BootServices) {
        return -1;
    }
    EFI_STATUS status = st->BootServices->LocateProtocol(
        (EFI_GUID *)&g_pointer_guid, NULL, (void **)&g_pointer);
    if (EFI_ERROR(status) || !g_pointer) {
        serial_write("[RSE] UEFI pointer unavailable\n");
        g_pointer = NULL;
        return -1;
    }
    status = g_pointer->Reset(g_pointer, TRUE);
    if (EFI_ERROR(status)) {
        serial_write("[RSE] UEFI pointer reset failed\n");
    }
    serial_write("[RSE] UEFI pointer online\n");
    return 0;
}

static int uefi_keyboard_init(struct rse_boot_info *boot_info) {
    EFI_SYSTEM_TABLE *st = get_system_table(boot_info);
    if (!st || !st->ConIn) {
        return -1;
    }
    EFI_STATUS status = st->ConIn->Reset(st->ConIn, TRUE);
    if (EFI_ERROR(status)) {
        serial_write("[RSE] UEFI keyboard reset failed\n");
    }
    serial_write("[RSE] UEFI keyboard online\n");
    return 0;
}

static int uefi_read_key(EFI_SYSTEM_TABLE *st, EFI_INPUT_KEY *out_key) {
    if (!st || !st->ConIn || !out_key) {
        return -1;
    }
    EFI_STATUS status = st->ConIn->ReadKeyStroke(st->ConIn, out_key);
    if (status == EFI_NOT_READY) {
        return 0;
    }
    if (EFI_ERROR(status)) {
        return -1;
    }
    return 1;
}

static void ui_redraw(struct limine_framebuffer *fb) {
    fb_draw_dashboard(fb);
    fb_draw_cursor(fb, g_cursor_x, g_cursor_y);
}

static void ui_run_action(enum ui_action action, struct rse_boot_info *boot_info) {
    switch (action) {
    case UI_ACT_BENCH:
        run_benchmarks(boot_info, 0);
        break;
    case UI_ACT_NET:
        bench_net_arp();
        bench_udp_http_server();
        bench_http_loopback();
        break;
    case UI_ACT_RESET:
        memset(&g_metrics, 0, sizeof(g_metrics));
        g_metrics.metrics_valid = 0;
        break;
    default:
        break;
    }
}

static void ui_event_loop(struct rse_boot_info *boot_info) {
    if (!g_framebuffer || !g_framebuffer->address) {
        hlt_loop();
    }
    EFI_SYSTEM_TABLE *st = get_system_table(boot_info);
    size_t max_x = g_framebuffer->width ? g_framebuffer->width - 1 : 0;
    size_t max_y = g_framebuffer->height ? g_framebuffer->height - 1 : 0;
    g_cursor_x = max_x / 2;
    g_cursor_y = max_y / 2;
    g_ui_hover = 0;
    ui_center_cursor_on_icon(g_ui_hover);
    ui_redraw(g_framebuffer);

    uint8_t last_left = 0;
    int last_click_icon = -1;
    uint64_t last_click_tick = 0;
    uint64_t tick = 0;

    for (;;) {
        int needs_redraw = 0;
        if (g_pointer) {
            EFI_SIMPLE_POINTER_STATE state;
            EFI_STATUS status = g_pointer->GetState(g_pointer, &state);
            if (!EFI_ERROR(status)) {
                int64_t dx = ui_scale_delta(state.RelativeMovementX);
                int64_t dy = ui_scale_delta(state.RelativeMovementY);
                if (dx || dy) {
                    int64_t new_x = (int64_t)g_cursor_x + dx;
                    int64_t new_y = (int64_t)g_cursor_y + dy;
                    if (new_x < 0) {
                        new_x = 0;
                    }
                    if (new_y < 0) {
                        new_y = 0;
                    }
                    if (new_x > (int64_t)max_x) {
                        new_x = (int64_t)max_x;
                    }
                    if (new_y > (int64_t)max_y) {
                        new_y = (int64_t)max_y;
                    }
                    g_cursor_x = (size_t)new_x;
                    g_cursor_y = (size_t)new_y;
                    needs_redraw = 1;
                }

                int hover = ui_hit_test(g_cursor_x, g_cursor_y);
                if (hover != g_ui_hover) {
                    g_ui_hover = hover;
                    needs_redraw = 1;
                }

                uint8_t left = state.LeftButton ? 1 : 0;
                if (left && !last_left) {
                    if (g_ui_hover >= 0) {
                        if (g_ui_hover == last_click_icon &&
                            (tick - last_click_tick) <= UI_DBLCLICK_TICKS) {
                            ui_run_action(g_icons[g_ui_hover].action, boot_info);
                            last_click_icon = -1;
                            last_click_tick = 0;
                            needs_redraw = 1;
                        } else {
                            last_click_icon = g_ui_hover;
                            last_click_tick = tick;
                        }
                    } else {
                        last_click_icon = -1;
                        last_click_tick = 0;
                    }
                }
                last_left = left;
            }
        }

        if (st && st->ConIn) {
            EFI_INPUT_KEY key;
            int got = uefi_read_key(st, &key);
            if (got > 0) {
                int new_hover = g_ui_hover;
                if (key.ScanCode == SCAN_LEFT || key.UnicodeChar == 'a' || key.UnicodeChar == 'A') {
                    new_hover = (g_ui_hover + 2) % 3;
                } else if (key.ScanCode == SCAN_RIGHT || key.UnicodeChar == 'd' || key.UnicodeChar == 'D') {
                    new_hover = (g_ui_hover + 1) % 3;
                } else if (key.UnicodeChar == '\t') {
                    new_hover = (g_ui_hover + 1) % 3;
                } else if (key.UnicodeChar == ' ') {
                    ui_run_action(g_icons[g_ui_hover].action, boot_info);
                    needs_redraw = 1;
                } else if (key.UnicodeChar == '\r') {
                    ui_run_action(g_icons[g_ui_hover].action, boot_info);
                    needs_redraw = 1;
                } else if (key.UnicodeChar == 'b' || key.UnicodeChar == 'B') {
                    ui_run_action(UI_ACT_BENCH, boot_info);
                    needs_redraw = 1;
                } else if (key.UnicodeChar == 'n' || key.UnicodeChar == 'N') {
                    ui_run_action(UI_ACT_NET, boot_info);
                    needs_redraw = 1;
                } else if (key.UnicodeChar == 'r' || key.UnicodeChar == 'R') {
                    ui_run_action(UI_ACT_RESET, boot_info);
                    needs_redraw = 1;
                }

                if (new_hover != g_ui_hover) {
                    g_ui_hover = new_hover;
                    ui_center_cursor_on_icon(g_ui_hover);
                    needs_redraw = 1;
                }
            }
        }
        if (needs_redraw) {
            ui_redraw(g_framebuffer);
        }
        uefi_stall(st, UI_TICK_USEC);
        tick++;
    }
}

static void kmain(struct rse_boot_info *boot_info) {
    g_boot_info = boot_info;
    serial_init();
    serial_write("[RSE] UEFI kernel start\n");
    init_gdt_user_segments();
#if RSE_ENABLE_USERMODE
    run_user_mode_smoke_guarded();
#endif

    if (boot_info && boot_info->magic == RSE_BOOT_MAGIC) {
        g_uefi_framebuffer.address = (void *)(uintptr_t)boot_info->fb_addr;
        g_uefi_framebuffer.width = boot_info->fb_width;
        g_uefi_framebuffer.height = boot_info->fb_height;
        g_uefi_framebuffer.pitch = boot_info->fb_pitch;
        g_uefi_framebuffer.bpp = (uint16_t)boot_info->fb_bpp;
        g_framebuffer = &g_uefi_framebuffer;
        serial_write("[RSE] UEFI framebuffer online\n");
    } else if (LIMINE_BASE_REVISION_SUPPORTED &&
               framebuffer_request.response &&
               framebuffer_request.response->framebuffer_count >= 1) {
        g_framebuffer = framebuffer_request.response->framebuffers[0];
        if (bootloader_request.response && bootloader_request.response->name) {
            serial_write("[RSE] Bootloader: ");
            serial_write(bootloader_request.response->name);
            serial_write("\n");
        }
        serial_write("[RSE] Limine framebuffer online\n");
    } else {
        serial_write("[RSE] No framebuffer available\n");
    }
    if (g_framebuffer) {
        memset(&g_metrics, 0, sizeof(g_metrics));
        g_metrics.metrics_valid = 0;
        fb_draw_dashboard(g_framebuffer);
    }
    run_benchmarks(boot_info, 1);
    if (g_framebuffer) {
        fb_draw_dashboard(g_framebuffer);
    }
#if RSE_AUTO_EXIT
    serial_write("[RSE] auto shutdown\n");
    rse_poweroff();
#endif
    if (g_framebuffer) {
        uefi_pointer_init(boot_info);
        uefi_keyboard_init(boot_info);
        ui_event_loop(boot_info);
    }
    hlt_loop();
}

void _start(void *boot_info) {
    kmain((struct rse_boot_info *)boot_info);
    hlt_loop();
}
