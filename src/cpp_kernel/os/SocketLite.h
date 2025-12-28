#pragma once

#include "Device.h"
#include "Syscall.h"
#include <cstdint>
#include <cstring>

namespace os {

struct SocketLite {
    enum class State : uint8_t {
        CREATED,
        BOUND,
        LISTENING,
        CONNECTED,
        CLOSED
    };

    static constexpr size_t kBufferSize = 8192;

    State state;
    uint16_t port;
    uint16_t peer_port;
    SocketLite* peer;
    SocketLite* pending;
    Device device;
    uint8_t buffer[kBufferSize];
    size_t head;
    size_t tail;
    size_t size;

    SocketLite()
        : state(State::CLOSED),
          port(0),
          peer_port(0),
          peer(nullptr),
          pending(nullptr),
          head(0),
          tail(0),
          size(0) {
        std::memset(buffer, 0, sizeof(buffer));
    }
};

class SocketManager {
public:
    static constexpr uint32_t kMaxSockets = 64;

    SocketManager() : next_ephemeral_(40000) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            in_use_[i] = false;
        }
    }

    SocketLite* allocate() {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                in_use_[i] = true;
                sockets_[i] = SocketLite();
                sockets_[i].state = SocketLite::State::CREATED;
                return &sockets_[i];
            }
        }
        return nullptr;
    }

    void release(SocketLite* sock) {
        if (!sock) {
            return;
        }
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (&sockets_[i] == sock) {
                SocketLite* pending = sock->pending;
                detach_peers(sock);
                if (pending && pending != sock) {
                    sock->pending = nullptr;
                    release(pending);
                }
                in_use_[i] = false;
                sockets_[i] = SocketLite();
                return;
            }
        }
    }

    SocketLite* find_listener(uint16_t port) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.state == SocketLite::State::LISTENING && sock.port == port) {
                return &sock;
            }
        }
        return nullptr;
    }

    bool port_in_use(uint16_t port) const {
        if (port == 0) {
            return false;
        }
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            const SocketLite& sock = sockets_[i];
            if (sock.state != SocketLite::State::CLOSED && sock.port == port) {
                return true;
            }
        }
        return false;
    }

    uint16_t allocate_ephemeral_port() {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            uint16_t port = next_ephemeral_++;
            if (port == 0) {
                continue;
            }
            if (!port_in_use(port)) {
                return port;
            }
        }
        return 0;
    }

private:
    void detach_peers(SocketLite* sock) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& other = sockets_[i];
            if (other.peer == sock) {
                other.peer = nullptr;
                other.state = SocketLite::State::CLOSED;
            }
            if (other.pending == sock) {
                other.pending = nullptr;
            }
        }
    }

    SocketLite sockets_[kMaxSockets];
    bool in_use_[kMaxSockets];
    uint16_t next_ephemeral_;
};

inline SocketManager& socket_manager() {
    static SocketManager mgr;
    return mgr;
}

inline int socket_open(Device* dev) {
    (void)dev;
    return 0;
}

inline int socket_close(Device* dev) {
    if (!dev) {
        return -1;
    }
    SocketLite* sock = static_cast<SocketLite*>(dev->private_data);
    socket_manager().release(sock);
    return 0;
}

inline ssize_t socket_read(Device* dev, void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    SocketLite* sock = static_cast<SocketLite*>(dev->private_data);
    if (!sock || sock->state != SocketLite::State::CONNECTED) {
        return -ENOTCONN;
    }
    if (sock->size == 0) {
        return -EAGAIN;
    }
    size_t to_read = count < sock->size ? count : sock->size;
    uint8_t* out = static_cast<uint8_t*>(buf);
    for (size_t i = 0; i < to_read; ++i) {
        out[i] = sock->buffer[sock->head];
        sock->head = (sock->head + 1) % SocketLite::kBufferSize;
    }
    sock->size -= to_read;
    return (ssize_t)to_read;
}

inline ssize_t socket_write(Device* dev, const void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    SocketLite* sock = static_cast<SocketLite*>(dev->private_data);
    if (!sock || sock->state != SocketLite::State::CONNECTED || !sock->peer) {
        return -ENOTCONN;
    }
    SocketLite* peer = sock->peer;
    size_t space = SocketLite::kBufferSize - peer->size;
    if (space == 0) {
        return -EAGAIN;
    }
    size_t to_write = count < space ? count : space;
    const uint8_t* in = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < to_write; ++i) {
        peer->buffer[peer->tail] = in[i];
        peer->tail = (peer->tail + 1) % SocketLite::kBufferSize;
    }
    peer->size += to_write;
    return (ssize_t)to_write;
}

inline int socket_ioctl(Device* dev, unsigned long request, void* arg) {
    (void)dev;
    (void)request;
    (void)arg;
    return -1;
}

inline Device* create_socket_device(SocketLite* sock) {
    if (!sock) {
        return nullptr;
    }
    Device* dev = &sock->device;
    *dev = Device();
    std::strncpy(dev->name, "sock", sizeof(dev->name) - 1);
    dev->type = DeviceType::CHARACTER;
    dev->private_data = sock;
    dev->open = socket_open;
    dev->close = socket_close;
    dev->read = socket_read;
    dev->write = socket_write;
    dev->ioctl = socket_ioctl;
    return dev;
}

inline bool is_socket_device(const Device* dev) {
    return dev && dev->ioctl == socket_ioctl;
}

} // namespace os
