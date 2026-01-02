#pragma once

#include "Device.h"
#include "NetDevice.h"
#include "Syscall.h"
#include <cstdint>
#include <cstring>

namespace os {

struct TcpLiteHeader {
    uint32_t magic;
    uint16_t flags;
    uint16_t conn;
    uint32_t len;
    uint32_t seq;
    uint32_t ack;
};

static constexpr uint32_t kTcpLiteMagic = 0x52534554u; // "RSET"
static constexpr uint16_t kTcpLiteSyn = 1u << 0;
static constexpr uint16_t kTcpLiteAck = 1u << 1;
static constexpr uint16_t kTcpLiteFin = 1u << 2;
static constexpr uint16_t kTcpLiteData = 1u << 3;
static constexpr uint16_t kTcpLiteRst = 1u << 4;
static constexpr uint32_t kNetLiteRetryTicks = 8u;
static constexpr uint32_t kNetLiteConnectTimeout = 64u;
static constexpr uint8_t kNetLiteMaxRetries = 3u;
static constexpr uint8_t kNetLiteMaxBacklog = 4u;
static constexpr uint32_t kNetLiteDataRetryTicks = 8u;
static constexpr uint8_t kNetLiteDataMaxRetries = 5u;

inline uint32_t g_socket_net_ticks = 0;

struct TcpLiteSynPayload {
    uint16_t dest_port;
    uint16_t src_port;
};

struct SocketLite {
    enum class State : uint8_t {
        CREATED,
        BOUND,
        LISTENING,
        CONNECTING,
        CONNECTED,
        CLOSED
    };

    enum class Backend : uint8_t {
        LOOPBACK,
        NET_LITE
    };

    static constexpr size_t kBufferSize = 8192;

    State state;
    Backend backend;
    uint16_t port;
    uint16_t peer_port;
    uint16_t conn_id;
    uint32_t connect_deadline;
    uint32_t connect_retry;
    uint8_t connect_attempts;
    uint8_t backlog;
    uint8_t pending_count;
    bool peer_closed;
    bool reset_pending;
    uint32_t tx_seq;
    uint32_t rx_seq;
    uint32_t tx_pending_seq;
    uint32_t tx_pending_len;
    uint32_t tx_retry_at;
    uint8_t tx_attempts;
    bool tx_pending;
    SocketLite* peer;
    SocketLite* pending;
    SocketLite* pending_next;
    Device device;
    uint8_t buffer[kBufferSize];
    uint8_t tx_buffer[kBufferSize];
    size_t head;
    size_t tail;
    size_t size;

    SocketLite()
        : state(State::CLOSED),
          backend(Backend::LOOPBACK),
          port(0),
          peer_port(0),
          conn_id(0),
          connect_deadline(0),
          connect_retry(0),
          connect_attempts(0),
          backlog(0),
          pending_count(0),
          peer_closed(false),
          reset_pending(false),
          tx_seq(1),
          rx_seq(1),
          tx_pending_seq(0),
          tx_pending_len(0),
          tx_retry_at(0),
          tx_attempts(0),
          tx_pending(false),
          peer(nullptr),
          pending(nullptr),
          pending_next(nullptr),
          head(0),
          tail(0),
          size(0) {
        std::memset(buffer, 0, sizeof(buffer));
        std::memset(tx_buffer, 0, sizeof(tx_buffer));
    }
};

struct NetWireState {
    static constexpr size_t kCapacity = 16384;
    uint8_t buffer[kCapacity];
    size_t head;
    size_t tail;
    size_t size;
    uint32_t drops;

    NetWireState() : head(0), tail(0), size(0), drops(0) {
        std::memset(buffer, 0, sizeof(buffer));
    }
};

inline int net_send_frame(uint16_t conn, uint16_t flags, const void* payload,
                          uint32_t len, uint32_t seq = 0, uint32_t ack = 0);

class SocketManager {
public:
    static constexpr uint32_t kMaxSockets = 64;

    SocketManager() : next_ephemeral_(40000), next_conn_id_(1), net_online_(false) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            in_use_[i] = false;
        }
    }

    SocketLite* allocate(SocketLite::Backend backend) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                in_use_[i] = true;
                sockets_[i] = SocketLite();
                sockets_[i].state = SocketLite::State::CREATED;
                sockets_[i].backend = backend;
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
                sock->pending = nullptr;
                sock->pending_count = 0;
                while (pending) {
                    SocketLite* next = pending->pending_next;
                    pending->pending_next = nullptr;
                    if (pending != sock) {
                        release(pending);
                    }
                    pending = next;
                }
                in_use_[i] = false;
                sockets_[i] = SocketLite();
                return;
            }
        }
    }

    SocketLite* find_listener(uint16_t port, SocketLite::Backend backend) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.state == SocketLite::State::LISTENING && sock.port == port &&
                sock.backend == backend) {
                return &sock;
            }
        }
        return nullptr;
    }

    SocketLite* find_by_conn(uint16_t conn_id) {
        if (conn_id == 0) {
            return nullptr;
        }
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.backend == SocketLite::Backend::NET_LITE &&
                sock.state != SocketLite::State::CLOSED &&
                sock.conn_id == conn_id) {
                return &sock;
            }
        }
        return nullptr;
    }

    bool port_in_use(uint16_t port, SocketLite::Backend backend) const {
        if (port == 0) {
            return false;
        }
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            const SocketLite& sock = sockets_[i];
            if (sock.backend != backend) {
                continue;
            }
            if (sock.state != SocketLite::State::CLOSED && sock.port == port) {
                return true;
            }
        }
        return false;
    }

    uint16_t allocate_ephemeral_port(SocketLite::Backend backend) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            uint16_t port = next_ephemeral_++;
            if (port == 0) {
                continue;
            }
            if (!port_in_use(port, backend)) {
                return port;
            }
        }
        return 0;
    }

    uint16_t allocate_conn_id() {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            uint16_t candidate = next_conn_id_++;
            if (candidate == 0) {
                continue;
            }
            if (!find_by_conn(candidate)) {
                return candidate;
            }
        }
        return 0;
    }

    void ensure_net_online() {
        if (net_online_) {
            return;
        }
        if (rse_net_init() == 0) {
            net_online_ = true;
        }
    }

    bool net_online() const {
        return net_online_;
    }

    void service_netlite_tx(uint32_t now) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.backend != SocketLite::Backend::NET_LITE ||
                sock.state != SocketLite::State::CONNECTED ||
                !sock.tx_pending) {
                continue;
            }
            if (sock.peer_closed || sock.reset_pending) {
                sock.tx_pending = false;
                continue;
            }
            if (now < sock.tx_retry_at) {
                continue;
            }
            if (sock.tx_attempts >= kNetLiteDataMaxRetries) {
                sock.reset_pending = true;
                sock.state = SocketLite::State::CLOSED;
                sock.tx_pending = false;
                (void)net_send_frame(sock.conn_id, kTcpLiteRst, nullptr, 0);
                continue;
            }
            int rc = net_send_frame(sock.conn_id, kTcpLiteData,
                                    sock.tx_buffer, sock.tx_pending_len,
                                    sock.tx_pending_seq, sock.rx_seq);
            if (rc < 0) {
                continue;
            }
            sock.tx_attempts++;
            sock.tx_retry_at = now + kNetLiteDataRetryTicks;
        }
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
                other.reset_pending = true;
            }
            if (other.pending) {
                SocketLite* prev = nullptr;
                SocketLite* node = other.pending;
                while (node) {
                    if (node == sock) {
                        if (prev) {
                            prev->pending_next = node->pending_next;
                        } else {
                            other.pending = node->pending_next;
                        }
                        node->pending_next = nullptr;
                        if (other.pending_count > 0) {
                            other.pending_count--;
                        }
                        other.reset_pending = true;
                        break;
                    }
                    prev = node;
                    node = node->pending_next;
                }
            }
        }
    }

    SocketLite sockets_[kMaxSockets];
    bool in_use_[kMaxSockets];
    uint16_t next_ephemeral_;
    uint16_t next_conn_id_;
    bool net_online_;
};

inline SocketManager& socket_manager() {
    static SocketManager mgr;
    return mgr;
}

inline void socket_append_pending(SocketLite* listener, SocketLite* pending) {
    if (!listener || !pending) {
        return;
    }
    pending->pending_next = nullptr;
    if (!listener->pending) {
        listener->pending = pending;
    } else {
        SocketLite* tail = listener->pending;
        while (tail->pending_next) {
            tail = tail->pending_next;
        }
        tail->pending_next = pending;
    }
    if (listener->pending_count < UINT8_MAX) {
        listener->pending_count++;
    }
}

inline SocketLite* socket_pop_pending(SocketLite* listener) {
    if (!listener || !listener->pending) {
        return nullptr;
    }
    SocketLite* head = listener->pending;
    listener->pending = head->pending_next;
    head->pending_next = nullptr;
    if (listener->pending_count > 0) {
        listener->pending_count--;
    }
    return head;
}

inline NetWireState& net_wire_state() {
    static NetWireState state;
    return state;
}

inline NetWireState& net_out_state() {
    static NetWireState state;
    return state;
}

inline bool net_wire_push(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }
    NetWireState& wire = net_wire_state();
    size_t free_space = NetWireState::kCapacity - wire.size;
    if (len > free_space) {
        wire.drops++;
        return false;
    }
    size_t remaining = len;
    const uint8_t* src = data;
    while (remaining > 0 && wire.size < NetWireState::kCapacity) {
        wire.buffer[wire.tail] = *src++;
        wire.tail = (wire.tail + 1) % NetWireState::kCapacity;
        wire.size++;
        remaining--;
    }
    return remaining == 0;
}

inline bool net_out_push(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }
    NetWireState& out = net_out_state();
    size_t free_space = NetWireState::kCapacity - out.size;
    if (len > free_space) {
        out.drops++;
        return false;
    }
    size_t remaining = len;
    const uint8_t* src = data;
    while (remaining > 0 && out.size < NetWireState::kCapacity) {
        out.buffer[out.tail] = *src++;
        out.tail = (out.tail + 1) % NetWireState::kCapacity;
        out.size++;
        remaining--;
    }
    return remaining == 0;
}

inline bool net_wire_peek(uint8_t* out, size_t len) {
    if (!out || len == 0) {
        return false;
    }
    NetWireState& wire = net_wire_state();
    if (wire.size < len) {
        return false;
    }
    size_t idx = wire.head;
    for (size_t i = 0; i < len; ++i) {
        out[i] = wire.buffer[idx];
        idx = (idx + 1) % NetWireState::kCapacity;
    }
    return true;
}

inline bool net_wire_pop(uint8_t* out, size_t len) {
    if (!out || len == 0) {
        return false;
    }
    NetWireState& wire = net_wire_state();
    if (wire.size < len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        out[i] = wire.buffer[wire.head];
        wire.head = (wire.head + 1) % NetWireState::kCapacity;
    }
    wire.size -= len;
    return true;
}

inline void net_wire_consume(size_t len) {
    if (len == 0) {
        return;
    }
    NetWireState& wire = net_wire_state();
    size_t consume = len > wire.size ? wire.size : len;
    wire.head = (wire.head + consume) % NetWireState::kCapacity;
    wire.size -= consume;
}

inline void net_out_flush() {
    SocketManager& mgr = socket_manager();
    if (!mgr.net_online()) {
        return;
    }
    NetWireState& out = net_out_state();
    while (out.size > 0) {
        size_t contiguous = out.size;
        size_t until_wrap = NetWireState::kCapacity - out.head;
        if (contiguous > until_wrap) {
            contiguous = until_wrap;
        }
        if (contiguous == 0) {
            break;
        }
        int wrote = rse_net_write(out.buffer + out.head, (uint32_t)contiguous);
        if (wrote <= 0) {
            break;
        }
        out.head = (out.head + static_cast<size_t>(wrote)) % NetWireState::kCapacity;
        out.size -= static_cast<size_t>(wrote);
    }
}

inline void netlite_reset_txrx(SocketLite* sock) {
    if (!sock) {
        return;
    }
    sock->tx_seq = 1;
    sock->rx_seq = 1;
    sock->tx_pending_seq = 0;
    sock->tx_pending_len = 0;
    sock->tx_retry_at = 0;
    sock->tx_attempts = 0;
    sock->tx_pending = false;
}

inline int net_write_all(const uint8_t* data, uint32_t len) {
    if (!data || len == 0) {
        return 0;
    }
    uint32_t remaining = len;
    const uint8_t* src = data;
    while (remaining > 0) {
        int wrote = rse_net_write(src, remaining);
        if (wrote < 0) {
            return wrote;
        }
        if (wrote == 0) {
            return -EAGAIN;
        }
        remaining -= static_cast<uint32_t>(wrote);
        src += wrote;
    }
    return static_cast<int>(len);
}

inline int net_send_frame(uint16_t conn, uint16_t flags, const void* payload,
                          uint32_t len, uint32_t seq, uint32_t ack) {
    TcpLiteHeader header{ kTcpLiteMagic, flags, conn, len, seq, ack };
    if (!socket_manager().net_online()) {
        return -EIO;
    }
    size_t total = sizeof(header) + len;
    if (total > NetWireState::kCapacity) {
        return -EINVAL;
    }
    NetWireState& out = net_out_state();
    if (out.size + total > NetWireState::kCapacity) {
        out.drops++;
        return -EAGAIN;
    }
    if (!net_out_push(reinterpret_cast<const uint8_t*>(&header), sizeof(header))) {
        return -EAGAIN;
    }
    if (len > 0) {
        const uint8_t* data = static_cast<const uint8_t*>(payload);
        if (!net_out_push(data, len)) {
            return -EAGAIN;
        }
    }
    net_out_flush();
    return (int)total;
}

inline void net_dispatch_frame(const TcpLiteHeader& header, const uint8_t* payload, uint32_t len) {
    SocketManager& mgr = socket_manager();
    if (header.conn == 0) {
        return;
    }
    if (header.flags & kTcpLiteSyn) {
        if (len < sizeof(TcpLiteSynPayload)) {
            return;
        }
        if (mgr.find_by_conn(header.conn)) {
            return;
        }
        TcpLiteSynPayload syn{};
        std::memcpy(&syn, payload, sizeof(syn));
        SocketLite* listener = mgr.find_listener(syn.dest_port, SocketLite::Backend::NET_LITE);
        if (!listener) {
            (void)net_send_frame(header.conn, kTcpLiteRst, nullptr, 0);
            return;
        }
        if (listener->backlog == 0) {
            listener->backlog = 1;
        }
        if (listener->pending_count >= listener->backlog) {
            (void)net_send_frame(header.conn, kTcpLiteRst, nullptr, 0);
            return;
        }
        SocketLite* server_sock = mgr.allocate(SocketLite::Backend::NET_LITE);
        if (!server_sock) {
            (void)net_send_frame(header.conn, kTcpLiteRst, nullptr, 0);
            return;
        }
        server_sock->state = SocketLite::State::CONNECTED;
        server_sock->port = syn.dest_port;
        server_sock->peer_port = syn.src_port;
        server_sock->conn_id = header.conn;
        netlite_reset_txrx(server_sock);
        socket_append_pending(listener, server_sock);
        (void)net_send_frame(header.conn, kTcpLiteAck, nullptr, 0);
        return;
    }

    if (header.flags & kTcpLiteAck) {
        SocketLite* sock = mgr.find_by_conn(header.conn);
        if (sock && sock->state == SocketLite::State::CONNECTING) {
            sock->state = SocketLite::State::CONNECTED;
            sock->connect_attempts = 0;
            sock->connect_retry = 0;
            sock->connect_deadline = 0;
            netlite_reset_txrx(sock);
        } else if (sock && sock->state == SocketLite::State::CONNECTED) {
            if (sock->tx_pending && header.ack == sock->tx_pending_seq + 1) {
                sock->tx_pending = false;
                sock->tx_pending_seq = 0;
                sock->tx_pending_len = 0;
                sock->tx_attempts = 0;
                sock->tx_retry_at = 0;
                sock->tx_seq = header.ack;
            }
        }
        return;
    }

    if (header.flags & kTcpLiteFin) {
        SocketLite* sock = mgr.find_by_conn(header.conn);
        if (sock && (sock->state == SocketLite::State::CONNECTED ||
                     sock->state == SocketLite::State::CONNECTING)) {
            if (sock->state == SocketLite::State::CONNECTING) {
                sock->reset_pending = true;
            } else {
                sock->peer_closed = true;
            }
            sock->state = SocketLite::State::CLOSED;
            sock->peer_port = 0;
            sock->connect_attempts = 0;
            sock->connect_retry = 0;
            sock->connect_deadline = 0;
        }
        return;
    }

    if (header.flags & kTcpLiteRst) {
        SocketLite* sock = mgr.find_by_conn(header.conn);
        if (sock && sock->state != SocketLite::State::CLOSED) {
            sock->reset_pending = true;
            sock->peer_closed = false;
            sock->state = SocketLite::State::CLOSED;
            sock->peer_port = 0;
            sock->connect_attempts = 0;
            sock->connect_retry = 0;
            sock->connect_deadline = 0;
            sock->tx_pending = false;
            sock->tx_pending_seq = 0;
            sock->tx_pending_len = 0;
            sock->tx_attempts = 0;
            sock->tx_retry_at = 0;
        }
        return;
    }

    if (header.flags & kTcpLiteData) {
        SocketLite* sock = mgr.find_by_conn(header.conn);
        if (!sock || sock->state != SocketLite::State::CONNECTED) {
            return;
        }
        if (sock->size >= SocketLite::kBufferSize) {
            (void)net_send_frame(header.conn, kTcpLiteAck, nullptr, 0, 0, sock->rx_seq);
            return;
        }
        if (header.seq < sock->rx_seq) {
            (void)net_send_frame(header.conn, kTcpLiteAck, nullptr, 0, 0, sock->rx_seq);
            return;
        }
        if (header.seq > sock->rx_seq) {
            (void)net_send_frame(header.conn, kTcpLiteAck, nullptr, 0, 0, sock->rx_seq);
            return;
        }
        size_t space = SocketLite::kBufferSize - sock->size;
        size_t to_write = len < space ? len : space;
        for (size_t i = 0; i < to_write; ++i) {
            sock->buffer[sock->tail] = payload[i];
            sock->tail = (sock->tail + 1) % SocketLite::kBufferSize;
        }
        sock->size += to_write;
        if (to_write == len) {
            sock->rx_seq++;
        }
        (void)net_send_frame(header.conn, kTcpLiteAck, nullptr, 0, 0, sock->rx_seq);
    }
}

inline void socket_poll_net() {
    SocketManager& mgr = socket_manager();
    if (!mgr.net_online()) {
        return;
    }
    g_socket_net_ticks++;
    net_out_flush();
    uint8_t scratch[256];
    while (true) {
        int got = rse_net_read(scratch, sizeof(scratch));
        if (got <= 0) {
            break;
        }
        if (!net_wire_push(scratch, static_cast<size_t>(got))) {
            break;
        }
    }

    while (net_wire_state().size >= sizeof(TcpLiteHeader)) {
        TcpLiteHeader header{};
        if (!net_wire_peek(reinterpret_cast<uint8_t*>(&header), sizeof(header))) {
            break;
        }
        if (header.magic != kTcpLiteMagic) {
            net_wire_consume(1);
            continue;
        }
        if (header.len > SocketLite::kBufferSize) {
            size_t total = sizeof(TcpLiteHeader) + header.len;
            if (net_wire_state().size < total) {
                break;
            }
            net_wire_consume(total);
            continue;
        }
        size_t total = sizeof(TcpLiteHeader) + header.len;
        if (net_wire_state().size < total) {
            break;
        }
        net_wire_consume(sizeof(TcpLiteHeader));
        uint8_t payload[SocketLite::kBufferSize];
        if (header.len > 0) {
            (void)net_wire_pop(payload, header.len);
        }
        net_dispatch_frame(header, payload, header.len);
    }
    mgr.service_netlite_tx(g_socket_net_ticks);
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
    if (sock && sock->backend == SocketLite::Backend::NET_LITE &&
        sock->state == SocketLite::State::CONNECTED && sock->conn_id != 0 &&
        !sock->peer_closed) {
        (void)net_send_frame(sock->conn_id, kTcpLiteFin, nullptr, 0);
    }
    socket_manager().release(sock);
    return 0;
}

inline ssize_t socket_read(Device* dev, void* buf, size_t count) {
    if (!dev || !buf || count == 0) {
        return 0;
    }
    SocketLite* sock = static_cast<SocketLite*>(dev->private_data);
    if (!sock) {
        return -ENOTCONN;
    }
    if (sock->backend == SocketLite::Backend::NET_LITE) {
        socket_poll_net();
    }
    if (sock->state != SocketLite::State::CONNECTED &&
        sock->state != SocketLite::State::CLOSED) {
        return -ENOTCONN;
    }
    if (sock->size == 0) {
        if (sock->peer_closed) {
            return 0;
        }
        if (sock->reset_pending || sock->state == SocketLite::State::CLOSED) {
            return -ECONNRESET;
        }
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
    if (!sock) {
        return -ENOTCONN;
    }
    if (sock->backend == SocketLite::Backend::NET_LITE) {
        if (!sock || sock->state != SocketLite::State::CONNECTED) {
            if (sock && sock->peer_closed) {
                return -EPIPE;
            }
            if (sock && sock->reset_pending) {
                return -ECONNRESET;
            }
            return -ENOTCONN;
        }
        if (sock->peer_closed) {
            return -EPIPE;
        }
        if (sock->reset_pending) {
            return -ECONNRESET;
        }
        if (sock->tx_pending) {
            socket_poll_net();
            return -EAGAIN;
        }
        uint32_t to_write = count > SocketLite::kBufferSize
            ? (uint32_t)SocketLite::kBufferSize
            : (uint32_t)count;
        std::memcpy(sock->tx_buffer, buf, to_write);
        sock->tx_pending = true;
        sock->tx_pending_seq = sock->tx_seq;
        sock->tx_pending_len = to_write;
        sock->tx_attempts = 1;
        sock->tx_retry_at = g_socket_net_ticks + kNetLiteDataRetryTicks;
        int rc = net_send_frame(sock->conn_id, kTcpLiteData,
                                sock->tx_buffer, to_write,
                                sock->tx_pending_seq, sock->rx_seq);
        if (rc < 0) {
            return rc;
        }
        return (ssize_t)to_write;
    }
    if (!sock || sock->state != SocketLite::State::CONNECTED) {
        if (sock && sock->peer_closed) {
            return -EPIPE;
        }
        if (sock && sock->reset_pending) {
            return -ECONNRESET;
        }
        return -ENOTCONN;
    }
    if (sock->peer_closed) {
        return -EPIPE;
    }
    if (sock->reset_pending) {
        return -ECONNRESET;
    }
    if (!sock->peer) {
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
