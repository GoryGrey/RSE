#pragma once

#include "Device.h"
#include <cstdint>
#include <cstring>

namespace os {

#ifdef RSE_KERNEL
extern "C" int rse_net_init(void);
extern "C" int rse_net_read(void* buf, uint32_t len);
extern "C" int rse_net_write(const void* buf, uint32_t len);
#else
inline int rse_net_init(void) { return -1; }
inline int rse_net_read(void* buf, uint32_t len) { (void)buf; (void)len; return -1; }
inline int rse_net_write(const void* buf, uint32_t len) { (void)buf; (void)len; return -1; }
#endif

inline int net_open(Device* dev) {
    (void)dev;
    return rse_net_init() == 0 ? 0 : -1;
}

inline int net_close(Device* dev) {
    (void)dev;
    return 0;
}

inline ssize_t net_read(Device* dev, void* buf, size_t count) {
    (void)dev;
    if (!buf || count == 0) {
        return 0;
    }
    int ret = rse_net_read(buf, (uint32_t)count);
    return ret < 0 ? -1 : (ssize_t)ret;
}

inline ssize_t net_write(Device* dev, const void* buf, size_t count) {
    (void)dev;
    if (!buf || count == 0) {
        return 0;
    }
    int ret = rse_net_write(buf, (uint32_t)count);
    return ret < 0 ? -1 : (ssize_t)ret;
}

inline int net_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)dev;
    (void)request;
    (void)arg;
    return -1;
}

inline Device* create_net_device(const char* name) {
    Device* dev = new Device();
    if (!dev) {
        return nullptr;
    }
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->type = DeviceType::CHARACTER;
    dev->private_data = nullptr;
    dev->open = net_open;
    dev->close = net_close;
    dev->read = net_read;
    dev->write = net_write;
    dev->ioctl = net_ioctl;
    return dev;
}

} // namespace os

