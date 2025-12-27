#pragma once

#include "Device.h"
#include "BlockFS.h"
#include <cstdint>
#include <cstring>
#include <new>

namespace os {

struct BlockDeviceData {
    uint32_t block_size;
    uint64_t cursor;
};

constexpr unsigned long BLOCK_IOCTL_GET_BLOCK_SIZE = 0x52534520ul;
constexpr unsigned long BLOCK_IOCTL_GET_TOTAL_BLOCKS = 0x52534521ul;

inline int block_open(Device* dev) {
    (void)dev;
    return 0;
}

inline int block_close(Device* dev) {
    (void)dev;
    return 0;
}

inline ssize_t block_read(Device* dev, void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    BlockDeviceData* data = static_cast<BlockDeviceData*>(dev->private_data);
    if (!data || data->block_size == 0) {
        return -1;
    }
    uint64_t block_size = data->block_size;
    uint64_t offset = data->cursor;
    uint8_t* out = static_cast<uint8_t*>(buf);
    uint32_t remaining = static_cast<uint32_t>(count);
    uint8_t* scratch = nullptr;
#ifdef RSE_KERNEL
    uint8_t stack_scratch[4096];
#endif
    if ((offset % block_size) != 0 || (remaining % block_size) != 0) {
#ifdef RSE_KERNEL
        if (block_size > sizeof(stack_scratch)) {
            return -1;
        }
        scratch = stack_scratch;
#else
        scratch = new (std::nothrow) uint8_t[block_size];
        if (!scratch) {
            return -1;
        }
#endif
    }

    if ((offset % block_size) != 0) {
        uint64_t lba = offset / block_size;
        if (rse_block_read(lba, scratch, 1) != 0) {
            delete[] scratch;
            return -1;
        }
        uint32_t block_off = static_cast<uint32_t>(offset % block_size);
        uint32_t take = static_cast<uint32_t>(block_size - block_off);
        if (take > remaining) {
            take = remaining;
        }
        std::memcpy(out, scratch + block_off, take);
        offset += take;
        out += take;
        remaining -= take;
    }

    uint32_t full_bytes = (remaining / block_size) * block_size;
    if (full_bytes > 0) {
        uint64_t lba = offset / block_size;
        uint32_t blocks = full_bytes / block_size;
        if (rse_block_read(lba, out, blocks) != 0) {
            delete[] scratch;
            return -1;
        }
        offset += full_bytes;
        out += full_bytes;
        remaining -= full_bytes;
    }

    if (remaining > 0) {
        uint64_t lba = offset / block_size;
        if (rse_block_read(lba, scratch, 1) != 0) {
            delete[] scratch;
            return -1;
        }
        std::memcpy(out, scratch, remaining);
        offset += remaining;
        remaining = 0;
    }

    #ifndef RSE_KERNEL
    delete[] scratch;
    #endif
    data->cursor = offset;
    return static_cast<ssize_t>(count);
}

inline ssize_t block_write(Device* dev, const void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    BlockDeviceData* data = static_cast<BlockDeviceData*>(dev->private_data);
    if (!data || data->block_size == 0) {
        return -1;
    }
    uint64_t block_size = data->block_size;
    uint64_t offset = data->cursor;
    const uint8_t* in = static_cast<const uint8_t*>(buf);
    uint32_t remaining = static_cast<uint32_t>(count);
    uint8_t* scratch = nullptr;
#ifdef RSE_KERNEL
    uint8_t stack_scratch[4096];
#endif
    if ((offset % block_size) != 0 || (remaining % block_size) != 0) {
#ifdef RSE_KERNEL
        if (block_size > sizeof(stack_scratch)) {
            return -1;
        }
        scratch = stack_scratch;
#else
        scratch = new (std::nothrow) uint8_t[block_size];
        if (!scratch) {
            return -1;
        }
#endif
    }

    if ((offset % block_size) != 0) {
        uint64_t lba = offset / block_size;
        if (rse_block_read(lba, scratch, 1) != 0) {
            delete[] scratch;
            return -1;
        }
        uint32_t block_off = static_cast<uint32_t>(offset % block_size);
        uint32_t take = static_cast<uint32_t>(block_size - block_off);
        if (take > remaining) {
            take = remaining;
        }
        std::memcpy(scratch + block_off, in, take);
        if (rse_block_write(lba, scratch, 1) != 0) {
            delete[] scratch;
            return -1;
        }
        offset += take;
        in += take;
        remaining -= take;
    }

    uint32_t full_bytes = (remaining / block_size) * block_size;
    if (full_bytes > 0) {
        uint64_t lba = offset / block_size;
        uint32_t blocks = full_bytes / block_size;
        if (rse_block_write(lba, in, blocks) != 0) {
            delete[] scratch;
            return -1;
        }
        offset += full_bytes;
        in += full_bytes;
        remaining -= full_bytes;
    }

    if (remaining > 0) {
        uint64_t lba = offset / block_size;
        if (rse_block_read(lba, scratch, 1) != 0) {
            delete[] scratch;
            return -1;
        }
        std::memcpy(scratch, in, remaining);
        if (rse_block_write(lba, scratch, 1) != 0) {
            delete[] scratch;
            return -1;
        }
        offset += remaining;
        remaining = 0;
    }

    #ifndef RSE_KERNEL
    delete[] scratch;
    #endif
    data->cursor = offset;
    return static_cast<ssize_t>(count);
}

inline int block_ioctl(Device* dev, unsigned long request, void* arg) {
    if (!dev) {
        return -1;
    }
    BlockDeviceData* data = static_cast<BlockDeviceData*>(dev->private_data);
    if (!data) {
        return -1;
    }
    if (request == BLOCK_IOCTL_GET_BLOCK_SIZE) {
        if (!arg) {
            return -1;
        }
        *static_cast<uint32_t*>(arg) = data->block_size;
        return 0;
    }
    if (request == BLOCK_IOCTL_GET_TOTAL_BLOCKS) {
        if (!arg) {
            return -1;
        }
        *static_cast<uint64_t*>(arg) = rse_block_total_blocks();
        return 0;
    }
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
    data->cursor = 0;
    dev->private_data = data;
    dev->open = block_open;
    dev->close = block_close;
    dev->read = block_read;
    dev->write = block_write;
    dev->ioctl = block_ioctl;
    return dev;
}

} // namespace os
