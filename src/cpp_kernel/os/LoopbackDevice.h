#pragma once

#include "Device.h"
#include <cstdint>
#include <cstring>

namespace os {

struct LoopbackData {
    static constexpr size_t CAPACITY = 8192;
    uint8_t buffer[CAPACITY];
    size_t head;
    size_t tail;
    size_t size;

    LoopbackData() : head(0), tail(0), size(0) {
        std::memset(buffer, 0, sizeof(buffer));
    }
};

inline int loop_open(Device* dev) {
    (void)dev;
    return 0;
}

inline int loop_close(Device* dev) {
    (void)dev;
    return 0;
}

inline ssize_t loop_read(Device* dev, void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    LoopbackData* data = (LoopbackData*)dev->private_data;
    if (!data || data->size == 0) {
        return 0;
    }
    size_t to_read = count < data->size ? count : data->size;
    for (size_t i = 0; i < to_read; ++i) {
        ((uint8_t*)buf)[i] = data->buffer[data->head];
        data->head = (data->head + 1) % LoopbackData::CAPACITY;
    }
    data->size -= to_read;
    return (ssize_t)to_read;
}

inline ssize_t loop_write(Device* dev, const void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    LoopbackData* data = (LoopbackData*)dev->private_data;
    if (!data) {
        return -1;
    }
    size_t space = LoopbackData::CAPACITY - data->size;
    size_t to_write = count < space ? count : space;
    for (size_t i = 0; i < to_write; ++i) {
        data->buffer[data->tail] = ((const uint8_t*)buf)[i];
        data->tail = (data->tail + 1) % LoopbackData::CAPACITY;
    }
    data->size += to_write;
    return (ssize_t)to_write;
}

inline int loop_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)dev;
    (void)request;
    (void)arg;
    return -1;
}

inline Device* create_loopback_device(const char* name) {
    Device* dev = new Device();
    if (!dev) {
        return nullptr;
    }
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->type = DeviceType::CHARACTER;
    LoopbackData* data = new LoopbackData();
    if (!data) {
        return dev;
    }
    dev->private_data = data;
    dev->open = loop_open;
    dev->close = loop_close;
    dev->read = loop_read;
    dev->write = loop_write;
    dev->ioctl = loop_ioctl;
    return dev;
}

} // namespace os

