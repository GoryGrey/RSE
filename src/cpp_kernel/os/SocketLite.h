#pragma once

#include "Device.h"
#include "NetDevice.h"
#include "Syscall.h"
#include <cstdint>
#include <cstring>

#ifndef RSE_NET_RAW
#define RSE_NET_RAW 0
#endif

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
static constexpr uint32_t kNetLiteCloseRetryTicks = 8u;
static constexpr uint8_t kNetLiteCloseMaxRetries = 3u;
static constexpr uint32_t kNetLiteCloseTimeout = 32u;
static constexpr uint32_t kTcpRetryTicks = 8u;
static constexpr uint32_t kTcpConnectTimeout = 128u;
static constexpr uint8_t kTcpMaxRetries = 4u;
static constexpr uint32_t kTcpDataRetryTicks = 8u;
static constexpr uint8_t kTcpDataMaxRetries = 5u;
static constexpr uint32_t kTcpCloseTimeout = 128u;
static constexpr uint16_t kTcpWindowBytes = 4096u;
static constexpr uint16_t kTcpMss = 1200u;
static constexpr uint32_t kArpEntryTtl = 256u;
static constexpr uint16_t kEthertypeIPv4 = 0x0800;
static constexpr uint16_t kEthertypeArp = 0x0806;
static constexpr uint8_t kIpProtoTcp = 6;
static constexpr uint8_t kTcpFlagFin = 0x01;
static constexpr uint8_t kTcpFlagSyn = 0x02;
static constexpr uint8_t kTcpFlagRst = 0x04;
static constexpr uint8_t kTcpFlagPsh = 0x08;
static constexpr uint8_t kTcpFlagAck = 0x10;

inline uint32_t g_socket_net_ticks = 0;

struct TcpLiteSynPayload {
    uint16_t dest_port;
    uint16_t src_port;
};

enum class TcpState : uint8_t {
    CLOSED,
    SYN_SENT,
    SYN_RCVD,
    ESTABLISHED,
    FIN_WAIT,
    CLOSE_WAIT,
    LAST_ACK
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
        NET_LITE,
        TCP
    };

    static constexpr size_t kBufferSize = 8192;

    State state;
    Backend backend;
    uint16_t port;
    uint16_t peer_port;
    uint16_t conn_id;
    uint32_t peer_ip;
    uint32_t local_ip;
    uint32_t connect_deadline;
    uint32_t connect_retry;
    uint8_t connect_attempts;
    uint8_t backlog;
    uint8_t pending_count;
    bool peer_closed;
    bool reset_pending;
    uint32_t tx_seq;
    uint32_t rx_seq;
    uint32_t syn_seq;
    uint32_t fin_seq;
    uint32_t tx_pending_seq;
    uint32_t tx_pending_len;
    uint32_t tx_retry_at;
    uint8_t tx_attempts;
    bool tx_pending;
    TcpState tcp_state;
    bool tcp_syn_pending;
    bool tcp_fin_pending;
    uint16_t peer_window;
    uint8_t peer_mac[6];
    uint8_t local_mac[6];
    uint32_t fin_retry_at;
    uint8_t fin_attempts;
    bool close_requested;
    uint32_t close_deadline;
    SocketLite* listener;
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
          peer_ip(0),
          local_ip(0),
          connect_deadline(0),
          connect_retry(0),
          connect_attempts(0),
          backlog(0),
          pending_count(0),
          peer_closed(false),
          reset_pending(false),
          tx_seq(1),
          rx_seq(1),
          syn_seq(0),
          fin_seq(0),
          tx_pending_seq(0),
          tx_pending_len(0),
          tx_retry_at(0),
          tx_attempts(0),
          tx_pending(false),
          tcp_state(TcpState::CLOSED),
          tcp_syn_pending(false),
          tcp_fin_pending(false),
          peer_window(kTcpWindowBytes),
          fin_retry_at(0),
          fin_attempts(0),
          close_requested(false),
          close_deadline(0),
          listener(nullptr),
          peer(nullptr),
          pending(nullptr),
          pending_next(nullptr),
          head(0),
          tail(0),
          size(0) {
        std::memset(buffer, 0, sizeof(buffer));
        std::memset(tx_buffer, 0, sizeof(tx_buffer));
        std::memset(peer_mac, 0, sizeof(peer_mac));
        std::memset(local_mac, 0, sizeof(local_mac));
    }
};

inline int tcp_send_segment(SocketLite* sock, uint8_t flags, const uint8_t* payload,
                            uint32_t len, uint32_t seq, uint32_t ack);

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

struct NetFrameState {
    static constexpr size_t kCapacity = 32768;
    uint8_t buffer[kCapacity];
    size_t head;
    size_t tail;
    size_t size;
    uint32_t drops;

    NetFrameState() : head(0), tail(0), size(0), drops(0) {
        std::memset(buffer, 0, sizeof(buffer));
    }
};

inline uint16_t net_htons(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

inline uint32_t net_htonl(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

inline uint16_t net_checksum(const uint8_t* data, uint32_t len) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i + 1 < len; i += 2) {
        sum += (uint16_t)((data[i] << 8) | data[i + 1]);
    }
    if (len & 1u) {
        sum += (uint16_t)(data[len - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

inline uint32_t net_checksum_add(uint32_t sum, const uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i + 1 < len; i += 2) {
        sum += (uint16_t)((data[i] << 8) | data[i + 1]);
    }
    if (len & 1u) {
        sum += (uint16_t)(data[len - 1] << 8);
    }
    return sum;
}

inline uint16_t net_checksum_finish(uint32_t sum) {
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

inline void net_ip_to_bytes(uint32_t ip, uint8_t out[4]) {
    out[0] = (uint8_t)((ip >> 24) & 0xFFu);
    out[1] = (uint8_t)((ip >> 16) & 0xFFu);
    out[2] = (uint8_t)((ip >> 8) & 0xFFu);
    out[3] = (uint8_t)(ip & 0xFFu);
}

inline uint32_t net_ip_from_bytes(const uint8_t ip[4]) {
    return (uint32_t)(ip[0] << 24) |
           (uint32_t)(ip[1] << 16) |
           (uint32_t)(ip[2] << 8) |
           (uint32_t)ip[3];
}

inline uint32_t tcp_backoff(uint32_t base, uint8_t attempt) {
    if (attempt <= 1) {
        return base;
    }
    uint8_t shift = (uint8_t)(attempt - 1);
    if (shift > 5) {
        shift = 5;
    }
    return base << shift;
}

struct __attribute__((packed)) net_eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
};

struct __attribute__((packed)) net_ipv4_hdr {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint8_t src[4];
    uint8_t dst[4];
};

struct __attribute__((packed)) net_tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t data_off;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
};

struct __attribute__((packed)) net_arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
};

inline int net_send_frame(uint16_t conn, uint16_t flags, const void* payload,
                          uint32_t len, uint32_t seq = 0, uint32_t ack = 0);

class SocketManager {
public:
    static constexpr uint32_t kMaxSockets = 64;
    static constexpr uint32_t kArpEntries = 8;

    SocketManager()
        : next_ephemeral_(40000),
          next_conn_id_(1),
          tcp_next_isn_(1),
          net_online_(false),
          local_ip_(0),
          local_mac_valid_(false),
          local_ip_valid_(false) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            in_use_[i] = false;
        }
        std::memset(local_mac_, 0, sizeof(local_mac_));
        for (uint32_t i = 0; i < kArpEntries; ++i) {
            arp_cache_[i].ip = 0;
            arp_cache_[i].last_seen = 0;
            arp_cache_[i].valid = false;
            std::memset(arp_cache_[i].mac, 0, sizeof(arp_cache_[i].mac));
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

    SocketLite* find_tcp_socket(uint16_t port, uint16_t peer_port, uint32_t peer_ip) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.backend != SocketLite::Backend::TCP ||
                sock.state == SocketLite::State::CLOSED) {
                continue;
            }
            if (sock.port == port && sock.peer_port == peer_port &&
                sock.peer_ip == peer_ip) {
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
            ensure_net_identity();
        }
    }

    bool net_online() const {
        return net_online_;
    }

    void ensure_net_identity() {
        if (!net_online_) {
            return;
        }
        if (!local_mac_valid_) {
            local_mac_valid_ = (rse_net_get_mac(local_mac_) == 0);
        }
        if (!local_ip_valid_) {
            uint8_t ip_bytes[4] = {};
            if (rse_net_get_ip(ip_bytes) == 0) {
                local_ip_ = (uint32_t)(ip_bytes[0] << 24) |
                            (uint32_t)(ip_bytes[1] << 16) |
                            (uint32_t)(ip_bytes[2] << 8) |
                            (uint32_t)ip_bytes[3];
                local_ip_valid_ = true;
            }
        }
    }

    uint32_t local_ip() const {
        return local_ip_;
    }

    bool local_ip_valid() const {
        return local_ip_valid_;
    }

    bool get_local_mac(uint8_t out[6]) const {
        if (!out || !local_mac_valid_) {
            return false;
        }
        std::memcpy(out, local_mac_, sizeof(local_mac_));
        return true;
    }

    void set_local_ip(uint32_t ip) {
        local_ip_ = ip;
        local_ip_valid_ = ip != 0;
    }

    uint32_t next_isn() {
        uint32_t value = tcp_next_isn_;
        tcp_next_isn_ += 0x1000u;
        if (tcp_next_isn_ == 0) {
            tcp_next_isn_ = 1;
        }
        return value;
    }

    bool arp_lookup(uint32_t ip, uint8_t out_mac[6]) {
        if (!out_mac || ip == 0) {
            return false;
        }
        uint32_t now = g_socket_net_ticks;
        for (uint32_t i = 0; i < kArpEntries; ++i) {
            if (arp_cache_[i].valid &&
                (now - arp_cache_[i].last_seen) > kArpEntryTtl) {
                arp_cache_[i].valid = false;
            }
            if (arp_cache_[i].valid && arp_cache_[i].ip == ip) {
                std::memcpy(out_mac, arp_cache_[i].mac, 6);
                return true;
            }
        }
        return false;
    }

    void arp_update(uint32_t ip, const uint8_t mac[6]) {
        if (!mac || ip == 0) {
            return;
        }
        uint32_t slot = kArpEntries;
        for (uint32_t i = 0; i < kArpEntries; ++i) {
            if (arp_cache_[i].valid && arp_cache_[i].ip == ip) {
                slot = i;
                break;
            }
            if (!arp_cache_[i].valid && slot == kArpEntries) {
                slot = i;
            }
        }
        if (slot == kArpEntries) {
            slot = (g_socket_net_ticks % kArpEntries);
        }
        arp_cache_[slot].ip = ip;
        arp_cache_[slot].valid = true;
        arp_cache_[slot].last_seen = g_socket_net_ticks;
        std::memcpy(arp_cache_[slot].mac, mac, 6);
    }

    void service_netlite_tx(uint32_t now) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.backend != SocketLite::Backend::NET_LITE) {
                continue;
            }
            if (sock.close_requested && sock.conn_id != 0 &&
                !sock.peer_closed && !sock.reset_pending) {
                if (sock.fin_retry_at == 0 || now >= sock.fin_retry_at) {
                    if (sock.fin_attempts >= kNetLiteCloseMaxRetries) {
                        sock.reset_pending = true;
                    } else {
                        (void)net_send_frame(sock.conn_id, kTcpLiteFin, nullptr, 0);
                        sock.fin_attempts++;
                        sock.fin_retry_at = now + kNetLiteCloseRetryTicks;
                    }
                }
            }
            if (sock.state != SocketLite::State::CONNECTED || !sock.tx_pending) {
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

    void service_tcp_tx(uint32_t now) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.backend != SocketLite::Backend::TCP ||
                sock.state == SocketLite::State::CLOSED) {
                continue;
            }
            if (sock.tcp_fin_pending &&
                (sock.tcp_state == TcpState::FIN_WAIT ||
                 sock.tcp_state == TcpState::LAST_ACK)) {
                if (sock.fin_retry_at == 0 || now >= sock.fin_retry_at) {
                    if (sock.fin_attempts >= kTcpMaxRetries) {
                        sock.reset_pending = true;
                        sock.state = SocketLite::State::CLOSED;
                        sock.tcp_state = TcpState::CLOSED;
                        sock.tcp_fin_pending = false;
                        sock.fin_attempts = 0;
                        sock.fin_retry_at = 0;
                    } else {
                        int rc = tcp_send_segment(&sock, (uint8_t)(kTcpFlagFin | kTcpFlagAck),
                                                  nullptr, 0, sock.fin_seq, sock.rx_seq);
                        if (rc >= 0) {
                            sock.fin_attempts++;
                            sock.fin_retry_at = now + tcp_backoff(kTcpRetryTicks, sock.fin_attempts);
                        }
                    }
                }
            }
            if (sock.state != SocketLite::State::CONNECTED ||
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
            if (sock.tx_attempts >= kTcpDataMaxRetries) {
                sock.reset_pending = true;
                sock.state = SocketLite::State::CLOSED;
                sock.tx_pending = false;
                continue;
            }
            int rc = tcp_send_segment(&sock, (uint8_t)(kTcpFlagAck | kTcpFlagPsh),
                                      sock.tx_buffer, sock.tx_pending_len,
                                      sock.tx_pending_seq, sock.rx_seq);
            if (rc < 0) {
                continue;
            }
            sock.tx_attempts++;
            sock.tx_retry_at = now + tcp_backoff(kTcpDataRetryTicks, sock.tx_attempts);
        }
    }

    void service_tcp_connect(uint32_t now) {
        auto reset_connect = [this](SocketLite* sock) {
            if (!sock) {
                return;
            }
            sock->state = (sock->port != 0)
                ? SocketLite::State::BOUND
                : SocketLite::State::CREATED;
            sock->peer_port = 0;
            sock->peer_ip = 0;
            sock->connect_attempts = 0;
            sock->connect_retry = 0;
            sock->connect_deadline = 0;
            sock->peer_closed = false;
            sock->reset_pending = true;
            sock->tcp_state = TcpState::CLOSED;
            sock->tcp_syn_pending = false;
            sock->listener = nullptr;
        };
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (sock.backend != SocketLite::Backend::TCP ||
                sock.state != SocketLite::State::CONNECTING ||
                !sock.tcp_syn_pending) {
                continue;
            }
            if (sock.connect_deadline != 0 && now >= sock.connect_deadline) {
                if (sock.listener) {
                    release(&sock);
                } else {
                    reset_connect(&sock);
                }
                continue;
            }
            if (sock.connect_retry == 0 || now < sock.connect_retry) {
                continue;
            }
            if (sock.connect_attempts >= kTcpMaxRetries) {
                if (sock.listener) {
                    release(&sock);
                } else {
                    reset_connect(&sock);
                }
                continue;
            }
            uint8_t flags = 0;
            uint32_t seq = 0;
            uint32_t ack = 0;
            if (sock.tcp_state == TcpState::SYN_SENT) {
                flags = kTcpFlagSyn;
                seq = sock.syn_seq;
            } else if (sock.tcp_state == TcpState::SYN_RCVD) {
                flags = (uint8_t)(kTcpFlagSyn | kTcpFlagAck);
                seq = sock.syn_seq;
                ack = sock.rx_seq;
            } else {
                reset_connect(&sock);
                continue;
            }
            int rc = tcp_send_segment(&sock, flags, nullptr, 0, seq, ack);
            if (rc < 0) {
                continue;
            }
            sock.connect_attempts++;
            sock.connect_retry = now + tcp_backoff(kTcpRetryTicks, sock.connect_attempts);
            if (sock.connect_deadline == 0) {
                sock.connect_deadline = now + kTcpConnectTimeout;
            }
        }
    }

    void sweep_closed(uint32_t now) {
        for (uint32_t i = 0; i < kMaxSockets; ++i) {
            if (!in_use_[i]) {
                continue;
            }
            SocketLite& sock = sockets_[i];
            if (!sock.close_requested) {
                continue;
            }
            bool expired = (sock.close_deadline != 0 && now >= sock.close_deadline);
            if (sock.backend == SocketLite::Backend::NET_LITE) {
                if (sock.state == SocketLite::State::CLOSED ||
                    sock.peer_closed || sock.reset_pending || expired) {
                    release(&sock);
                }
                continue;
            }
            if (sock.backend == SocketLite::Backend::TCP) {
                if (sock.tcp_state == TcpState::CLOSED ||
                    sock.state == SocketLite::State::CLOSED) {
                    release(&sock);
                    continue;
                }
                if (expired) {
                    sock.reset_pending = true;
                    sock.state = SocketLite::State::CLOSED;
                    sock.tcp_state = TcpState::CLOSED;
                    release(&sock);
                }
                continue;
            }
        }
    }

private:
    struct ArpEntry {
        uint32_t ip;
        uint8_t mac[6];
        uint32_t last_seen;
        bool valid;
    };

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
    uint32_t tcp_next_isn_;
    bool net_online_;
    uint32_t local_ip_;
    uint8_t local_mac_[6];
    bool local_mac_valid_;
    bool local_ip_valid_;
    ArpEntry arp_cache_[kArpEntries];
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
    pending->listener = listener;
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
    head->listener = nullptr;
    if (listener->pending_count > 0) {
        listener->pending_count--;
    }
    return head;
}

inline SocketLite* socket_pop_pending_ready(SocketLite* listener) {
    if (!listener) {
        return nullptr;
    }
    SocketLite* prev = nullptr;
    SocketLite* node = listener->pending;
    while (node) {
        if (node->state == SocketLite::State::CONNECTED) {
            if (prev) {
                prev->pending_next = node->pending_next;
            } else {
                listener->pending = node->pending_next;
            }
            node->pending_next = nullptr;
            node->listener = nullptr;
            if (listener->pending_count > 0) {
                listener->pending_count--;
            }
            return node;
        }
        prev = node;
        node = node->pending_next;
    }
    return nullptr;
}

inline NetWireState& net_wire_state() {
    static NetWireState state;
    return state;
}

inline NetWireState& net_out_state() {
    static NetWireState state;
    return state;
}

inline NetFrameState& net_frame_state() {
    static NetFrameState state;
    return state;
}

inline bool net_frame_push(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }
    NetFrameState& frame = net_frame_state();
    size_t free_space = NetFrameState::kCapacity - frame.size;
    if (len > free_space) {
        frame.drops++;
        return false;
    }
    size_t remaining = len;
    const uint8_t* src = data;
    while (remaining > 0 && frame.size < NetFrameState::kCapacity) {
        frame.buffer[frame.tail] = *src++;
        frame.tail = (frame.tail + 1) % NetFrameState::kCapacity;
        frame.size++;
        remaining--;
    }
    return remaining == 0;
}

inline bool net_frame_peek(uint8_t* out, size_t len) {
    if (!out || len == 0) {
        return false;
    }
    NetFrameState& frame = net_frame_state();
    if (frame.size < len) {
        return false;
    }
    size_t idx = frame.head;
    for (size_t i = 0; i < len; ++i) {
        out[i] = frame.buffer[idx];
        idx = (idx + 1) % NetFrameState::kCapacity;
    }
    return true;
}

inline bool net_frame_pop(uint8_t* out, size_t len) {
    if (!out || len == 0) {
        return false;
    }
    NetFrameState& frame = net_frame_state();
    if (frame.size < len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        out[i] = frame.buffer[frame.head];
        frame.head = (frame.head + 1) % NetFrameState::kCapacity;
    }
    frame.size -= len;
    return true;
}

inline void net_frame_consume(size_t len) {
    if (len == 0) {
        return;
    }
    NetFrameState& frame = net_frame_state();
    size_t consume = len > frame.size ? frame.size : len;
    frame.head = (frame.head + consume) % NetFrameState::kCapacity;
    frame.size -= consume;
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
    if (len > 0 && !payload) {
        return -EINVAL;
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
            if (sock->listener) {
                mgr.release(sock);
                return;
            }
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
            if (sock->listener) {
                mgr.release(sock);
                return;
            }
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

inline bool tcp_fill_local_identity(SocketLite* sock) {
    if (!sock) {
        return false;
    }
    SocketManager& mgr = socket_manager();
    mgr.ensure_net_identity();
    if (!mgr.local_ip_valid()) {
        return false;
    }
    if (!mgr.get_local_mac(sock->local_mac)) {
        return false;
    }
    if (sock->local_ip == 0) {
        sock->local_ip = mgr.local_ip();
    }
    return sock->local_ip != 0;
}

inline bool tcp_is_local_ip(uint32_t ip) {
    SocketManager& mgr = socket_manager();
    uint32_t local = mgr.local_ip();
    if (ip == 0 || ip == RSE_ADDR_LOOPBACK) {
        return true;
    }
    return local != 0 && ip == local;
}

inline uint16_t tcp_local_window(const SocketLite* sock) {
    if (!sock) {
        return kTcpWindowBytes;
    }
    size_t avail = SocketLite::kBufferSize - sock->size;
    if (avail > kTcpWindowBytes) {
        avail = kTcpWindowBytes;
    }
    return (uint16_t)avail;
}

inline uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                             const net_tcp_hdr* tcp,
                             const uint8_t* payload,
                             uint32_t len) {
    uint8_t pseudo[12];
    net_ip_to_bytes(src_ip, pseudo);
    net_ip_to_bytes(dst_ip, pseudo + 4);
    pseudo[8] = 0;
    pseudo[9] = kIpProtoTcp;
    pseudo[10] = (uint8_t)((len + sizeof(net_tcp_hdr)) >> 8);
    pseudo[11] = (uint8_t)((len + sizeof(net_tcp_hdr)) & 0xFFu);
    uint32_t sum = 0;
    sum = net_checksum_add(sum, pseudo, sizeof(pseudo));
    sum = net_checksum_add(sum, reinterpret_cast<const uint8_t*>(tcp), sizeof(net_tcp_hdr));
    if (payload && len > 0) {
        sum = net_checksum_add(sum, payload, len);
    }
    return net_checksum_finish(sum);
}

inline int tcp_send_arp_request(uint32_t target_ip) {
    SocketManager& mgr = socket_manager();
    uint8_t local_mac[6] = {};
    if (!mgr.get_local_mac(local_mac)) {
        return -EIO;
    }
    uint32_t local_ip = mgr.local_ip();
    if (local_ip == 0 || target_ip == 0) {
        return -EINVAL;
    }
    uint8_t frame[64];
    net_eth_hdr eth = {};
    net_arp_pkt arp = {};
    for (uint32_t i = 0; i < 6; ++i) {
        eth.dst[i] = 0xFF;
        eth.src[i] = local_mac[i];
        arp.sha[i] = local_mac[i];
        arp.tha[i] = 0;
    }
    eth.ethertype = net_htons(kEthertypeArp);
    arp.htype = net_htons(0x0001);
    arp.ptype = net_htons(kEthertypeIPv4);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = net_htons(0x0001);
    net_ip_to_bytes(local_ip, arp.spa);
    net_ip_to_bytes(target_ip, arp.tpa);
    uint32_t offset = 0;
    std::memcpy(frame + offset, &eth, sizeof(eth));
    offset += sizeof(eth);
    std::memcpy(frame + offset, &arp, sizeof(arp));
    offset += sizeof(arp);
    if (offset < 60) {
        std::memset(frame + offset, 0, 60 - offset);
        offset = 60;
    }
    int rc = rse_net_write(frame, offset);
    return rc < 0 ? rc : 0;
}

inline int tcp_send_arp_reply(uint32_t target_ip, const uint8_t target_mac[6]) {
    SocketManager& mgr = socket_manager();
    uint8_t local_mac[6] = {};
    if (!mgr.get_local_mac(local_mac)) {
        return -EIO;
    }
    uint32_t local_ip = mgr.local_ip();
    if (local_ip == 0 || !target_mac) {
        return -EINVAL;
    }
    uint8_t frame[64];
    net_eth_hdr eth = {};
    net_arp_pkt arp = {};
    for (uint32_t i = 0; i < 6; ++i) {
        eth.dst[i] = target_mac[i];
        eth.src[i] = local_mac[i];
        arp.sha[i] = local_mac[i];
        arp.tha[i] = target_mac[i];
    }
    eth.ethertype = net_htons(kEthertypeArp);
    arp.htype = net_htons(0x0001);
    arp.ptype = net_htons(kEthertypeIPv4);
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = net_htons(0x0002);
    net_ip_to_bytes(local_ip, arp.spa);
    net_ip_to_bytes(target_ip, arp.tpa);
    uint32_t offset = 0;
    std::memcpy(frame + offset, &eth, sizeof(eth));
    offset += sizeof(eth);
    std::memcpy(frame + offset, &arp, sizeof(arp));
    offset += sizeof(arp);
    if (offset < 60) {
        std::memset(frame + offset, 0, 60 - offset);
        offset = 60;
    }
    int rc = rse_net_write(frame, offset);
    return rc < 0 ? rc : 0;
}

inline bool tcp_resolve_peer_mac(SocketLite* sock) {
    if (!sock) {
        return false;
    }
    if (!tcp_fill_local_identity(sock)) {
        return false;
    }
    if (tcp_is_local_ip(sock->peer_ip) || sock->peer_ip == sock->local_ip) {
        std::memcpy(sock->peer_mac, sock->local_mac, sizeof(sock->peer_mac));
        return true;
    }
    SocketManager& mgr = socket_manager();
    if (mgr.arp_lookup(sock->peer_ip, sock->peer_mac)) {
        return true;
    }
    (void)tcp_send_arp_request(sock->peer_ip);
    return false;
}

inline int tcp_send_segment(SocketLite* sock, uint8_t flags, const uint8_t* payload,
                            uint32_t len, uint32_t seq, uint32_t ack) {
    if (!sock || sock->peer_ip == 0 || sock->port == 0 || sock->peer_port == 0) {
        return -EINVAL;
    }
    if (!socket_manager().net_online()) {
        return -EIO;
    }
    if (!tcp_resolve_peer_mac(sock)) {
        return -EAGAIN;
    }
    uint8_t frame[1600];
    uint32_t offset = 0;
    net_eth_hdr eth = {};
    net_ipv4_hdr ip = {};
    net_tcp_hdr tcp = {};

    std::memcpy(eth.dst, sock->peer_mac, sizeof(eth.dst));
    std::memcpy(eth.src, sock->local_mac, sizeof(eth.src));
    eth.ethertype = net_htons(kEthertypeIPv4);

    ip.ver_ihl = 0x45;
    ip.tos = 0;
    uint16_t ip_len = (uint16_t)(sizeof(net_ipv4_hdr) + sizeof(net_tcp_hdr) + len);
    ip.total_len = net_htons(ip_len);
    ip.id = net_htons(0x1234);
    ip.frag_off = net_htons(0x4000);
    ip.ttl = 64;
    ip.proto = kIpProtoTcp;
    ip.checksum = 0;
    net_ip_to_bytes(sock->local_ip, ip.src);
    net_ip_to_bytes(sock->peer_ip, ip.dst);
    ip.checksum = net_htons(net_checksum(reinterpret_cast<const uint8_t*>(&ip), sizeof(ip)));

    tcp.src_port = net_htons(sock->port);
    tcp.dst_port = net_htons(sock->peer_port);
    tcp.seq = net_htonl(seq);
    tcp.ack = net_htonl(ack);
    tcp.data_off = (uint8_t)(5u << 4);
    tcp.flags = flags;
    tcp.window = net_htons(tcp_local_window(sock));
    tcp.checksum = 0;
    tcp.urgent = 0;
    tcp.checksum = net_htons(tcp_checksum(sock->local_ip, sock->peer_ip, &tcp,
                                          payload, len));

    std::memcpy(frame + offset, &eth, sizeof(eth));
    offset += sizeof(eth);
    std::memcpy(frame + offset, &ip, sizeof(ip));
    offset += sizeof(ip);
    std::memcpy(frame + offset, &tcp, sizeof(tcp));
    offset += sizeof(tcp);
    if (payload && len > 0) {
        std::memcpy(frame + offset, payload, len);
        offset += len;
    }
    if (offset < 60) {
        std::memset(frame + offset, 0, 60 - offset);
        offset = 60;
    }
    int rc = rse_net_write(frame, offset);
    return rc < 0 ? rc : (int)len;
}

inline void tcp_handle_arp(const net_eth_hdr* eth, const net_arp_pkt* arp) {
    if (!eth || !arp) {
        return;
    }
    uint16_t oper = net_htons(arp->oper);
    uint32_t sender_ip = net_ip_from_bytes(arp->spa);
    if (sender_ip != 0) {
        socket_manager().arp_update(sender_ip, arp->sha);
    }
    if (oper != 0x0001) {
        return;
    }
    uint32_t target_ip = net_ip_from_bytes(arp->tpa);
    if (!tcp_is_local_ip(target_ip)) {
        return;
    }
    (void)tcp_send_arp_reply(sender_ip, arp->sha);
}

inline void tcp_handle_ipv4(const net_eth_hdr* eth, const net_ipv4_hdr* ip,
                            const uint8_t* payload, uint32_t len) {
    if (!eth || !ip || !payload) {
        return;
    }
    if ((ip->ver_ihl >> 4) != 4) {
        return;
    }
    uint32_t ihl = (uint32_t)(ip->ver_ihl & 0x0F) * 4;
    if (ihl < sizeof(net_ipv4_hdr) || len < ihl) {
        return;
    }
    if (net_checksum(reinterpret_cast<const uint8_t*>(ip), ihl) != 0) {
        return;
    }
    if (ip->proto != kIpProtoTcp) {
        return;
    }
    uint32_t src_ip = net_ip_from_bytes(ip->src);
    uint32_t dst_ip = net_ip_from_bytes(ip->dst);
    if (!tcp_is_local_ip(dst_ip)) {
        return;
    }
    if (len < ihl + sizeof(net_tcp_hdr)) {
        return;
    }
    const net_tcp_hdr* tcp = reinterpret_cast<const net_tcp_hdr*>(
        reinterpret_cast<const uint8_t*>(payload) + ihl);
    uint32_t tcp_header_len = (uint32_t)((tcp->data_off >> 4) & 0x0F) * 4;
    if (tcp_header_len < sizeof(net_tcp_hdr)) {
        return;
    }
    uint32_t payload_offset = ihl + tcp_header_len;
    if (payload_offset > len) {
        return;
    }
    uint32_t payload_len = len - payload_offset;
    const uint8_t* tcp_payload = reinterpret_cast<const uint8_t*>(payload) + payload_offset;
    net_tcp_hdr tcp_copy = *tcp;
    tcp_copy.checksum = 0;
    uint16_t calc = tcp_checksum(src_ip, dst_ip, &tcp_copy, tcp_payload, payload_len);
    uint16_t recv = net_htons(tcp->checksum);
    if (calc != recv) {
        return;
    }
    socket_manager().arp_update(src_ip, eth->src);

    uint16_t dst_port = net_htons(tcp->dst_port);
    uint16_t src_port = net_htons(tcp->src_port);
    uint32_t seq = net_htonl(tcp->seq);
    uint32_t ack = net_htonl(tcp->ack);
    uint8_t flags = tcp->flags;
    uint16_t window = net_htons(tcp->window);

    SocketManager& mgr = socket_manager();
    SocketLite* sock = mgr.find_tcp_socket(dst_port, src_port, src_ip);

    if ((flags & kTcpFlagSyn) && !(flags & kTcpFlagAck)) {
        SocketLite* listener = mgr.find_listener(dst_port, SocketLite::Backend::TCP);
        if (!listener) {
            SocketLite temp;
            temp.port = dst_port;
            temp.peer_port = src_port;
            temp.peer_ip = src_ip;
            tcp_fill_local_identity(&temp);
            std::memcpy(temp.peer_mac, eth->src, sizeof(temp.peer_mac));
            (void)tcp_send_segment(&temp, (uint8_t)(kTcpFlagRst | kTcpFlagAck),
                                   nullptr, 0, 0, seq + 1);
            return;
        }
        if (listener->backlog == 0) {
            listener->backlog = 1;
        }
        if (listener->pending_count >= listener->backlog) {
            SocketLite temp;
            temp.port = dst_port;
            temp.peer_port = src_port;
            temp.peer_ip = src_ip;
            tcp_fill_local_identity(&temp);
            std::memcpy(temp.peer_mac, eth->src, sizeof(temp.peer_mac));
            (void)tcp_send_segment(&temp, (uint8_t)(kTcpFlagRst | kTcpFlagAck),
                                   nullptr, 0, 0, seq + 1);
            return;
        }
        SocketLite* server_sock = mgr.allocate(SocketLite::Backend::TCP);
        if (!server_sock) {
            return;
        }
        server_sock->state = SocketLite::State::CONNECTING;
        server_sock->tcp_state = TcpState::SYN_RCVD;
        server_sock->port = dst_port;
        server_sock->peer_port = src_port;
        server_sock->peer_ip = src_ip;
        server_sock->rx_seq = seq + 1;
        server_sock->syn_seq = mgr.next_isn();
        server_sock->tx_seq = server_sock->syn_seq + 1;
        server_sock->tcp_syn_pending = true;
        server_sock->peer_window = window;
        server_sock->connect_attempts = 1;
        server_sock->connect_retry = g_socket_net_ticks + kTcpRetryTicks;
        server_sock->connect_deadline = g_socket_net_ticks + kTcpConnectTimeout;
        tcp_fill_local_identity(server_sock);
        std::memcpy(server_sock->peer_mac, eth->src, sizeof(server_sock->peer_mac));
        (void)tcp_send_segment(server_sock, (uint8_t)(kTcpFlagSyn | kTcpFlagAck),
                               nullptr, 0, server_sock->syn_seq, server_sock->rx_seq);
        socket_append_pending(listener, server_sock);
        return;
    }

    if (!sock) {
        if (flags & kTcpFlagRst) {
            return;
        }
        SocketLite temp;
        temp.port = dst_port;
        temp.peer_port = src_port;
        temp.peer_ip = src_ip;
        tcp_fill_local_identity(&temp);
        std::memcpy(temp.peer_mac, eth->src, sizeof(temp.peer_mac));
        uint32_t ack_num = seq + payload_len;
        if (flags & kTcpFlagSyn) {
            ack_num += 1;
        }
        if (flags & kTcpFlagFin) {
            ack_num += 1;
        }
        (void)tcp_send_segment(&temp, (uint8_t)(kTcpFlagRst | kTcpFlagAck),
                               nullptr, 0, 0, ack_num);
        return;
    }

    if (flags & kTcpFlagRst) {
        if (sock->listener) {
            mgr.release(sock);
            return;
        }
        sock->reset_pending = true;
        sock->peer_closed = false;
        sock->state = SocketLite::State::CLOSED;
        sock->tcp_state = TcpState::CLOSED;
        sock->tx_pending = false;
        sock->tcp_fin_pending = false;
        return;
    }

    if ((flags & kTcpFlagSyn) && (flags & kTcpFlagAck)) {
        if (sock->tcp_state == TcpState::SYN_SENT &&
            ack == sock->syn_seq + 1) {
            sock->rx_seq = seq + 1;
            sock->state = SocketLite::State::CONNECTED;
            sock->tcp_state = TcpState::ESTABLISHED;
            sock->connect_attempts = 0;
            sock->connect_retry = 0;
            sock->connect_deadline = 0;
            sock->tcp_syn_pending = false;
            sock->peer_window = window;
            std::memcpy(sock->peer_mac, eth->src, sizeof(sock->peer_mac));
            (void)tcp_send_segment(sock, kTcpFlagAck, nullptr, 0,
                                   sock->tx_seq, sock->rx_seq);
        }
        return;
    }

    if (flags & kTcpFlagAck) {
        sock->peer_window = window;
        if (sock->tcp_state == TcpState::SYN_RCVD &&
            ack == sock->syn_seq + 1) {
            sock->state = SocketLite::State::CONNECTED;
            sock->tcp_state = TcpState::ESTABLISHED;
            sock->connect_attempts = 0;
            sock->connect_retry = 0;
            sock->connect_deadline = 0;
            sock->tcp_syn_pending = false;
        }
        if (ack <= sock->tx_seq) {
            if (sock->tx_pending &&
                ack >= sock->tx_pending_seq + sock->tx_pending_len) {
                sock->tx_pending = false;
                sock->tx_pending_seq = 0;
                sock->tx_pending_len = 0;
                sock->tx_attempts = 0;
                sock->tx_retry_at = 0;
            }
            if (sock->tcp_fin_pending && ack == sock->fin_seq + 1) {
                sock->tcp_fin_pending = false;
                sock->fin_attempts = 0;
                sock->fin_retry_at = 0;
                if (sock->tcp_state == TcpState::LAST_ACK) {
                    sock->tcp_state = TcpState::CLOSED;
                    sock->state = SocketLite::State::CLOSED;
                } else if (sock->tcp_state == TcpState::FIN_WAIT &&
                           sock->peer_closed) {
                    sock->tcp_state = TcpState::CLOSED;
                    sock->state = SocketLite::State::CLOSED;
                }
            }
        }
    }

    if (payload_len > 0) {
        if (sock->state != SocketLite::State::CONNECTED) {
            return;
        }
        if (seq != sock->rx_seq) {
            (void)tcp_send_segment(sock, kTcpFlagAck, nullptr, 0,
                                   sock->tx_seq, sock->rx_seq);
            return;
        }
        size_t space = SocketLite::kBufferSize - sock->size;
        size_t to_write = payload_len < space ? payload_len : space;
        for (size_t i = 0; i < to_write; ++i) {
            sock->buffer[sock->tail] = tcp_payload[i];
            sock->tail = (sock->tail + 1) % SocketLite::kBufferSize;
        }
        sock->size += to_write;
        sock->rx_seq += (uint32_t)to_write;
        (void)tcp_send_segment(sock, kTcpFlagAck, nullptr, 0,
                               sock->tx_seq, sock->rx_seq);
    }

    if (flags & kTcpFlagFin) {
        sock->peer_closed = true;
        sock->rx_seq += 1;
        (void)tcp_send_segment(sock, kTcpFlagAck, nullptr, 0,
                               sock->tx_seq, sock->rx_seq);
        if (sock->tcp_state == TcpState::FIN_WAIT && !sock->tcp_fin_pending) {
            sock->state = SocketLite::State::CLOSED;
            sock->tcp_state = TcpState::CLOSED;
        } else if (sock->tcp_state == TcpState::FIN_WAIT) {
            sock->tcp_state = TcpState::LAST_ACK;
        } else {
            sock->tcp_state = TcpState::CLOSE_WAIT;
        }
    }
}

inline void tcp_handle_frame(const uint8_t* frame, uint32_t len) {
    if (!frame || len < sizeof(net_eth_hdr)) {
        return;
    }
    const net_eth_hdr* eth = reinterpret_cast<const net_eth_hdr*>(frame);
    uint16_t ethertype = net_htons(eth->ethertype);
    const uint8_t* payload = frame + sizeof(net_eth_hdr);
    uint32_t payload_len = len - sizeof(net_eth_hdr);
    if (ethertype == kEthertypeArp) {
        if (payload_len < sizeof(net_arp_pkt)) {
            return;
        }
        const net_arp_pkt* arp = reinterpret_cast<const net_arp_pkt*>(payload);
        tcp_handle_arp(eth, arp);
        return;
    }
    if (ethertype == kEthertypeIPv4) {
        if (payload_len < sizeof(net_ipv4_hdr)) {
            return;
        }
        const net_ipv4_hdr* ip = reinterpret_cast<const net_ipv4_hdr*>(payload);
        uint16_t total_len = net_htons(ip->total_len);
        if (total_len < sizeof(net_ipv4_hdr) ||
            total_len > payload_len) {
            return;
        }
        tcp_handle_ipv4(eth, ip, payload, total_len);
    }
}

inline void netlite_poll_net() {
    SocketManager& mgr = socket_manager();
    if (!mgr.net_online()) {
        return;
    }
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

inline void tcp_poll_net();

inline void socket_poll_net() {
    SocketManager& mgr = socket_manager();
    if (!mgr.net_online()) {
        return;
    }
    g_socket_net_ticks++;
#if RSE_NET_RAW
    tcp_poll_net();
#else
    netlite_poll_net();
#endif
    mgr.sweep_closed(g_socket_net_ticks);
}

inline void tcp_poll_net() {
    SocketManager& mgr = socket_manager();
    if (!mgr.net_online()) {
        return;
    }
    uint8_t scratch[2048];
    while (true) {
        int got = rse_net_read(scratch, sizeof(scratch));
        if (got <= 0) {
            break;
        }
        if (!net_frame_push(scratch, static_cast<size_t>(got))) {
            break;
        }
    }

    while (net_frame_state().size >= sizeof(net_eth_hdr)) {
        net_eth_hdr eth{};
        if (!net_frame_peek(reinterpret_cast<uint8_t*>(&eth), sizeof(eth))) {
            break;
        }
        uint16_t ethertype = net_htons(eth.ethertype);
        uint32_t frame_len = 0;
        if (ethertype == kEthertypeArp) {
            frame_len = sizeof(net_eth_hdr) + sizeof(net_arp_pkt);
        } else if (ethertype == kEthertypeIPv4) {
            uint8_t ip_buf[sizeof(net_eth_hdr) + sizeof(net_ipv4_hdr)];
            if (!net_frame_peek(ip_buf, sizeof(ip_buf))) {
                break;
            }
            const net_ipv4_hdr* ip = reinterpret_cast<const net_ipv4_hdr*>(
                ip_buf + sizeof(net_eth_hdr));
            uint16_t total_len = net_htons(ip->total_len);
            if (total_len < sizeof(net_ipv4_hdr)) {
                net_frame_consume(1);
                continue;
            }
            frame_len = sizeof(net_eth_hdr) + total_len;
        } else {
            net_frame_consume(1);
            continue;
        }
        if (frame_len < 60) {
            frame_len = 60;
        }
        if (net_frame_state().size < frame_len) {
            break;
        }
        uint8_t frame[1600];
        if (frame_len > sizeof(frame)) {
            net_frame_consume(frame_len);
            continue;
        }
        if (!net_frame_pop(frame, frame_len)) {
            break;
        }
        tcp_handle_frame(frame, frame_len);
    }

    mgr.service_tcp_connect(g_socket_net_ticks);
    mgr.service_tcp_tx(g_socket_net_ticks);
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
    if (!sock) {
        return 0;
    }
    if (sock->backend == SocketLite::Backend::NET_LITE) {
        if (sock->state != SocketLite::State::CONNECTED) {
            socket_manager().release(sock);
            return 0;
        }
        if (sock->conn_id != 0 && !sock->peer_closed) {
            (void)net_send_frame(sock->conn_id, kTcpLiteFin, nullptr, 0);
            sock->fin_attempts = 1;
            sock->fin_retry_at = g_socket_net_ticks + kNetLiteCloseRetryTicks;
        }
        sock->close_requested = true;
        sock->close_deadline = g_socket_net_ticks + kNetLiteCloseTimeout;
        sock->state = SocketLite::State::CLOSED;
        return 0;
    }
    if (sock->backend == SocketLite::Backend::TCP) {
        if (sock->state != SocketLite::State::CONNECTED) {
            socket_manager().release(sock);
            return 0;
        }
        sock->close_requested = true;
        sock->close_deadline = g_socket_net_ticks + kTcpCloseTimeout;
        sock->fin_seq = sock->tx_seq;
        sock->tx_seq += 1;
        sock->tcp_fin_pending = true;
        sock->fin_attempts = 1;
        sock->fin_retry_at = g_socket_net_ticks + kTcpRetryTicks;
        sock->tcp_state = (sock->tcp_state == TcpState::CLOSE_WAIT)
            ? TcpState::LAST_ACK
            : TcpState::FIN_WAIT;
        (void)tcp_send_segment(sock, (uint8_t)(kTcpFlagFin | kTcpFlagAck),
                               nullptr, 0, sock->fin_seq, sock->rx_seq);
        return 0;
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
    if (sock->backend == SocketLite::Backend::NET_LITE ||
        sock->backend == SocketLite::Backend::TCP) {
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
    if (sock->backend == SocketLite::Backend::TCP) {
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
        uint16_t window = sock->peer_window;
        if (window == 0) {
            socket_poll_net();
            return -EAGAIN;
        }
        uint32_t to_write = count > kTcpMss ? kTcpMss : (uint32_t)count;
        if (window < to_write) {
            to_write = window;
        }
        if (to_write == 0) {
            return -EAGAIN;
        }
        std::memcpy(sock->tx_buffer, buf, to_write);
        sock->tx_pending = true;
        sock->tx_pending_seq = sock->tx_seq;
        sock->tx_pending_len = to_write;
        sock->tx_seq += to_write;
        sock->tx_attempts = 1;
        sock->tx_retry_at = g_socket_net_ticks + kTcpDataRetryTicks;
        int rc = tcp_send_segment(sock, (uint8_t)(kTcpFlagAck | kTcpFlagPsh),
                                  sock->tx_buffer, to_write,
                                  sock->tx_pending_seq, sock->rx_seq);
        if (rc < 0) {
            sock->tx_attempts = 0;
            sock->tx_retry_at = g_socket_net_ticks + kTcpDataRetryTicks;
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
