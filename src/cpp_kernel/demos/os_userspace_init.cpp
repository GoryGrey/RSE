#include "../os/SyscallDispatcher.h"
#include "../os/VFS.h"
#include "../os/MemFS.h"
#include "../os/FileDescriptor.h"
#include "../os/TorusScheduler.h"
#include "../os/ConsoleDevice.h"
#include "../os/BasicDevices.h"
#include "../os/LoopbackDevice.h"
#include "../os/NetDevice.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace os {
TorusContext* current_torus_context = nullptr;
}

static uint64_t xorshift64(uint64_t& state) {
    uint64_t x = state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state = x;
    return x;
}

static void format_name(char* buf, size_t idx) {
    buf[0] = 'f';
    buf[1] = 'i';
    buf[2] = 'l';
    buf[3] = 'e';
    buf[4] = (char)('0' + ((idx / 1000) % 10));
    buf[5] = (char)('0' + ((idx / 100) % 10));
    buf[6] = (char)('0' + ((idx / 10) % 10));
    buf[7] = (char)('0' + (idx % 10));
    buf[8] = '\0';
}

static void run_compute_bench() {
    std::cout << "\n[init] compute workload" << std::endl;
    const uint64_t iters = 2 * 1000 * 1000;
    uint64_t state = 0x123456789abcdef0ULL;
    uint64_t acc = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < iters; ++i) {
        uint64_t v = xorshift64(state);
        acc ^= (v + (i << 1));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = duration.count() / 1e6;
    double ops_per_sec = seconds > 0 ? (iters / seconds) : 0.0;

    std::cout << "  ops=" << iters
              << " duration_us=" << duration.count()
              << " ops/sec=" << (uint64_t)ops_per_sec
              << " checksum=" << acc << std::endl;
}

static void run_file_bench() {
    std::cout << "\n[init] memfs file I/O workload" << std::endl;
    const size_t file_count = 128;
    const size_t io_size = 4096;
    std::vector<uint8_t> write_buf(io_size);
    std::vector<uint8_t> read_buf(io_size);

    for (size_t i = 0; i < io_size; ++i) {
        write_buf[i] = (uint8_t)(i ^ 0x5a);
    }

    auto start = std::chrono::high_resolution_clock::now();
    size_t bytes_written = 0;
    size_t bytes_read = 0;
    size_t ops = 0;

    char name[16];
    for (size_t i = 0; i < file_count; ++i) {
        format_name(name, i);
        int64_t fd = os::open(name, os::O_CREAT | os::O_TRUNC | os::O_RDWR);
        if (fd < 0) {
            std::cout << "  open failed for " << name << std::endl;
            continue;
        }
        ops++;

        int64_t w = os::write(fd, write_buf.data(), write_buf.size());
        if (w > 0) {
            bytes_written += (size_t)w;
        }
        ops++;

        (void)os::lseek(fd, 0, 0);
        ops++;

        int64_t r = os::read(fd, read_buf.data(), read_buf.size());
        if (r > 0) {
            bytes_read += (size_t)r;
        }
        ops++;

        (void)os::close(fd);
        ops++;
    }

    for (size_t i = 0; i < file_count; ++i) {
        format_name(name, i);
        (void)os::unlink(name);
        ops++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = duration.count() / 1e6;
    double ops_per_sec = seconds > 0 ? (ops / seconds) : 0.0;

    std::cout << "  files=" << file_count
              << " bytes_written=" << bytes_written
              << " bytes_read=" << bytes_read
              << " ops=" << ops
              << " duration_us=" << duration.count()
              << " ops/sec=" << (uint64_t)ops_per_sec << std::endl;
}

int main() {
    std::cout << "\n[RSE] userspace init (simulated) starting" << std::endl;

    os::MemFS memfs;
    os::VFS vfs(&memfs);
    os::DeviceManager dev_mgr;
    os::Device* console = os::create_console_device();
    os::Device* dev_null = os::create_null_device();
    os::Device* dev_zero = os::create_zero_device();
    os::Device* dev_loop = os::create_loopback_device("loopback");
    dev_mgr.registerDevice(console);
    dev_mgr.registerDevice(dev_null);
    dev_mgr.registerDevice(dev_zero);
    if (dev_loop) {
        dev_mgr.registerDevice(dev_loop);
    }
    if (os::rse_net_init() == 0) {
        os::Device* dev_net = os::create_net_device("net0");
        if (dev_net) {
            dev_mgr.registerDevice(dev_net);
        }
    }
    vfs.setDeviceManager(&dev_mgr);
    os::TorusScheduler scheduler(0);
    os::SyscallDispatcher dispatcher;
    os::TorusContext torus;
    torus.scheduler = &scheduler;
    torus.dispatcher = &dispatcher;
    torus.vfs = &vfs;
    os::current_torus_context = &torus;

    uint32_t pid = os::allocate_pid();
    os::OSProcess* init = new os::OSProcess(pid, 0, 0);
    init->fd_table.bindStandardDevices(console);
    scheduler.addProcess(init);
    scheduler.tick();

    const char* banner = "[init] running workloads via syscalls\n";
    os::write(1, banner, std::strlen(banner));

    run_compute_bench();
    run_file_bench();

    std::cout << "\n[RSE] userspace init complete" << std::endl;
    return 0;
}
