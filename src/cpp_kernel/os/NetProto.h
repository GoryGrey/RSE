#pragma once

#include "VFS.h"
#include "Syscall.h"
#include <cstdint>

namespace os {

struct TcpLiteHeader {
    uint32_t magic;
    uint16_t flags;
    uint16_t conn;
    uint32_t len;
};

static constexpr uint32_t kTcpLiteMagic = 0x52534554u; // "RSET"
static constexpr uint16_t kTcpLiteSyn = 1u << 0;
static constexpr uint16_t kTcpLiteAck = 1u << 1;
static constexpr uint16_t kTcpLiteFin = 1u << 2;
static constexpr uint16_t kTcpLiteData = 1u << 3;
static constexpr uint16_t kTcpLiteRst = 1u << 4;

inline int64_t tcp_lite_write(VFS* vfs, FileDescriptorTable* fdt, int32_t fd,
                              uint16_t conn, uint16_t flags,
                              const void* payload, uint32_t len) {
    if (!vfs || !fdt) {
        return -EINVAL;
    }
    TcpLiteHeader header{ kTcpLiteMagic, flags, conn, len };
    const uint8_t* src = static_cast<const uint8_t*>(payload);
    uint32_t written = 0;
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
    uint32_t header_remaining = sizeof(header);
    while (header_remaining > 0) {
        int64_t wrote = vfs->write(fdt, fd, header_bytes + written, header_remaining);
        if (wrote < 0) {
            return wrote;
        }
        if (wrote == 0) {
            return -EAGAIN;
        }
        header_remaining -= static_cast<uint32_t>(wrote);
        written += static_cast<uint32_t>(wrote);
    }
    uint32_t payload_written = 0;
    while (payload_written < len) {
        int64_t wrote = vfs->write(fdt, fd, src + payload_written, len - payload_written);
        if (wrote < 0) {
            return wrote;
        }
        if (wrote == 0) {
            return -EAGAIN;
        }
        payload_written += static_cast<uint32_t>(wrote);
    }
    return static_cast<int64_t>(sizeof(header) + len);
}

inline int64_t tcp_lite_read(VFS* vfs, FileDescriptorTable* fdt, int32_t fd,
                             TcpLiteHeader* out_header, void* payload,
                             uint32_t max_len, uint32_t* out_len) {
    if (!vfs || !fdt || !out_header || !payload || !out_len) {
        return -EINVAL;
    }
    uint8_t* header_bytes = reinterpret_cast<uint8_t*>(out_header);
    uint32_t read_bytes = 0;
    while (read_bytes < sizeof(TcpLiteHeader)) {
        int64_t got = vfs->read(fdt, fd, header_bytes + read_bytes,
                                sizeof(TcpLiteHeader) - read_bytes);
        if (got < 0) {
            return got;
        }
        if (got == 0) {
            return -EAGAIN;
        }
        read_bytes += static_cast<uint32_t>(got);
    }
    if (out_header->magic != kTcpLiteMagic) {
        return -EINVAL;
    }
    if (out_header->len > max_len) {
        return -EINVAL;
    }
    uint32_t payload_read = 0;
    uint8_t* dst = static_cast<uint8_t*>(payload);
    while (payload_read < out_header->len) {
        int64_t got = vfs->read(fdt, fd, dst + payload_read,
                                out_header->len - payload_read);
        if (got < 0) {
            return got;
        }
        if (got == 0) {
            return -EAGAIN;
        }
        payload_read += static_cast<uint32_t>(got);
    }
    *out_len = out_header->len;
    return static_cast<int64_t>(sizeof(TcpLiteHeader) + payload_read);
}

} // namespace os
