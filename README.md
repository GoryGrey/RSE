# RSE (Resilient Spatial Execution)

Last Updated: January 02, 2026 (ring3 user-mode signal delivery via handler trampoline; ring3 scheduler skips stopped/blocked/waiting processes; sys_kill SIGSTOP/SIGCONT support with pending user signal queueing; kernel-mode custom signal handlers for cooperative tasks; sys_wait blocks on child exit with wakeup on zombie; net frame send rejects null payload; UEFI GDT compatibility segments for loader selectors; sys_fork uid/gid + signal handler inheritance; sys_ps filters by uid for non-root; exec resets signal handlers (ignores preserved); memfs/persist ancestor exec enforcement; virtio-net TX backpressure + RX guardrails; empty net reads return EAGAIN; net queue + RX/TX error counters surfaced; BlockFS sanitize scrubs invalid entries and zeroes stale slots; BlockFS slot zeroing on create/remove; mergeable RX disabled pending reassembly; raw TCP socket backend for AF_INET when RSE_NET_RAW=1 with retransmit/backoff + window backpressure + close sweep + pending handshake cleanup; sys_kill uid enforcement; sys_socket_tcp_test)

Status: Research prototype. Bootable UEFI kernel with an interactive dashboard and in-kernel workloads; braided projection exchange works in multi-VM via shared memory.

Quick Links: [Project Status](PROJECT_STATUS.md) | [Documentation](#documentation)

---

## Overview

RSE explores a braided-torus execution model for coordinating computation without a global scheduler. Multiple tori exchange fixed-size projections cyclically (A -> B -> C -> A), keeping coordination overhead constant while preserving autonomy.

This repo includes both layers:
- Runtime: Betti-RDL single-torus + braided execution engine.
- OS: bootable UEFI kernel scaffold with syscalls, VFS, devices, and userspace runner.

---

## Architecture Summary

```
Torus A (32^3 lattice) ⟲
    ↓ projection (fixed size)
Torus B (32^3 lattice) ⟲
    ↓ projection (fixed size)
Torus C (32^3 lattice) ⟲
    ↓ projection (fixed size)
(cycle repeats A→B→C→A)
```

Key properties:
- Fixed-size projections independent of workload size.
- Cyclic constraint exchange instead of a global controller.
- O(1) coordination overhead by design.

---

## What Works Today

- Bootable UEFI kernel in QEMU (serial + framebuffer).
- UEFI GDT compatibility segments to honor loader selectors (fixes early #GP).
- In-kernel workloads: compute, memory, RAMFS I/O, UEFI FS/block I/O, HTTP loopback.
- /dev/net0 UDP loopback plus raw Ethernet/IP/TCP backend for AF_INET sockets when `RSE_NET_RAW=1` (retransmit/backoff, FIN retry, window backpressure, ARP cache aging).
- Cooperative userspace tasks + ring3 exec smoke (UEFI).
- Fast-path I/O device (/dev/fast0) using a native ring buffer.
- Framebuffer dashboard with keyboard/mouse input.
- Exec enforces executable permission bit on ELF targets.
- Syscalls support default/ignore signal dispositions (SIGKILL/SIGSTOP immutable).
- SIGSTOP/SIGCONT stop/resume support in sys_kill.
- Kernel-mode custom signal handlers for cooperative tasks.
- Ring3 user-mode signal delivery (basic trampoline).
- Ring3 scheduler skips stopped/blocked/waiting processes when rotating slots.
- sys_kill enforces uid (root or same uid).
- sys_fork inherits uid/gid + signal handlers; sys_ps filters by uid for non-root callers; exec resets signal handlers to default (ignores preserved).
- NET_LITE sockets return EOF on peer FIN, EPIPE on write after close, and ECONNREFUSED on refused connect.
- NET_LITE uses seq/ack + retransmit (stop-and-wait) for reliable delivery.
- TCP/NET_LITE close paths now defer release and sweep closed sockets; pending handshakes are released on timeout/reset.
- virtio-net driver returns EAGAIN on TX queue full/empty reads and validates RX descriptors; UDP queue drops and RX/TX error counters are tracked (mergeable RX disabled until reassembly support lands).
- MemFS/persist path traversal enforces execute permission on all ancestor directories.
- Braided runtime (single-node projections + constraint application).
- Projection exchange across 3 VMs via IVSHMEM shared memory.
- Block-backed persistence via BlockFS mounted at /persist with directories and owner/group/other permission enforcement.
- BlockFS zeroes slot data on create/remove to avoid stale payload leakage.
- BlockFS sanitize now scrubs invalid entries and zeroes stale slots during recovery.

## Known Limitations

- User-mode isolation and permissions are still evolving.
- Custom signal handling is basic (no sigaltstack/sigreturn yet).
- Network stack remains minimal (raw TCP is basic; no congestion control, fragmentation, or full socket options).
- BlockFS is fixed-slot; ownership uses 8-bit uid/gid (single group, no ACLs).
- Workload init is one-shot per boot.

---

## Production-Grade Priorities

1) User-mode isolation
- Stronger address-space boundaries and permission model.
- Expanded syscall surface with strict pointer validation.
- Signals and robust process lifecycle.

2) Network stack
- Stable RX/TX under load.
- TCP hardening (retransmit, flow control, teardown edge cases).
- Driver hardening (virtio-net).

3) Filesystem
- BlockFS permissions/ownership expansion (multi-group/ACL).
- Stronger journaling and recovery.

---

## Benchmarks (Real Logs Only)

Metrics are captured per run. Use logs to inspect real values:

- Kernel + UEFI benchmarks: `./scripts/run_full_system_test.sh` (see `build/boot/boot.log`; logged runs write to `build/boot/full_test.log` when using the logged wrapper).
- Host baseline: `./scripts/run_linux_baseline.sh`.
- Default smoke mode: `RSE_BENCH_SMOKE=1` (set `RSE_BENCH_SMOKE=0` to run full UEFI/virtio/net micro-benchmarks).

Latest snapshot (January 02, 2026):

| Metric | RSE (UEFI/QEMU, TSC-calibrated) | Linux baseline (host) | Notes |
|--------|----------------------------------|------------------------|-------|
| Compute | 17 ns/op (7,025,530 ns total) | 580.5 ns/op | Smoke run (UEFI) |
| Memory copy | 5 ns/byte (385,899,666 ns total) | 171.3 ns/byte | Smoke run (UEFI) |
| File I/O | RAMFS 21,825 ns/op | 30,625 ns/op | Smoke run (UEFI) |
| HTTP loopback | skipped (smoke) | blocked (permission denied) | Not run in smoke mode |

Notes:
- RSE ns values are derived from TSC calibration via UEFI Stall; treat as approximate.
- Source logs: `build/boot/full_test.log` (RSE; logged run) and `benchmarks/linux_baseline.json` (Linux).

---

## Build and Run

UEFI kernel (QEMU):
```bash
make -f boot/Makefile.uefi run-iso
```

Framebuffer dashboard:
```bash
make -f boot/Makefile.uefi run
```

Raw frame mode for /dev/net0:
```bash
make -f boot/Makefile.uefi RSE_NET_RAW=1 run-iso
```

Projection exchange (3 VMs, IVSHMEM):
```bash
scripts/run_projection_exchange.sh
```

Proof run (build + boot + external RX):
```bash
scripts/run_proof.sh
```

Braided runtime demo:
```bash
cd src/cpp_kernel/braided
mkdir -p build && cd build
cmake ..
make
./braided_demo
```

---

## Documentation

- `PROJECT_STATUS.md` - current implementation status and benchmark logs
- `docs/ARCHITECTURE.md` - system design
- `docs/OS_ROADMAP.md` - long-term plan
- `docs/design/` - design specs
- `docs/phase_reports/` and `docs/status/` - historical status docs

---

## License

MIT License. See `LICENSE`.
