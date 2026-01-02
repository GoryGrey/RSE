#include "../os/SyscallDispatcher.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
#include "../os/BlockFS.h"
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

static uint64_t write_user_string(os::OSProcess& proc, const char* value) {
    size_t len = std::strlen(value) + 1;
    uint64_t addr = proc.vmem->allocate(len);
    assert(addr != 0);
    bool ok = proc.vmem->writeUser(addr, value, len);
    assert(ok);
    return addr;
}

int main() {
    std::cout << "[sys_vfs_persist Tests]" << std::endl;

    alignas(os::PAGE_SIZE) std::array<uint8_t, 1 << 20> phys{};
    os::PhysicalAllocator phys_alloc(reinterpret_cast<uint64_t>(phys.data()), phys.size());

    os::MemFS memfs;
    os::VFS vfs(&memfs);

    os::rse_block_configure(512, 20000);
    os::BlockFS blockfs;
    bool mounted = blockfs.mount(512, os::rse_block_total_blocks());
    assert(mounted);
    vfs.setBlockFS(&blockfs);

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

    const char mem_path[] = "/hello.txt";
    uint64_t mem_path_addr = write_user_string(proc, mem_path);
    int64_t mem_fd = os::syscall(os::SYS_OPEN, mem_path_addr,
                                 os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(mem_fd >= 0);

    const char payload[] = "memfs";
    uint64_t payload_addr = proc.vmem->allocate(sizeof(payload) - 1);
    assert(payload_addr != 0);
    assert(proc.vmem->writeUser(payload_addr, payload, sizeof(payload) - 1));
    int64_t wrote = os::syscall(os::SYS_WRITE, mem_fd, payload_addr, sizeof(payload) - 1);
    assert(wrote == static_cast<int64_t>(sizeof(payload) - 1));
    (void)os::syscall(os::SYS_LSEEK, mem_fd, 0, SEEK_SET);

    std::array<char, 16> mem_out{};
    uint64_t mem_out_addr = proc.vmem->allocate(mem_out.size());
    assert(mem_out_addr != 0);
    int64_t mem_read = os::syscall(os::SYS_READ, mem_fd, mem_out_addr, sizeof(payload) - 1);
    assert(mem_read == static_cast<int64_t>(sizeof(payload) - 1));
    assert(proc.vmem->readUser(mem_out.data(), mem_out_addr, mem_out.size()));
    assert(std::memcmp(mem_out.data(), payload, sizeof(payload) - 1) == 0);

    const char persist_path[] = "/persist/alpha.txt";
    uint64_t persist_addr = write_user_string(proc, persist_path);
    int64_t persist_fd = os::syscall(os::SYS_OPEN, persist_addr,
                                     os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(persist_fd >= 0);

    const char persist_payload[] = "blockfs";
    uint64_t persist_payload_addr = proc.vmem->allocate(sizeof(persist_payload) - 1);
    assert(persist_payload_addr != 0);
    assert(proc.vmem->writeUser(persist_payload_addr, persist_payload, sizeof(persist_payload) - 1));
    int64_t persist_wrote = os::syscall(os::SYS_WRITE, persist_fd,
                                        persist_payload_addr, sizeof(persist_payload) - 1);
    assert(persist_wrote == static_cast<int64_t>(sizeof(persist_payload) - 1));
    (void)os::syscall(os::SYS_LSEEK, persist_fd, 0, SEEK_SET);

    std::array<char, 16> persist_out{};
    uint64_t persist_out_addr = proc.vmem->allocate(persist_out.size());
    assert(persist_out_addr != 0);
    int64_t persist_read = os::syscall(os::SYS_READ, persist_fd,
                                       persist_out_addr, sizeof(persist_payload) - 1);
    assert(persist_read == static_cast<int64_t>(sizeof(persist_payload) - 1));
    assert(proc.vmem->readUser(persist_out.data(), persist_out_addr, persist_out.size()));
    assert(std::memcmp(persist_out.data(), persist_payload, sizeof(persist_payload) - 1) == 0);

    uint64_t stat_addr = proc.vmem->allocate(sizeof(os::rse_stat));
    assert(stat_addr != 0);
    int64_t stat_rc = os::syscall(os::SYS_STAT, persist_addr, stat_addr);
    assert(stat_rc == 0);
    os::rse_stat st{};
    assert(proc.vmem->readUser(&st, stat_addr, sizeof(st)));
    assert(st.size == sizeof(persist_payload) - 1);
    assert(st.type == os::RSE_STAT_FILE);

    const char persist_root[] = "/persist";
    uint64_t persist_root_addr = write_user_string(proc, persist_root);
    std::array<char, 128> list_buf{};
    uint64_t list_addr = proc.vmem->allocate(list_buf.size());
    assert(list_addr != 0);
    int64_t list_rc = os::syscall(os::SYS_LIST, persist_root_addr,
                                  list_addr, list_buf.size());
    assert(list_rc >= 0);
    assert(proc.vmem->readUser(list_buf.data(), list_addr, list_buf.size()));
    assert(std::strstr(list_buf.data(), "alpha.txt") != nullptr);

    const char persist_dir[] = "/persist/dir";
    uint64_t dir_addr = write_user_string(proc, persist_dir);
    int64_t mkdir_rc = os::syscall(os::SYS_MKDIR, dir_addr, 0755);
    assert(mkdir_rc == 0);
    int64_t dir_open = os::syscall(os::SYS_OPEN, dir_addr, os::O_RDONLY);
    assert(dir_open == -os::EISDIR);

    list_buf.fill(0);
    uint64_t root_list_addr = proc.vmem->allocate(list_buf.size());
    assert(root_list_addr != 0);
    int64_t root_list_rc = os::syscall(os::SYS_LIST, persist_root_addr,
                                       root_list_addr, list_buf.size());
    assert(root_list_rc >= 0);
    assert(proc.vmem->readUser(list_buf.data(), root_list_addr, list_buf.size()));
    assert(std::strstr(list_buf.data(), "dir/") != nullptr);

    uint64_t dir_stat_addr = proc.vmem->allocate(sizeof(os::rse_stat));
    assert(dir_stat_addr != 0);
    int64_t dir_stat_rc = os::syscall(os::SYS_STAT, dir_addr, dir_stat_addr);
    assert(dir_stat_rc == 0);
    os::rse_stat dir_stat{};
    assert(proc.vmem->readUser(&dir_stat, dir_stat_addr, sizeof(dir_stat)));
    assert(dir_stat.type == os::RSE_STAT_DIR);

    const char persist_child[] = "/persist/dir/name.txt";
    uint64_t child_addr = write_user_string(proc, persist_child);
    int64_t child_fd = os::syscall(os::SYS_OPEN, child_addr,
                                   os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(child_fd >= 0);

    const char invalid_persist[] = "/persist/dir//name";
    uint64_t invalid_addr = write_user_string(proc, invalid_persist);
    int64_t invalid_rc = os::syscall(os::SYS_OPEN, invalid_addr,
                                     os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(invalid_rc == -os::EINVAL);
    os::FileDescriptorTable direct_fdt;
    int32_t direct_invalid = vfs.open(&direct_fdt, invalid_persist,
                                      os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(direct_invalid == -os::EINVAL);

    const char dot_persist[] = "/persist/./bad";
    uint64_t dot_addr = write_user_string(proc, dot_persist);
    int64_t dot_rc = os::syscall(os::SYS_OPEN, dot_addr,
                                 os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(dot_rc == -os::EINVAL);

    const char dotdot_persist[] = "/persist/dir/../bad";
    uint64_t dotdot_addr = write_user_string(proc, dotdot_persist);
    int64_t dotdot_rc = os::syscall(os::SYS_OPEN, dotdot_addr,
                                    os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(dotdot_rc == -os::EINVAL);

    list_buf.fill(0);
    uint64_t dir_list_addr = proc.vmem->allocate(list_buf.size());
    assert(dir_list_addr != 0);
    int64_t dir_list_rc = os::syscall(os::SYS_LIST, dir_addr,
                                      dir_list_addr, list_buf.size());
    assert(dir_list_rc >= 0);
    assert(proc.vmem->readUser(list_buf.data(), dir_list_addr, list_buf.size()));
    assert(std::strstr(list_buf.data(), "name.txt") != nullptr);

    int64_t rm_dir_busy = os::syscall(os::SYS_UNLINK, dir_addr);
    assert(rm_dir_busy == -1);
    int64_t rm_child = os::syscall(os::SYS_UNLINK, child_addr);
    assert(rm_child == 0);
    int64_t rm_dir = os::syscall(os::SYS_UNLINK, dir_addr);
    assert(rm_dir == 0);

    const char persist_writeonly[] = "/persist/writeonly";
    uint64_t writeonly_addr = write_user_string(proc, persist_writeonly);
    int64_t writeonly_mkdir = os::syscall(os::SYS_MKDIR, writeonly_addr, 0300);
    assert(writeonly_mkdir == 0);

    std::array<char, 64> perm_list_buf{};
    uint64_t perm_list_addr = proc.vmem->allocate(perm_list_buf.size());
    assert(perm_list_addr != 0);
    int64_t perm_list_rc = os::syscall(os::SYS_LIST, writeonly_addr,
                                       perm_list_addr, perm_list_buf.size());
    assert(perm_list_rc == -os::EACCES);

    const char persist_writeonly_file[] = "/persist/writeonly/data.txt";
    uint64_t writeonly_file_addr = write_user_string(proc, persist_writeonly_file);
    int64_t writeonly_fd = os::syscall(os::SYS_OPEN, writeonly_file_addr,
                                       os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(writeonly_fd >= 0);
    (void)os::syscall(os::SYS_CLOSE, writeonly_fd);

    const char persist_noexec[] = "/persist/noexec";
    uint64_t noexec_addr = write_user_string(proc, persist_noexec);
    int64_t noexec_mkdir = os::syscall(os::SYS_MKDIR, noexec_addr, 0200);
    assert(noexec_mkdir == 0);

    const char persist_noexec_file[] = "/persist/noexec/data.txt";
    uint64_t noexec_file_addr = write_user_string(proc, persist_noexec_file);
    int64_t noexec_open = os::syscall(os::SYS_OPEN, noexec_file_addr,
                                      os::O_CREAT | os::O_TRUNC | os::O_RDWR);
    assert(noexec_open == -os::EACCES);

    const char persist_ro[] = "/persist/ro.txt";
    uint64_t ro_addr = write_user_string(proc, persist_ro);
    int64_t ro_fd = os::syscall(os::SYS_OPEN, ro_addr,
                                os::O_CREAT | os::O_TRUNC, 0400);
    assert(ro_fd >= 0);
    int64_t ro_close = os::syscall(os::SYS_CLOSE, ro_fd);
    assert(ro_close == 0);

    int64_t ro_open_write = os::syscall(os::SYS_OPEN, ro_addr, os::O_WRONLY);
    assert(ro_open_write == -os::EACCES);
    int64_t ro_open_read = os::syscall(os::SYS_OPEN, ro_addr, os::O_RDONLY);
    assert(ro_open_read >= 0);
    (void)os::syscall(os::SYS_CLOSE, ro_open_read);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
