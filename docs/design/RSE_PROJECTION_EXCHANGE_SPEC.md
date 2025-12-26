# RSE Projection Exchange Specification

**Version**: 0.1  
**Status**: Draft (design-aligned)  
**Scope**: Defines the on-wire projection exchange used by braided tori across machines.

## 1) Design Sources (Canonical)

This spec is derived directly from:
- `docs/design/BRAIDED_TORUS_DESIGN.md` (projection structure, O(1) size, integrity)
- `docs/design/RSE_BRAIDED_TORUS_ANALYSIS.md` (cyclic exchange semantics, low-bandwidth/high-value)
- `docs/OS_ROADMAP.md` (reliable delivery, <1ms latency target, failure handling)
- `docs/RSE_Whitepaper.md` and `docs/The General Theory of Recursive Symbolic.md` (topological constraints and cyclic stabilization)

## 2) Goals and Constraints

**Goals**
- Exchange torus projections over a network without a global controller.
- Preserve the braided, cyclic constraint model (A -> B/C, then B -> A/C, then C -> A/B).
- Keep bandwidth fixed (O(1) per exchange) and verifiable (integrity hash).

**Constraints**
- **O(1) size**: payload size must be fixed (no growth with event/process count).
- **Serializable**: explicit, architecture-stable encoding.
- **Verifiable**: include hash for consistency checks.
- **Reliable delivery**: no silent drops; projections must be applied or retried.
- **Latency target**: < 1ms exchange (as stated in OS roadmap).

## 3) Projection Payload (Authoritative)

Projection layout is fixed and constant-size. The structure mirrors the design in
`BRAIDED_TORUS_DESIGN.md`.

**Projection fields**
- `torus_id` (u32)
- `timestamp` (u64) - logical time when projection was created
- `total_events_processed` (u64)
- `current_time` (u64)
- `active_processes` (u32)
- `pending_events` (u32)
- `edge_count` (u32)
- `boundary_states[1024]` (u32) - x=0 face of 32x32 boundary
- `constraint_vector[16]` (i32)
- `state_hash` (u64) - FNV-1a over critical fields

**Size**
- Total: ~4.2 KB (constant)

## 4) Wire Format

All fields are **little-endian**. A projection is carried in a single message.

### 4.1 Header (fixed)

```
struct RsepxHeader {
  char     magic[4];        // "RSEP"
  uint16_t version;         // 0x0001
  uint16_t msg_type;        // 1=PROJECTION, 2=ACK, 3=HEARTBEAT
  uint32_t torus_id;        // sender torus
  uint32_t phase;           // 0=A_PROJECTS, 1=B_PROJECTS, 2=C_PROJECTS
  uint64_t timestamp;       // logical time
  uint64_t seq;             // monotonic sequence per torus
  uint32_t payload_len;     // bytes following header
  uint64_t payload_hash;    // FNV-1a over payload
  uint16_t frag_index;      // fragment index (0..frag_count-1)
  uint16_t frag_count;      // number of fragments
  uint32_t frag_len;        // bytes in this fragment
};
```

### 4.2 Payload (PROJECTION)

Payload is a raw serialized `Projection` as defined in section 3.

**Fragmentation**: If the projection exceeds a single L2 frame payload, it is
split into fixed-size fragments. Each fragment carries the same header with
`frag_index`, `frag_count`, and `frag_len`. Reassembly completes when all
fragments are present for the same `seq` and `payload_hash`.

### 4.3 ACK

```
struct RsepxAck {
  uint64_t seq;             // acknowledged sequence
  uint64_t payload_hash;    // hash of payload being acked
};
```

### 4.4 HEARTBEAT

Optional keepalive; used for failure detection when braid interval is long.

## 5) Exchange Semantics (Braided Cycle)

**Cycle**: A -> B/C, then B -> A/C, then C -> A/B.

At each braid interval `k`:
1. The projecting torus sends its projection to the other two tori.
2. Receivers verify:
   - `payload_hash` matches
   - `projection.state_hash` matches `projection.computeHash()`
   - `seq` is monotonic
3. Receivers apply the projection as constraints (no full state transfer).
4. Receivers ACK the projection.

**Reliability**
- Sender retransmits if no ACK within `ack_timeout` (configurable).
- A projection is applied at most once per `seq`.

**Consistency Failures**
- If `state_hash` fails, record a consistency violation and reject.

## 6) Failure Detection and Recovery

From `RSE_BRAIDED_TORUS_ANALYSIS.md`:
- Missing projections imply torus failure or partition.
- Recovery uses projections from the other two tori.

**Failure Detection**
- If `N` consecutive expected projections from a torus are missing (default 3), mark it as degraded.
- Receivers continue operating with remaining two projections.

**Reconstruction (future)**
- Reconstruct failed torus state using last known projections from the other two.
- Resume braid cycle when the torus rejoins.

## 7) Transport Requirements

Transport **MUST** provide:
- Reliable, ordered delivery of projection messages.
- Latency target: < 1ms exchange in LAN conditions.

**Baseline transport**: TCP (simplicity + reliability).
**Optimization path**: Custom reliable protocol over UDP with ACK/seq, if TCP latency is too high.

## 8) Metrics and Success Criteria

- Projection size: ~4.2 KB (fixed)
- Exchange interval: configurable, default 1000 ticks
- Exchange latency: < 1 ms (target)
- Consistency violations: 0 in steady state
- No global coordinator, only cyclic constraints

## 9) Implementation Map (First Cut)

Kernel/OS:
- `src/cpp_kernel/braided/Projection.*`: serialization + hash
- `src/cpp_kernel/os/net/*`: minimal IP/UDP or TCP stack
- `src/cpp_kernel/os/ProjectionTransport.*`: send/recv/ack
- `src/cpp_kernel/os/BraidExchange.*`: state machine for phases, seqs, timeouts
- `boot/init.c` or `boot/kernel_os.cpp`: config for braid interval + endpoints

Benchmarks:
- Measure exchange latency and projection integrity error rate.
- Verify that projection exchange does not change O(1) memory behavior.

## 10) Notes on Novelty Alignment

This spec preserves the braided, cyclic constraint model and fixed-size projections described in your theory papers. It avoids hierarchical controllers, uses low-bandwidth/high-value state summaries, and keeps the topology-driven invariants as the coordinating mechanism.
