#pragma once

#include "Device.h"
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace os {

#ifdef RSE_KERNEL
extern "C" int rse_net_init(void);
extern "C" int rse_net_read(void* buf, uint32_t len);
extern "C" int rse_net_write(const void* buf, uint32_t len);
#else
struct NetLoopback {
    static constexpr size_t CAPACITY = 16384;
    uint8_t buffer[CAPACITY];
    size_t head;
    size_t tail;
    size_t size;
    bool online;

    NetLoopback() : head(0), tail(0), size(0), online(false) {
        std::memset(buffer, 0, sizeof(buffer));
    }
};

inline NetLoopback& net_loopback() {
    static NetLoopback state;
    return state;
}

inline int rse_net_init(void) {
    NetLoopback& state = net_loopback();
    state.online = true;
    return 0;
}

inline int rse_net_read(void* buf, uint32_t len) {
    NetLoopback& state = net_loopback();
    if (!state.online || !buf || len == 0) {
        return state.online ? 0 : -1;
    }
    if (state.size == 0) {
        return 0;
    }
    uint32_t to_read = len < state.size ? len : (uint32_t)state.size;
    uint8_t* out = static_cast<uint8_t*>(buf);
    for (uint32_t i = 0; i < to_read; ++i) {
        out[i] = state.buffer[state.head];
        state.head = (state.head + 1) % NetLoopback::CAPACITY;
    }
    state.size -= to_read;
    return (int)to_read;
}

inline int rse_net_write(const void* buf, uint32_t len) {
    NetLoopback& state = net_loopback();
    if (!state.online || !buf || len == 0) {
        return state.online ? 0 : -1;
    }
    size_t space = NetLoopback::CAPACITY - state.size;
    uint32_t to_write = len < space ? len : (uint32_t)space;
    const uint8_t* in = static_cast<const uint8_t*>(buf);
    for (uint32_t i = 0; i < to_write; ++i) {
        state.buffer[state.tail] = in[i];
        state.tail = (state.tail + 1) % NetLoopback::CAPACITY;
    }
    state.size += to_write;
    return (int)to_write;
}
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
