#pragma once

#include "Device.h"
#include <cstdint>
#include <cstring>

namespace os {

struct BlockDeviceData {
    uint32_t block_size;
};

inline int block_open(Device* dev) {
    (void)dev;
    return 0;
}

inline int block_close(Device* dev) {
    (void)dev;
    return 0;
}

inline ssize_t block_read(Device* dev, void* buf, size_t count) {
    (void)dev;
    (void)buf;
    (void)count;
    return -1;
}

inline ssize_t block_write(Device* dev, const void* buf, size_t count) {
    (void)dev;
    (void)buf;
    (void)count;
    return -1;
}

inline int block_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)dev;
    (void)request;
    (void)arg;
    return -1;
}

inline Device* create_block_device(const char* name, uint32_t block_size) {
    Device* dev = new Device();
    if (!dev) {
        return nullptr;
    }
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->type = DeviceType::BLOCK;
    BlockDeviceData* data = new BlockDeviceData();
    if (!data) {
        return dev;
    }
    data->block_size = block_size;
    dev->private_data = data;
    dev->open = block_open;
    dev->close = block_close;
    dev->read = block_read;
    dev->write = block_write;
    dev->ioctl = block_ioctl;
    return dev;
}

} // namespace os

