#include "../os/NetDevice.h"
#include "../os/MemFS.h"
#include "../os/VFS.h"

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

    const char payload[] = "net-loopback";
    int64_t wrote = vfs.write(&fdt, fd, payload, sizeof(payload) - 1);
    assert(wrote == static_cast<int64_t>(sizeof(payload) - 1));

    char out[32] = {};
    int64_t read = vfs.read(&fdt, fd, out, sizeof(payload) - 1);
    assert(read == static_cast<int64_t>(sizeof(payload) - 1));
    assert(std::memcmp(out, payload, sizeof(payload) - 1) == 0);

    int64_t empty = vfs.read(&fdt, fd, out, sizeof(out));
    assert(empty == 0);

    vfs.close(&fdt, fd);

    std::cout << "  ✓ all tests passed" << std::endl;
    return 0;
}
