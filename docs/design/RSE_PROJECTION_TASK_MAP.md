# RSE Projection Exchange Task Map

**Scope**: Concrete work items to implement the projection exchange spec.
**Spec Source**: `docs/design/RSE_PROJECTION_EXCHANGE_SPEC.md`

## Phase A: Projection Serialization (Kernel-Safe)

1) **Projection wire encode/decode**
   - Implement fixed-size serialization/deserialization for `braided::Projection`.
   - Ensure little-endian, stable layout across toolchains.
   - File targets: `src/cpp_kernel/braided/Projection.*`

2) **Integrity hashing**
   - Implement FNV-1a over the serialized payload.
   - Verify `state_hash` and `payload_hash` on receive.

## Phase B: Transport (Reliable)

3) **Reliable transport baseline**
   - Decide baseline: TCP or reliable-UDP wrapper.
   - Define sockets/connection semantics for torus A/B/C.
   - File targets: `src/cpp_kernel/os/net/*`, `src/cpp_kernel/os/ProjectionTransport.*`
   - Sandbox validation: shared-memory transport (ivshmem) to prove multi-VM exchange.

4) **ACK + retransmission**
   - Sequence tracking per sender torus.
   - Timeout + retransmit policy.
   - Apply at-most-once semantics.

## Phase C: Braid Exchange State Machine

5) **Phase logic**
   - A_PROJECTS -> B_PROJECTS -> C_PROJECTS cycle.
   - Enforce only one projecting torus per phase.
   - File targets: `src/cpp_kernel/os/BraidExchange.*`

6) **Constraint application**
   - Call into existing braided constraint hooks (applyConstraint).
   - Record violations on hash mismatch or invalid projection.

## Phase D: OS Integration

7) **Config + endpoints**
   - Define torus IDs, peer addresses, braid interval.
   - Wire into `boot/kernel_os.cpp` or `boot/init.c`.

8) **Failure detection**
   - Track missed projections, mark peer degraded.
   - Emit events for reconstruction path (future phase).

## Phase E: Bench + Validation

9) **Latency measurement**
   - Record exchange time (send -> ACK).
   - Target <1ms LAN latency.

10) **Correctness validation**
   - Validate fixed projection size.
   - Confirm integrity hashes match.
   - Ensure O(1) memory behavior remains.

## Phase F: Distributed Mode (Roadmap-Linked)

11) **Projection-based reconstruction**
   - Rebuild failed torus from peer projections.
   - Resume braid cycle without global controller.

12) **Cross-machine process spawning**
   - Spawn and migrate processes across tori.
   - Keep constraints consistent with braid rules.
