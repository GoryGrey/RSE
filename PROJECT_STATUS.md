# RSE PROJECT STATUS
Last Updated: January 01, 2026 (PIT timer preemption; ring3 sleep/yield + timeslice; W^X mprotect checks; user-pointer bounds tightened; ring3 maps user-only pages; NET_LITE connect retries/backlog/FIN handling + overflow drops + conn collision checks; BlockFS checksum recovery; CLOEXEC dup hygiene; persist dir open returns EISDIR; exec requires exec-bit; workload yields; directory listing sort; ISO workflow scripts; logged test heartbeat; quick boot test runner)

---

## Vision

Build a next-generation operating system that makes old hardware run like new through fundamentally better architecture.

Principles:
- No global scheduler
- No global memory manager
- No single point of failure
- Emergent coordination through a braided-torus architecture

Goal: Make a 10-year-old laptop feel brand new by eliminating architectural bottlenecks.

---

## Current Status: Prototype

This status covers both the Betti-RDL runtime and the OS scaffold in this repo.

### What Works

- Bootable UEFI kernel (serial + framebuffer) with dashboard and input.
- In-kernel benchmarks (compute, memory, RAMFS, UEFI FS/block, fastio, HTTP loopback).
- MemFS + BlockFS with per-process file descriptors, `/persist` directories, MemFS nested paths, deterministic list ordering, permission checks on open/list/mkdir/unlink (including parent exec/write), `.`/`..` path segments rejected, and `-EISDIR` on directory open.
- BlockFS persistence with checksum + journal + corruption detection (flat table, directory paths), with mount-time sanitize for duplicates/invalid entries and journal write checks; remove fully resets entry metadata.
- TCP-lite framing over `/dev/net0` for loopback handshake/data tests with NET_LITE connect retries/timeouts, FIN-on-close, queued pending accepts, overflow-safe wire buffering, stricter FIN state handling, and conn-id collision checks.
- In-kernel socket syscalls (`socket/bind/listen/accept/connect`) with loopback buffers and NET_LITE framing over the net device path.
- Syscall dispatcher with user-range validation anchored to per-process code/stack bounds (including nanosleep/pipe/time pointers) and per-torus dispatch.
- Ring3 page table mirroring only maps pages flagged as user in the process page tables.
- ELF loader enforces the user virtual address window; out-of-range segments are rejected.
- User mmap rejects overlaps; mmap/mprotect/munmap validate zero/unaligned sizes; PROT_EXEC blocked for anonymous mmap and limited to code pages; mprotect denies exec on writable pages and write access over code ranges; stack guard pages widened; mmap uses guard pages by default; read/write reject oversized counts; mprotect refuses unmapped ranges; exec requires exec-bit; VFS reserves `/dev` and rejects invalid `/persist` subpaths; net loopback backpressure returns `-EAGAIN`.
- Ring3 exec smoke (UEFI): exec path works; isolation still evolving.
- Ring3 init now loads a real freestanding ELF at `/bin/init` (no synthetic payloads).
- Ring3 init supports script-driven workloads via `/persist/boot.rc` or `/boot.rc`.
- Ring3 init yields during long loops to avoid starving other ring3 slots.
- Ring3 scheduling rotates across multiple ring3 slots on syscall boundaries with per-process time slices (3 slots per torus), supports sleep/nanosleep via per-syscall ticks, and has PIT-based preemption enabled.
- UEFI kernel build is freestanding and provides minimal string/mem shims for kernel C++ code.
- Projection exchange across 3 VMs via IVSHMEM shared memory.
- BraidShell demo with telemetry sourced from real logs.
- ISO build/run scripts for bootable testing (`scripts/build_iso.sh`, `scripts/run_iso.sh`).

Design guardrail:
- Bootstrapping userland must stay true to the braided, decentralized model: any bootstrap init is minimal, non-monolithic, and must not hinder torus autonomy or system capabilities.

### Verified Test Coverage

- `./scripts/run_full_system_test.sh` (build + native tests + UEFI boot + IVSHMEM exchange).
- `./scripts/run_quick_system_test.sh` (UEFI boot + smoke benchmarks only).
- `./scripts/run_linux_baseline.sh` (host baseline).
- Latest full system run log: `build/boot/full_test.log` (Jan 01, 2026).
- Syscall + OS tests: `sys_wait_test`, `sys_ps_test`, `sys_stat_test`, `sys_memfs_dir_test`,
  `sys_user_isolation_test`, `sys_vfs_persist_test`, `sys_socket_test`, `sys_socket_net_test`,
  `sys_pipe_test`, `sys_dup_test`, `sys_mmap_test`, `exec_vfs_test`.
- Devices: `blockfs_test`, `net_device_test`.

Note: If the IDE freezes during the 3-VM exchange step, run it from a terminal and redirect logs outside the repo:
`NET_LOG_DIR=/tmp/rse_net_exchange TIMEOUT_EXCHANGE=90 TIMEOUT_BOOT=120 ./scripts/run_full_system_test.sh`

### Known Limitations

- User-mode isolation/permissions are still evolving; ring3 time slicing is syscall-boundary only and limited to a small slot count.
- Network stack is minimal (ARP/UDP parsing + loopback); NET_LITE framing is not full TCP.
- BlockFS uses fixed slots; permissions are basic (no user ownership model yet).
- Workload init is script-driven but still one-shot per boot (no interactive console input yet).

---

## Production-Grade Priorities

1) User-mode isolation
- Stronger address-space boundaries and permission model.
- Expanded syscall surface with strict user pointer validation.
- Signal handling and robust process lifecycle.

2) Network stack
- Stable RX/TX under load.
- Minimal TCP support.
- Driver hardening (virtio-net).

3) Filesystem
- BlockFS directories + permissions (MemFS directories now land).
- Stronger journaling and recovery.

---

## Architecture Summary

Braided-torus execution model with fixed-size projections exchanged cyclically:

```
Torus A (32^3 lattice) ⟲
    ↓ projection (fixed size)
Torus B (32^3 lattice) ⟲
    ↓ projection (fixed size)
Torus C (32^3 lattice) ⟲
    ↓ projection (fixed size)
(cycle repeats A→B→C→A)
```

Key property: no global controller; coordination is constant-size and cyclic.

---

## Performance Snapshot (Real Logs Only)

Latest snapshot (January 01, 2026):

| Metric | RSE (UEFI/QEMU, TSC-calibrated) | Linux baseline (host) | Notes |
|--------|----------------------------------|------------------------|-------|
| Compute | 16 ns/op (6,685,442 ns total) | 580.5 ns/op | ~3.4x faster vs Dec 28 snapshot |
| Memory copy | 6 ns/byte (415,030,802 ns total) | 171.3 ns/byte | ~1.5x faster vs Dec 28 snapshot |
| File I/O | RAMFS 9,102 ns/op | 30,625 ns/op | ~1.2x faster vs Dec 28 snapshot |
| HTTP loopback | 344 ns/req | blocked (permission denied) | ~1.3x faster vs Dec 28 snapshot |

Notes:
- RSE ns values are derived from TSC calibration via UEFI `Stall`; treat as approximate.
- Source logs: `build/boot/full_test.log` (RSE; logged run) and `benchmarks/linux_baseline.json` (Linux).
- Capture fresh runs with `./scripts/run_full_system_test_logged.sh` and `./scripts/run_linux_baseline.sh`.

---

## How to Run

- Full system verification: `./scripts/run_full_system_test.sh`
- Full system verification (logged): `LOG_PATH=/tmp/rse_full_test.log ./scripts/run_full_system_test_logged.sh`
- Quick UEFI verification: `./scripts/run_quick_system_test.sh`
- Logged runs can show liveness: `LIVE_TAIL=1 HEARTBEAT_SECS=15 ./scripts/run_full_system_test_logged.sh`
- Smoke benchmark default: `RSE_BENCH_SMOKE=1` (set `RSE_BENCH_SMOKE=0` for full UEFI/virtio/net micro-benchmarks).
- Projection exchange only: `./scripts/run_projection_exchange.sh`
- Host baseline: `./scripts/run_linux_baseline.sh`
- BraidShell telemetry: set `RSE_METRICS_PATH` to a real metrics log.

Build kernel/tests:
```
cmake -S src/cpp_kernel -B build/cpp_kernel
cmake --build build/cpp_kernel
```

---

## Maintenance Note

- After each commit, update `PROJECT_STATUS.md` (and any related status docs like `README.md` or `docs/OS_ROADMAP.md` if they reference the change).

---

## Documentation Map

- Status (historical): `docs/status/`
- Phase reports (historical): `docs/phase_reports/`
- Architecture: `docs/ARCHITECTURE.md`
- Design specs: `docs/design/`
- Roadmap: `docs/OS_ROADMAP.md`
- API: `docs/API_REFERENCE.md`

---

Status: Prototype
Next Milestone: User-mode isolation + expanded syscall surface
