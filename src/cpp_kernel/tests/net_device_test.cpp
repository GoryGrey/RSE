#include "../os/NetDevice.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"
#include "../os/NetProto.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

int main() {
    std::cout << "[net_device Tests]" << std::endl;

    os::MemFS memfs;
    os::VFS vfs(&memfs);
    os::DeviceManager dev_mgr;
    os::Device* net = os::create_net_device("net0");
    assert(net != nullptr);
    bool ok = dev_mgr.registerDevice(net);
    assert(ok);
    vfs.setDeviceManager(&dev_mgr);

    os::FileDescriptorTable fdt;
    int32_t fd = vfs.open(&fdt, "/dev/net0", os::O_RDWR);
    assert(fd >= 0);

    int32_t server_fd = vfs.open(&fdt, "/dev/net0", os::O_RDWR);
    assert(server_fd >= 0);

    int32_t invalid = vfs.open(&fdt, "/dev/net0/extra", os::O_RDWR);
    assert(invalid == -os::EINVAL);

    int32_t missing = vfs.open(&fdt, "/dev/missing0", os::O_RDWR);
    assert(missing == -os::ENOENT);

    const char payload[] = "net-loopback";
    int64_t wrote = vfs.write(&fdt, fd, payload, sizeof(payload) - 1);
    assert(wrote == static_cast<int64_t>(sizeof(payload) - 1));

    char out[32] = {};
    int64_t read = vfs.read(&fdt, fd, out, sizeof(payload) - 1);
    assert(read == static_cast<int64_t>(sizeof(payload) - 1));
    assert(std::memcmp(out, payload, sizeof(payload) - 1) == 0);

    int64_t empty = vfs.read(&fdt, fd, out, sizeof(out));
    assert(empty == -EAGAIN);

    std::array<uint8_t, os::NetLoopback::CAPACITY> bulk{};
    for (size_t i = 0; i < bulk.size(); ++i) {
        bulk[i] = static_cast<uint8_t>(i & 0xFF);
    }
    int64_t bulk_written = vfs.write(&fdt, fd, bulk.data(), bulk.size());
    assert(bulk_written == static_cast<int64_t>(bulk.size()));

    int64_t full = vfs.write(&fdt, fd, payload, sizeof(payload) - 1);
    assert(full == -EAGAIN);

    int64_t drained = vfs.read(&fdt, fd, out, sizeof(out));
    assert(drained > 0);
    int64_t resumed = vfs.write(&fdt, fd, payload, sizeof(payload) - 1);
    assert(resumed == static_cast<int64_t>(sizeof(payload) - 1));

    std::array<uint8_t, 512> stress{};
    for (size_t i = 0; i < stress.size(); ++i) {
        stress[i] = static_cast<uint8_t>(i & 0xFF);
    }
    std::array<uint8_t, 512> drain{};
    size_t total_written = 0;
    const size_t target = os::NetLoopback::CAPACITY * 4;
    while (total_written < target) {
        int64_t wrote = vfs.write(&fdt, fd, stress.data(), stress.size());
        if (wrote == -EAGAIN) {
            int64_t got = vfs.read(&fdt, fd, drain.data(), drain.size());
            assert(got > 0 || got == -EAGAIN);
            continue;
        }
        assert(wrote > 0);
        total_written += static_cast<size_t>(wrote);
    }
    size_t drained_bytes = 0;
    for (;;) {
        int64_t got = vfs.read(&fdt, fd, drain.data(), drain.size());
        if (got == -EAGAIN) {
            break;
        }
        assert(got >= 0);
        drained_bytes += static_cast<size_t>(got);
    }
    assert(drained_bytes > 0);

    auto tcp_write_retry = [&](int32_t use_fd, uint16_t conn, uint16_t flags,
                               const void* data, uint32_t len) {
        for (int i = 0; i < 1000; ++i) {
            int64_t rc = os::tcp_lite_write(&vfs, &fdt, use_fd, conn, flags, data, len);
            if (rc == -EAGAIN) {
                continue;
            }
            assert(rc >= 0);
            return;
        }
        assert(false);
    };
    auto tcp_read_retry = [&](int32_t use_fd, os::TcpLiteHeader& header,
                              std::array<uint8_t, 64>& data, uint32_t& out_len) {
        for (int i = 0; i < 1000; ++i) {
            int64_t rc = os::tcp_lite_read(&vfs, &fdt, use_fd, &header,
                                           data.data(), data.size(), &out_len);
            if (rc == -EAGAIN) {
                continue;
            }
            assert(rc >= 0);
            return;
        }
        assert(false);
    };

    const uint16_t conn = 1;
    os::TcpLiteHeader header{};
    std::array<uint8_t, 64> tcp_buf{};
    uint32_t tcp_len = 0;

    tcp_write_retry(fd, conn, os::kTcpLiteSyn, nullptr, 0);
    tcp_read_retry(server_fd, header, tcp_buf, tcp_len);
    assert(header.flags == os::kTcpLiteSyn);
    tcp_write_retry(server_fd, conn, (uint16_t)(os::kTcpLiteSyn | os::kTcpLiteAck), nullptr, 0);
    tcp_read_retry(fd, header, tcp_buf, tcp_len);
    assert(header.flags == (os::kTcpLiteSyn | os::kTcpLiteAck));
    tcp_write_retry(fd, conn, os::kTcpLiteAck, nullptr, 0);
    tcp_read_retry(server_fd, header, tcp_buf, tcp_len);
    assert(header.flags == os::kTcpLiteAck);

    const char msg[] = "tcp-lite";
    tcp_write_retry(fd, conn, os::kTcpLiteData,
                    msg, static_cast<uint32_t>(sizeof(msg) - 1));
    tcp_read_retry(server_fd, header, tcp_buf, tcp_len);
    assert(header.flags == os::kTcpLiteData);
    assert(tcp_len == sizeof(msg) - 1);
    assert(std::memcmp(tcp_buf.data(), msg, sizeof(msg) - 1) == 0);

    tcp_write_retry(server_fd, conn, (uint16_t)(os::kTcpLiteData | os::kTcpLiteAck),
                    msg, static_cast<uint32_t>(sizeof(msg) - 1));
    tcp_read_retry(fd, header, tcp_buf, tcp_len);
    assert((header.flags & os::kTcpLiteData) != 0);
    assert((header.flags & os::kTcpLiteAck) != 0);

    vfs.close(&fdt, fd);
    vfs.close(&fdt, server_fd);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
