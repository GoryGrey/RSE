# RSE PROJECT STATUS
Last Updated: January 02, 2026 (ring3 user-mode signal delivery via handler trampoline; ring3 scheduler skips stopped/blocked/waiting processes; sys_kill supports SIGSTOP/SIGCONT stop-resume (ring3-aware) and queues pending user signals with wake for blocked/sleeping targets; sys_wait returns EINTR when a pending signal exists; kernel-mode custom signal handlers for cooperative tasks; sys_wait blocks current process when waiting for child (wakes on zombie); net frame send rejects null payload; NET_LITE close retries for FIN; raw TCP drops payloads when recv buffer is full (no partial accept); UEFI GDT compatibility segments for loader selectors; PIT timer preemption; ring3 sleep/yield + timeslice; W^X mprotect checks; user-pointer bounds tightened; ring3 maps user-only pages; NET_LITE connect retries/backlog/FIN handling + EOF/EPIPE + RST refused + seq/ack retransmit + outbound queue + overflow drops + conn collision checks; raw TCP socket backend (ARP/IPv4/TCP) for AF_INET when RSE_NET_RAW=1 with retransmit/backoff, FIN retry, window backpressure, ARP cache aging, close sweep, and pending handshake cleanup; sys_kill now enforces uid (root or same uid) even for sig=0 probe; sys_fork inherits uid/gid + signal handlers; sys_ps filters by uid for non-root callers; exec resets signal handlers to default (ignores preserved); virtio-net TX backpressure returns EAGAIN; virtio-net RX length/id guards; net reads return EAGAIN on empty; net queue drops + RX/TX error counters surfaced; mergeable RX disabled pending reassembly; virtio-net UEFI fallback only on hard errors; BlockFS checksum recovery + slot zeroing on create/remove + scrub on checksum mismatch; BlockFS sanitize now scrubs invalid entries and zeroes stale slots (mode bits masked to 0777); BlockFS journal recovery test hook (non-kernel); uid/gid ownership enforced for MemFS/BlockFS with stat reporting; CLOEXEC dup hygiene; persist dir open returns EISDIR; exec requires exec-bit; ancestor exec enforced for memfs/persist path traversal; signal ignore/default; stat parent-exec gating; workload yields; directory listing sort; ISO workflow scripts; logged test heartbeat; quick boot test runner)

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
- MemFS + BlockFS with per-process file descriptors, `/persist` directories, MemFS nested paths, deterministic list ordering, permission checks on open/list/mkdir/unlink (including parent exec/write), uid/gid ownership enforcement, `.`/`..` path segments rejected, and `-EISDIR` on directory open.
- MemFS/persist path traversal now enforces execute permission on all ancestor directories for open/list/mkdir/unlink.
- BlockFS persistence with checksum + journal + corruption detection (flat table, directory paths), with mount-time sanitize for duplicates/invalid entries, mode scrubbing, checksum-mismatch slot scrubbing, and stale slot zeroing; journal write checks; remove resets entry metadata and zeroes slot data; new files zero their slots before use.
- TCP-lite framing over `/dev/net0` for loopback handshake/data tests with NET_LITE connect retries/timeouts, FIN-on-close (EOF for reads, EPIPE on write) with FIN retry, RST refused handling, seq/ack retransmit (stop-and-wait), outbound queue with flush, queued pending accepts, overflow-safe wire buffering, stricter FIN state handling, and conn-id collision checks.
- Raw Ethernet/IP/TCP backend for AF_INET sockets when `RSE_NET_RAW=1` (SYN/SYN-ACK/ACK, data, FIN), with retransmit/backoff, FIN retry, window backpressure, recv buffer backpressure (no partial accept), ARP cache aging, close sweep, pending handshake cleanup, and local MAC/IP discovery.
- virtio-net driver backpressure returns EAGAIN, RX descriptor length/id validation, and guarded UEFI fallback (empty reads return EAGAIN; net queue drops + RX/TX counters tracked; mergeable RX disabled until reassembly support exists).
- In-kernel socket syscalls (`socket/bind/listen/accept/connect`) with loopback buffers and NET_LITE framing over the net device path.
- Syscall dispatcher with user-range validation anchored to per-process code/stack bounds (including nanosleep/pipe/time pointers) and per-torus dispatch.
- User-mode isolation: sys_fork inherits uid/gid + signal handlers; sys_ps hides other users for non-root callers; exec resets signal handlers to default (ignores preserved); sys_kill enforces uid on sig=0 probes; SIGSTOP/SIGCONT stop/resume support (ring3-aware); user-mode signal handlers now delivered via trampoline; blocked/sleeping targets are woken on queued user signals; kernel-mode custom handlers for cooperative tasks; sys_wait blocks on child exit with wakeup on zombie and returns EINTR when a pending signal exists.
- Ring3 page table mirroring only maps pages flagged as user in the process page tables.
- ELF loader enforces the user virtual address window; out-of-range segments are rejected.
- User mmap rejects overlaps; mmap/mprotect/munmap validate zero/unaligned sizes; PROT_EXEC blocked for anonymous mmap and limited to code pages; mprotect denies exec on writable pages and write access over code ranges; stack guard pages widened; mmap uses guard pages by default; read/write reject oversized counts; mprotect refuses unmapped ranges; exec requires exec-bit; stat enforces parent exec; VFS reserves `/dev` and rejects invalid `/persist` subpaths; net loopback backpressure returns `-EAGAIN`.
- Ring3 exec smoke (UEFI): exec path works; isolation still evolving.
- Ring3 init now loads a real freestanding ELF at `/bin/init` (no synthetic payloads).
- Ring3 init supports script-driven workloads via `/persist/boot.rc` or `/boot.rc`.
- Ring3 init yields during long loops to avoid starving other ring3 slots.
- Ring3 scheduling rotates across multiple ring3 slots on syscall boundaries with per-process time slices (3 slots per torus), skips stopped/blocked/waiting processes, supports sleep/nanosleep via per-syscall ticks, and has PIT-based preemption enabled.
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
- Latest full system run log: `/tmp/rse_full_test.log` (Jan 02, 2026; TIMEOUT_BOOT=90).
- Latest projection exchange logs: `/tmp/rse_net_exchange` (Jan 02, 2026).
- Prior repo log snapshot: `build/boot/full_test.log` (Jan 02, 2026).
- Syscall + OS tests: `sys_wait_test`, `sys_kill_test`, `sys_ps_test`, `sys_stat_test`,
  `sys_memfs_dir_test`, `sys_user_isolation_test`, `sys_vfs_persist_test`,
  `sys_socket_test`, `sys_socket_net_test`, `sys_socket_tcp_test` (RSE_NET_RAW=1),
  `sys_pipe_test`, `sys_dup_test`, `sys_mmap_test`, `exec_vfs_test`.
- Devices: `blockfs_test`, `net_device_test`.
- Recent local runs: full system test (logged), `blockfs_test`, `sys_ps_test`, `sys_kill_test`, `sys_user_isolation_test`, `sys_socket_test`, `sys_socket_net_test`, `sys_socket_tcp_test`, `net_device_test` (Jan 02, 2026).

Note: If the IDE freezes during the 3-VM exchange step, run it from a terminal and redirect logs outside the repo:
`NET_LOG_DIR=/tmp/rse_net_exchange TIMEOUT_EXCHANGE=90 TIMEOUT_BOOT=120 ./scripts/run_full_system_test.sh`

### Known Limitations

- User-mode isolation/permissions are still evolving; ring3 time slicing is syscall-boundary only and limited to a small slot count.
- Network stack is minimal (NET_LITE framing + raw TCP basics; no congestion control, fragmentation, or socket options).
- BlockFS uses fixed slots; ownership uses 8-bit uid/gid (single group, no ACLs).
- Workload init is script-driven but still one-shot per boot (no interactive console input yet).

---

## Production-Grade Priorities

1) User-mode isolation
- Stronger address-space boundaries and permission model.
- Expanded syscall surface with strict user pointer validation.
- Signal handling and robust process lifecycle.

2) Network stack
- Stable RX/TX under load.
- TCP hardening (retransmit, flow control, teardown edge cases).
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

Latest snapshot (January 02, 2026):

| Metric | RSE (UEFI/QEMU, TSC-calibrated) | Linux baseline (host) | Notes |
|--------|----------------------------------|------------------------|-------|
| Compute | 17 ns/op (7,025,530 ns total) | 580.5 ns/op | Smoke run (UEFI) |
| Memory copy | 5 ns/byte (385,899,666 ns total) | 171.3 ns/byte | Smoke run (UEFI) |
| File I/O | RAMFS 21,825 ns/op | 30,625 ns/op | Smoke run (UEFI) |
| HTTP loopback | skipped (smoke) | blocked (permission denied) | Not run in smoke mode |

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
