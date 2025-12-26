#pragma once

#include <cstddef>
#include <cstdint>

#include "../src/cpp_kernel/braided/Projection.h"

enum RsepxMsgType : uint16_t {
    RSEPX_MSG_PROJECTION = 1,
    RSEPX_MSG_ACK = 2,
};

struct __attribute__((packed)) RsepxHeader {
    uint8_t magic[4];
    uint16_t version;
    uint16_t msg_type;
    uint32_t torus_id;
    uint32_t phase;
    uint64_t timestamp;
    uint64_t seq;
    uint32_t payload_len;
    uint64_t payload_hash;
    uint16_t frag_index;
    uint16_t frag_count;
    uint32_t frag_len;
};

struct __attribute__((packed)) RsepxAck {
    uint64_t seq;
    uint64_t payload_hash;
};

struct RsepxReceived {
    enum Kind {
        None = 0,
        Projection,
        Ack
    } kind;
    RsepxHeader header;
    braided::Projection projection;
    RsepxAck ack;
    uint8_t src_mac[6];
};

void rsepx_init(uint32_t torus_id);
int rsepx_send_projection(const braided::Projection& proj, uint32_t phase, uint64_t seq, const uint8_t* dst_mac);
int rsepx_send_ack(uint64_t seq, uint64_t payload_hash, const uint8_t* dst_mac);
int rsepx_poll(RsepxReceived* out);
