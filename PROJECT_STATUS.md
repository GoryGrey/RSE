# RSE PROJECT STATUS
Last Updated: December 28, 2025 (FS permissions + BlockFS sanitize hardening)

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
- MemFS + BlockFS with per-process file descriptors, `/persist` directories, MemFS nested paths, and permission checks on open/list/mkdir/unlink.
- BlockFS persistence with checksum + journal + corruption detection (flat table, directory paths), with mount-time sanitize for duplicates/invalid entries.
- TCP-lite framing over `/dev/net0` for loopback handshake/data tests.
- In-kernel socket syscalls (`socket/bind/listen/accept/connect`) with loopback device-backed buffers.
- Syscall dispatcher with user-range validation and per-torus dispatch.
- User mmap rejects overlaps; mmap/mprotect/munmap validate zero/unaligned sizes; PROT_EXEC blocked for anonymous mmap and limited to code pages; stack guard pages widened; mmap uses guard pages by default; read/write reject oversized counts; mprotect refuses unmapped ranges; VFS reserves `/dev` and rejects invalid `/persist` subpaths; net loopback backpressure returns `-EAGAIN`.
- Ring3 exec smoke (UEFI): exec path works; isolation still evolving.
- Projection exchange across 3 VMs via IVSHMEM shared memory.
- BraidShell demo with telemetry sourced from real logs.

### Verified Test Coverage

- `./scripts/run_full_system_test.sh` (build + native tests + UEFI boot + IVSHMEM exchange).
- `./scripts/run_linux_baseline.sh` (host baseline).
- Syscall + OS tests: `sys_wait_test`, `sys_ps_test`, `sys_stat_test`, `sys_memfs_dir_test`,
  `sys_user_isolation_test`, `sys_vfs_persist_test`, `sys_socket_test`, `sys_pipe_test`,
  `sys_dup_test`, `sys_mmap_test`, `exec_vfs_test`.
- Devices: `blockfs_test`, `net_device_test`.

Note: If the IDE freezes during the 3-VM exchange step, run it from a terminal and redirect logs outside the repo:
`NET_LOG_DIR=/tmp/rse_net_exchange TIMEOUT_EXCHANGE=90 TIMEOUT_BOOT=120 ./scripts/run_full_system_test.sh`

### Known Limitations

- No full user-mode isolation/permissions yet; ring3 exec is a smoke path.
- Network stack is minimal (ARP/UDP parsing + loopback); socket syscalls are loopback-only.
- BlockFS uses fixed slots; permissions are basic (no user ownership model yet).
- Workload init is one-shot per boot.

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

Latest snapshot (Dec 28, 2025):

| Metric | RSE (UEFI/QEMU, TSC-calibrated) | Linux baseline (host) | Notes |
|--------|----------------------------------|------------------------|-------|
| Compute | 55 ns/op (22,301,494 ns total) | 580.5 ns/op | Different workloads/environments |
| Memory copy | 9 ns/byte (631,566,526 ns total) | 171.3 ns/byte | Different sizes/passes |
| File I/O | RAMFS 11,078 ns/op | 30,625 ns/op | Different storage paths |
| HTTP loopback | 434 ns/req | blocked (permission denied) | Network sandbox blocks host HTTP |

Notes:
- RSE ns values are derived from TSC calibration via UEFI `Stall`; treat as approximate.
- Source logs: `build/boot/boot.log` (RSE) and `benchmarks/linux_baseline.json` (Linux).
- Capture fresh runs with `./scripts/run_full_system_test.sh` and `./scripts/run_linux_baseline.sh`.

---

## How to Run

- Full system verification: `./scripts/run_full_system_test.sh`
- Full system verification (logged): `LOG_PATH=/tmp/rse_full_test.log ./scripts/run_full_system_test_logged.sh`
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
