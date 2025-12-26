#include "net_projection.h"

extern "C" int rse_net_write(const void* buf, uint32_t len);
extern "C" int rse_net_read(void* buf, uint32_t len);
extern "C" int rse_net_get_mac(uint8_t mac_out[6]);
extern "C" void serial_write(const char* msg);
extern "C" void serial_write_u64(uint64_t value);

namespace {

static constexpr uint16_t kRsepxVersion = 0x0001;
static constexpr uint16_t kRsepxEthertype = 0x88B5;
static constexpr uint32_t kRsepxFragPayload = 1024;
static constexpr uint16_t kRsepxMaxFrags =
    (uint16_t)((sizeof(braided::Projection) + kRsepxFragPayload - 1) / kRsepxFragPayload);
static constexpr uint32_t kRsepxMaxPayload = (uint32_t)sizeof(braided::Projection);
static constexpr uint32_t kRsepxFrameMax = 2048;

struct __attribute__((packed)) EthHeader {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
};

struct RsepxAssembly {
    uint64_t seq;
    uint64_t payload_hash;
    uint32_t payload_len;
    uint16_t frag_count;
    uint32_t received_mask;
    uint8_t buffer[kRsepxMaxPayload];
};

static uint32_t g_local_torus = 0;
static uint8_t g_local_mac[6];
static int g_local_mac_ok = 0;
static RsepxAssembly g_assemblies[3];
static uint32_t g_drop_logs = 0;

static uint16_t rsepx_htons(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint64_t rsepx_fnv1a(const uint8_t* data, uint32_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void rsepx_fill_magic(uint8_t magic[4]) {
    magic[0] = 'R';
    magic[1] = 'S';
    magic[2] = 'E';
    magic[3] = 'P';
}

static int rsepx_magic_ok(const uint8_t magic[4]) {
    return magic[0] == 'R' && magic[1] == 'S' && magic[2] == 'E' && magic[3] == 'P';
}

static void rsepx_set_mac(uint8_t out[6], const uint8_t* in) {
    for (uint32_t i = 0; i < 6; ++i) {
        out[i] = in[i];
    }
}

static void rsepx_memcpy(void* dst, const void* src, uint32_t len) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (uint32_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static int rsepx_mac_equal(const uint8_t* a, const uint8_t* b) {
    for (uint32_t i = 0; i < 6; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static void rsepx_assembly_reset(RsepxAssembly& asmbl, uint64_t seq, uint64_t hash,
                                 uint16_t frag_count, uint32_t payload_len) {
    asmbl.seq = seq;
    asmbl.payload_hash = hash;
    asmbl.payload_len = payload_len;
    asmbl.frag_count = frag_count;
    asmbl.received_mask = 0;
}

} // namespace

void rsepx_init(uint32_t torus_id) {
    g_local_torus = torus_id;
    g_local_mac_ok = (rse_net_get_mac(g_local_mac) == 0);
    for (size_t i = 0; i < 3; ++i) {
        rsepx_assembly_reset(g_assemblies[i], 0, 0, 0, 0);
    }
}

int rsepx_send_projection(const braided::Projection& proj, uint32_t phase, uint64_t seq,
                          const uint8_t* dst_mac) {
    if (!g_local_mac_ok && rse_net_get_mac(g_local_mac) == 0) {
        g_local_mac_ok = 1;
    }
    if (!g_local_mac_ok) {
        return -1;
    }

    uint8_t payload[kRsepxMaxPayload];
    const uint32_t payload_len = (uint32_t)proj.serialize(payload, sizeof(payload));
    if (payload_len == 0 || payload_len > kRsepxMaxPayload) {
        return -1;
    }
    const uint64_t payload_hash = rsepx_fnv1a(payload, payload_len);
    const uint16_t frag_count = (uint16_t)((payload_len + kRsepxFragPayload - 1) / kRsepxFragPayload);
    if (frag_count == 0 || frag_count > kRsepxMaxFrags) {
        return -1;
    }

    uint8_t dst[6];
    if (dst_mac) {
        rsepx_set_mac(dst, dst_mac);
    } else {
        for (uint32_t i = 0; i < 6; ++i) {
            dst[i] = 0xFF;
        }
    }

    uint8_t frame[kRsepxFrameMax];
    for (uint16_t frag_index = 0; frag_index < frag_count; ++frag_index) {
        const uint32_t offset = (uint32_t)frag_index * kRsepxFragPayload;
        uint32_t frag_len = payload_len - offset;
        if (frag_len > kRsepxFragPayload) {
            frag_len = kRsepxFragPayload;
        }

        EthHeader eth{};
        rsepx_set_mac(eth.dst, dst);
        rsepx_set_mac(eth.src, g_local_mac);
        eth.ethertype = rsepx_htons(kRsepxEthertype);

        RsepxHeader hdr{};
        rsepx_fill_magic(hdr.magic);
        hdr.version = kRsepxVersion;
        hdr.msg_type = RSEPX_MSG_PROJECTION;
        hdr.torus_id = proj.torus_id;
        hdr.phase = phase;
        hdr.timestamp = proj.timestamp;
        hdr.seq = seq;
        hdr.payload_len = payload_len;
        hdr.payload_hash = payload_hash;
        hdr.frag_index = frag_index;
        hdr.frag_count = frag_count;
        hdr.frag_len = frag_len;

        uint32_t frame_len = 0;
        rsepx_memcpy(frame + frame_len, &eth, sizeof(eth));
        frame_len += sizeof(eth);
        rsepx_memcpy(frame + frame_len, &hdr, sizeof(hdr));
        frame_len += sizeof(hdr);
        rsepx_memcpy(frame + frame_len, payload + offset, frag_len);
        frame_len += frag_len;

        if (rse_net_write(frame, frame_len) < 0) {
            return -1;
        }
    }

    return 0;
}

int rsepx_send_ack(uint64_t seq, uint64_t payload_hash, const uint8_t* dst_mac) {
    if (!g_local_mac_ok && rse_net_get_mac(g_local_mac) == 0) {
        g_local_mac_ok = 1;
    }
    if (!g_local_mac_ok) {
        return -1;
    }
    if (!dst_mac) {
        return -1;
    }

    EthHeader eth{};
    rsepx_set_mac(eth.dst, dst_mac);
    rsepx_set_mac(eth.src, g_local_mac);
    eth.ethertype = rsepx_htons(kRsepxEthertype);

    RsepxHeader hdr{};
    rsepx_fill_magic(hdr.magic);
    hdr.version = kRsepxVersion;
    hdr.msg_type = RSEPX_MSG_ACK;
    hdr.torus_id = g_local_torus;
    hdr.phase = 0;
    hdr.timestamp = 0;
    hdr.seq = seq;
    hdr.payload_len = (uint32_t)sizeof(RsepxAck);
    hdr.payload_hash = payload_hash;
    hdr.frag_index = 0;
    hdr.frag_count = 1;
    hdr.frag_len = (uint32_t)sizeof(RsepxAck);

    RsepxAck ack{};
    ack.seq = seq;
    ack.payload_hash = payload_hash;

    uint8_t frame[kRsepxFrameMax];
    uint32_t frame_len = 0;
    rsepx_memcpy(frame + frame_len, &eth, sizeof(eth));
    frame_len += sizeof(eth);
    rsepx_memcpy(frame + frame_len, &hdr, sizeof(hdr));
    frame_len += sizeof(hdr);
    rsepx_memcpy(frame + frame_len, &ack, sizeof(ack));
    frame_len += sizeof(ack);

    return rse_net_write(frame, frame_len) < 0 ? -1 : 0;
}

int rsepx_poll(RsepxReceived* out) {
    if (!out) {
        return -1;
    }
    out->kind = RsepxReceived::None;
    int got = 0;

    uint8_t frame[kRsepxFrameMax];
    int len = rse_net_read(frame, sizeof(frame));
    if (len <= 0) {
        return 0;
    }
    if ((uint32_t)len < sizeof(EthHeader) + sizeof(RsepxHeader)) {
        return 0;
    }

    const EthHeader* eth = reinterpret_cast<const EthHeader*>(frame);
    if (rsepx_htons(eth->ethertype) != kRsepxEthertype) {
        if (g_drop_logs < 4) {
            serial_write("[RSE] net projection drop ethertype=");
            serial_write_u64(rsepx_htons(eth->ethertype));
            serial_write(" len=");
            serial_write_u64((uint64_t)len);
            serial_write("\n");
            g_drop_logs++;
        }
        return 0;
    }
    if (!g_local_mac_ok && rse_net_get_mac(g_local_mac) == 0) {
        g_local_mac_ok = 1;
    }
    if (g_local_mac_ok && rsepx_mac_equal(eth->src, g_local_mac)) {
        return 0;
    }

    const RsepxHeader* hdr = reinterpret_cast<const RsepxHeader*>(frame + sizeof(EthHeader));
    if (!rsepx_magic_ok(hdr->magic) || hdr->version != kRsepxVersion) {
        if (g_drop_logs < 4) {
            serial_write("[RSE] net projection drop header\n");
            g_drop_logs++;
        }
        return 0;
    }
    const uint8_t* payload = frame + sizeof(EthHeader) + sizeof(RsepxHeader);
    const uint32_t payload_len = (uint32_t)len - (uint32_t)(sizeof(EthHeader) + sizeof(RsepxHeader));

    if (hdr->msg_type == RSEPX_MSG_ACK) {
        if (payload_len < sizeof(RsepxAck)) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop ack size\n");
                g_drop_logs++;
            }
            return 0;
        }
        out->kind = RsepxReceived::Ack;
        out->header = *hdr;
        rsepx_memcpy(&out->ack, payload, sizeof(RsepxAck));
        rsepx_set_mac(out->src_mac, eth->src);
        got = 1;
    } else if (hdr->msg_type == RSEPX_MSG_PROJECTION) {
        if (hdr->payload_len > kRsepxMaxPayload) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop payload len\n");
                g_drop_logs++;
            }
            return 0;
        }
        if (hdr->frag_count == 0 || hdr->frag_count > kRsepxMaxFrags) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop frag count\n");
                g_drop_logs++;
            }
            return 0;
        }
        if (hdr->frag_index >= hdr->frag_count) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop frag index\n");
                g_drop_logs++;
            }
            return 0;
        }
        if (hdr->frag_len > kRsepxFragPayload) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop frag len\n");
                g_drop_logs++;
            }
            return 0;
        }
        if (payload_len < hdr->frag_len) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop short frame\n");
                g_drop_logs++;
            }
            return 0;
        }
        if (hdr->torus_id >= 3) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop torus id\n");
                g_drop_logs++;
            }
            return 0;
        }

        RsepxAssembly& asmbl = g_assemblies[hdr->torus_id];
        const uint64_t seq = hdr->seq;
        if (asmbl.seq != seq || asmbl.payload_hash != hdr->payload_hash ||
            asmbl.frag_count != hdr->frag_count || asmbl.payload_len != hdr->payload_len) {
            rsepx_assembly_reset(asmbl, seq, hdr->payload_hash, hdr->frag_count, hdr->payload_len);
        }

        const uint32_t offset = (uint32_t)hdr->frag_index * kRsepxFragPayload;
        if (offset + hdr->frag_len > asmbl.payload_len) {
            if (g_drop_logs < 4) {
                serial_write("[RSE] net projection drop offset\n");
                g_drop_logs++;
            }
            return 0;
        }
        rsepx_memcpy(asmbl.buffer + offset, payload, hdr->frag_len);
        asmbl.received_mask |= (uint32_t)(1u << hdr->frag_index);

        const uint32_t want_mask = (hdr->frag_count >= 32) ? 0xFFFFFFFFu : ((1u << hdr->frag_count) - 1u);
        if (asmbl.received_mask == want_mask) {
            const uint64_t hash = rsepx_fnv1a(asmbl.buffer, asmbl.payload_len);
            if (hash != asmbl.payload_hash) {
                if (g_drop_logs < 4) {
                    serial_write("[RSE] net projection drop hash\n");
                    g_drop_logs++;
                }
                rsepx_assembly_reset(asmbl, 0, 0, 0, 0);
                return 0;
            }
            out->kind = RsepxReceived::Projection;
            out->header = *hdr;
            out->projection = braided::Projection::deserialize(asmbl.buffer, asmbl.payload_len);
            rsepx_set_mac(out->src_mac, eth->src);
            rsepx_assembly_reset(asmbl, 0, 0, 0, 0);
            got = 1;
        }
    }

    return got;
}
