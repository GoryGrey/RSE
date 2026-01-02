# RSE (Resilient Spatial Execution)

Last Updated: January 01, 2026 (persist dir open returns EISDIR; /persist directory support + permissions; status refresh)

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
- In-kernel workloads: compute, memory, RAMFS I/O, UEFI FS/block I/O, HTTP loopback.
- /dev/net0 UDP loopback (minimal stack; no full IP/TCP yet).
- Cooperative userspace tasks + ring3 exec smoke (UEFI).
- Fast-path I/O device (/dev/fast0) using a native ring buffer.
- Framebuffer dashboard with keyboard/mouse input.
- Braided runtime (single-node projections + constraint application).
- Projection exchange across 3 VMs via IVSHMEM shared memory.
- Block-backed persistence via BlockFS mounted at /persist with directories and basic permission checks.

## Known Limitations

- User-mode isolation and permissions are still evolving.
- Network RX/TX needs hardening; no TCP yet.
- BlockFS is fixed-slot; ownership model is still missing and permissions are basic.
- Workload init is one-shot per boot.

---

## Production-Grade Priorities

1) User-mode isolation
- Stronger address-space boundaries and permission model.
- Expanded syscall surface with strict pointer validation.
- Signals and robust process lifecycle.

2) Network stack
- Stable RX/TX under load.
- Minimal TCP support.
- Driver hardening (virtio-net).

3) Filesystem
- BlockFS permissions/ownership model.
- Stronger journaling and recovery.

---

## Benchmarks (Real Logs Only)

Metrics are captured per run. Use logs to inspect real values:

- Kernel + UEFI benchmarks: `./scripts/run_full_system_test.sh` (see `build/boot/boot.log` or `/tmp` overrides).
- Host baseline: `./scripts/run_linux_baseline.sh`.
- Default smoke mode: `RSE_BENCH_SMOKE=1` (set `RSE_BENCH_SMOKE=0` to run full UEFI/virtio/net micro-benchmarks).

Latest snapshot (Dec 28, 2025):

| Metric | RSE (UEFI/QEMU, TSC-calibrated) | Linux baseline (host) | Notes |
|--------|----------------------------------|------------------------|-------|
| Compute | 55 ns/op (22,301,494 ns total) | 580.5 ns/op | Different workloads/environments |
| Memory copy | 9 ns/byte (631,566,526 ns total) | 171.3 ns/byte | Different sizes/passes |
| File I/O | RAMFS 11,078 ns/op | 30,625 ns/op | Different storage paths |
| HTTP loopback | 434 ns/req | blocked (permission denied) | Network sandbox blocks host HTTP |

Notes:
- RSE ns values are derived from TSC calibration via UEFI Stall; treat as approximate.
- Source logs: `build/boot/boot.log` (RSE) and `benchmarks/linux_baseline.json` (Linux).

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
