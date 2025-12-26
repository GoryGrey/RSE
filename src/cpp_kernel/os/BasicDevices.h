#pragma once

#include "Device.h"
#include <cstring>

namespace os {

inline int null_open(Device* dev) {
    (void)dev;
    return 0;
}

inline int null_close(Device* dev) {
    (void)dev;
    return 0;
}

inline ssize_t null_read(Device* dev, void* buf, size_t count) {
    (void)dev;
    (void)buf;
    (void)count;
    return 0;
}

inline ssize_t null_write(Device* dev, const void* buf, size_t count) {
    (void)dev;
    (void)buf;
    return (ssize_t)count;
}

inline int null_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)dev;
    (void)request;
    (void)arg;
    return -1;
}

inline Device* create_null_device() {
    Device* dev = new Device();
    strncpy(dev->name, "null", sizeof(dev->name) - 1);
    dev->type = DeviceType::CHARACTER;
    dev->private_data = nullptr;
    dev->open = null_open;
    dev->close = null_close;
    dev->read = null_read;
    dev->write = null_write;
    dev->ioctl = null_ioctl;
    return dev;
}

inline int zero_open(Device* dev) {
    (void)dev;
    return 0;
}

inline int zero_close(Device* dev) {
    (void)dev;
    return 0;
}

inline ssize_t zero_read(Device* dev, void* buf, size_t count) {
    (void)dev;
    if (buf && count) {
        std::memset(buf, 0, count);
    }
    return (ssize_t)count;
}

inline ssize_t zero_write(Device* dev, const void* buf, size_t count) {
    (void)dev;
    (void)buf;
    return (ssize_t)count;
}

inline int zero_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)dev;
    (void)request;
    (void)arg;
    return -1;
}

inline Device* create_zero_device() {
    Device* dev = new Device();
    strncpy(dev->name, "zero", sizeof(dev->name) - 1);
    dev->type = DeviceType::CHARACTER;
    dev->private_data = nullptr;
    dev->open = zero_open;
    dev->close = zero_close;
    dev->read = zero_read;
    dev->write = zero_write;
    dev->ioctl = zero_ioctl;
    return dev;
}

} // namespace os

