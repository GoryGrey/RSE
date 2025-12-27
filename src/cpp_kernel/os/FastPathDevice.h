#pragma once

#include "Device.h"
#include <cstdint>
#include <cstring>

namespace os {

struct FastPathState {
    static constexpr uint32_t kBufferSize = 1u << 16;
    static constexpr uint32_t kMask = kBufferSize - 1u;
    uint8_t buffer[kBufferSize];
    uint32_t head;
    uint32_t tail;
};

constexpr unsigned long FASTPATH_IOCTL_RESET = 0x52534501ul;

inline void fastpath_reset(Device* dev) {
    if (!dev || !dev->private_data) {
        return;
    }
    FastPathState* st = static_cast<FastPathState*>(dev->private_data);
    st->head = 0;
    st->tail = 0;
    std::memset(st->buffer, 0, sizeof(st->buffer));
}

inline uint32_t fastpath_used(const FastPathState* st) {
    if (!st) {
        return 0;
    }
    if (st->head >= st->tail) {
        return st->head - st->tail;
    }
    return FastPathState::kBufferSize - (st->tail - st->head);
}

inline uint32_t fastpath_free(const FastPathState* st) {
    const uint32_t used = fastpath_used(st);
    if (used >= (FastPathState::kBufferSize - 1u)) {
        return 0;
    }
    return (FastPathState::kBufferSize - 1u) - used;
}

inline int fastpath_open(Device* dev) {
    return dev ? 0 : -1;
}

inline int fastpath_close(Device* dev) {
    return dev ? 0 : -1;
}

inline ssize_t fastpath_read(Device* dev, void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    FastPathState* st = static_cast<FastPathState*>(dev->private_data);
    if (!st) {
        return -1;
    }
    uint32_t available = fastpath_used(st);
    uint32_t to_read = count > available ? available : static_cast<uint32_t>(count);
    uint8_t* dst = static_cast<uint8_t*>(buf);
    for (uint32_t i = 0; i < to_read; ++i) {
        dst[i] = st->buffer[st->tail];
        st->tail = (st->tail + 1u) & FastPathState::kMask;
    }
    return static_cast<ssize_t>(to_read);
}

inline ssize_t fastpath_write(Device* dev, const void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    FastPathState* st = static_cast<FastPathState*>(dev->private_data);
    if (!st) {
        return -1;
    }
    uint32_t free = fastpath_free(st);
    uint32_t to_write = count > free ? free : static_cast<uint32_t>(count);
    const uint8_t* src = static_cast<const uint8_t*>(buf);
    for (uint32_t i = 0; i < to_write; ++i) {
        st->buffer[st->head] = src[i];
        st->head = (st->head + 1u) & FastPathState::kMask;
    }
    return static_cast<ssize_t>(to_write);
}

inline int fastpath_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)arg;
    if (!dev) {
        return -1;
    }
    if (request == FASTPATH_IOCTL_RESET) {
        fastpath_reset(dev);
        return 0;
    }
    return -1;
}

inline Device* create_fastpath_device(const char* name) {
    Device* dev = new Device();
    FastPathState* st = new FastPathState();
    if (!dev || !st) {
        return nullptr;
    }
    std::memset(st, 0, sizeof(FastPathState));
    const char* dev_name = name ? name : "fast0";
    std::strncpy(dev->name, dev_name, sizeof(dev->name) - 1);
    dev->name[sizeof(dev->name) - 1] = '\0';
    dev->type = DeviceType::CHARACTER;
    dev->private_data = st;
    dev->open = fastpath_open;
    dev->close = fastpath_close;
    dev->read = fastpath_read;
    dev->write = fastpath_write;
    dev->ioctl = fastpath_ioctl;
    return dev;
}

} // namespace os
