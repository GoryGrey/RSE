# RSE Operating System: Roadmap

Last Updated: January 02, 2026 (ring3 scheduler skips stopped/blocked/waiting processes; pending user signals queued; UEFI GDT compatibility segments; full system test log refreshed)
Status: Active development (prototype)

---

## Summary

This roadmap tracks the path from the current prototype to a production-grade OS. It reflects what is already implemented and the next milestones needed to harden isolation, networking, and storage.

---

## Current Baseline (Now)

- Bootable UEFI kernel (serial + framebuffer) with dashboard.
- SIGSTOP/SIGCONT stop/resume support in sys_kill.
- Kernel-mode custom signal handlers for cooperative tasks.
- sys_wait blocks on child exit with wakeup on zombie.
- Ring3 user-mode signal delivery (basic trampoline).
- Ring3 scheduling skips stopped/blocked/waiting processes while rotating slots.
- UEFI GDT compatibility segments to honor loader selectors (fixes early #GP).
- Syscall dispatcher with user-pointer validation and per-torus dispatch.
- User isolation progress: sys_fork inherits uid/gid + signal handlers; sys_ps filters by uid for non-root callers.
- ELF loader enforces the user virtual address window (rejects out-of-range segments).
- MemFS + BlockFS with checksum journal; /persist mounted with directory paths; MemFS supports nested paths, deterministic list order, uid/gid ownership, and permission checks (including ancestor exec); BlockFS sanitizes invalid entries on mount, scrubs checksum-mismatch/stale slots, and zeroes slots on create/remove.
- Minimal network stack (ARP/UDP parsing + loopback) with NET_LITE socket syscalls and a raw Ethernet/IP/TCP backend for AF_INET sockets when `RSE_NET_RAW=1` (basic handshake/data/FIN with close retries, close sweep, pending handshake cleanup).
- UEFI kernel build is freestanding with minimal string/mem shims for kernel C++ code.
- Ring3 init loads a real freestanding user ELF at `/bin/init`, can run `/persist/boot.rc` or `/boot.rc` scripts, and yields/sleeps across ring3 slots on syscall ticks.
- IVSHMEM projection exchange across 3 VMs.
- Ring3 exec smoke path works (isolation still evolving; PIT-based preemption now enabled).

---

## Near-Term Milestones (0–3 months)

### 1) User-Mode Isolation (Priority)
- Stronger address-space boundaries.
- Permission model for memory + file access.
- Signals and robust process lifecycle (exec/exit/wait).
- Expand syscall surface while keeping strict pointer checks.
- Preemptive user scheduling + multiple ring3 processes per torus (PIT-driven time slicing).
  
Bootstrap guardrail:
- Introduce only a minimal bootstrap init to launch braided init; it must remain non-monolithic and must not hinder torus autonomy or system capabilities.

Success criteria:
- Faulty user pointers reliably return EFAULT.
- Non-user-mapped pages cannot be read/written by user processes.
- User processes cannot access kernel-only regions.

### 2) Network Stack Hardening
- Stable RX/TX under load (virtio-net).
- Raw TCP socket backend is in place (retransmit/backoff, FIN retry, window backpressure, ARP cache aging); harden connect/listen/accept/close beyond NET_LITE framing.
- Better packet validation + timeouts.

Success criteria:
- RX/TX stress tests complete without stalls.
- Raw TCP handshake and simple request/response works.

### 3) Filesystem Growth
- BlockFS directories + permissions (owner/group/other enforcement in place; single-group uid/gid).
- Stronger journaling and recovery (beyond current checksum/sanitize pass and journal write checks).
- Consistent stat/list semantics for /persist and root.

Success criteria:
- Directory listing works for nested paths.
- Reboot/recovery keeps filesystem consistent.

---

## Mid-Term Milestones (3–9 months)

### 4) Process Model & Scheduling
- Priority and fairness policies.
- Process migration across tori.
- Better accounting and observability.

### 5) Device Driver Expansion
- Hardening block and net drivers.
- Early input stack for keyboard/mouse.
- More storage targets (virtio-blk variants).

### 6) Baseline Compatibility
- Broader syscall coverage.
- ELF loader correctness with more binaries.
- POSIX subset validation.

---

## Long-Term Milestones (9+ months)

### 7) Distributed Mode
- Network projection exchange across machines.
- Failure detection + recovery (torus rebuild).

### 8) Production Hardening
- Security review.
- Stress and fuzz testing.
- Release-grade documentation.

---

## Benchmarks and Validation

- Use real logs only; no synthetic numbers.
- Run `./scripts/run_full_system_test.sh` and `./scripts/run_linux_baseline.sh` for snapshots.
- Quick UEFI boot check: `./scripts/run_quick_system_test.sh` (smoke benchmarks only).
- Default runs use smoke mode (`RSE_BENCH_SMOKE=1`) to validate real workloads; set `RSE_BENCH_SMOKE=0` for full UEFI/virtio/net micro-benchmarks.
- Keep benchmarks apples-to-apples before claiming comparisons.

## Bootable ISO Workflow

- Build: `./scripts/build_iso.sh`
- Run: `./scripts/run_iso.sh`
- Scripted workloads: `boot/boot.rc.sample` → `/persist/boot.rc`

---

## Sources of Truth

- Current status: `PROJECT_STATUS.md`
- Architecture: `docs/ARCHITECTURE.md`
- Design specs: `docs/design/`
- API: `docs/API_REFERENCE.md`
