# RSE Operating System: Roadmap

Last Updated: December 28, 2025
Status: Active development (prototype)

---

## Summary

This roadmap tracks the path from the current prototype to a production-grade OS. It reflects what is already implemented and the next milestones needed to harden isolation, networking, and storage.

---

## Current Baseline (Now)

- Bootable UEFI kernel (serial + framebuffer) with dashboard.
- Syscall dispatcher with user-pointer validation and per-torus dispatch.
- MemFS + BlockFS with checksum journal; /persist mounted with directory paths.
- Minimal network stack (ARP/UDP parsing + loopback).
- IVSHMEM projection exchange across 3 VMs.
- Ring3 exec smoke path works (isolation still evolving).

---

## Near-Term Milestones (0–3 months)

### 1) User-Mode Isolation (Priority)
- Stronger address-space boundaries.
- Permission model for memory + file access.
- Signals and robust process lifecycle (exec/exit/wait).
- Expand syscall surface while keeping strict pointer checks.

Success criteria:
- Faulty user pointers reliably return EFAULT.
- Non-user-mapped pages cannot be read/written by user processes.
- User processes cannot access kernel-only regions.

### 2) Network Stack Hardening
- Stable RX/TX under load (virtio-net).
- Minimal TCP support (connect/listen/accept/close).
- Better packet validation + timeouts.

Success criteria:
- RX/TX stress tests complete without stalls.
- TCP handshake and simple request/response works.

### 3) Filesystem Growth
- BlockFS directories + permissions (basic directory entries now exist under /persist).
- Stronger journaling and recovery.
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
- Keep benchmarks apples-to-apples before claiming comparisons.

---

## Sources of Truth

- Current status: `PROJECT_STATUS.md`
- Architecture: `docs/ARCHITECTURE.md`
- Design specs: `docs/design/`
- API: `docs/API_REFERENCE.md`
