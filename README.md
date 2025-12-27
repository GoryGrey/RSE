# RSE (Resilient Spatial Execution)

**Last Updated**: December 26, 2025 (UEFI run-iso: user-mode window + brk/mmap remap)  
**Status**: Research prototype. Bootable UEFI kernel with an interactive dashboard (keyboard/mouse) and in-kernel workloads; braided projection exchange works in multi-VM via shared memory. Ring3 exec smoke passes with a bounded user-mode window, but full user-mode isolation and network transport are still in progress.

**Quick Links**: [Project Status](PROJECT_STATUS.md) | [Documentation](#documentation)

---

## Overview

RSE explores a braided-torus execution model for coordinating computation without a global scheduler. The core idea is to run multiple toroidal lattices (tori) in parallel and periodically exchange **fixed-size projections** of state to apply constraints cyclically (A -> B -> C -> A). This keeps coordination overhead constant (O(1) projection size) while preserving autonomy across tori.

This repo contains both layers:
- **Runtime**: the Betti-RDL single-torus + braided execution engine (standalone library/runtime).
- **OS**: a bootable UEFI kernel scaffold with syscalls, VFS, devices, and userspace runner.

This repo contains:
- Bootable UEFI kernel with an OS scaffold and in-kernel workload harness.
- Single-torus and braided runtime implementations.
- Design and theory documents that specify the projection model and constraints.

---

## Architecture Summary

```
Torus A (32^3 lattice) ⟲
    ↓ projection      ↑
Torus B (32^3 lattice) ⟲
    ↓ projection      ↑
Torus C (32^3 lattice) ⟲
    ↓ projection      ↑
(cycle repeats)
```

Key properties:
- **Fixed-size projections** (~4.2 KB) independent of workload size.
- **Cyclic constraint exchange** instead of a global controller.
- **O(1) coordination overhead** by design.

---

## What Works Today

- **Bootable UEFI kernel** in QEMU (serial + framebuffer).
- **In-kernel workloads**: compute, memory, RAMFS I/O, UEFI FS I/O, raw block I/O, HTTP loopback.
- **/dev/net0 UDP loopback** with simple RX/echo path (no full IP stack yet).
- **Init shell demo** (stdout + cat + device probes).
- **Cooperative userspace tasks** (in-kernel runner using syscalls).
- **Ring3 smoke + exec path** in UEFI (user-mode window + page-table refresh on brk/mmap).
- **Framebuffer dashboard** (boot + benchmark summary panels, keyboard/mouse input, on-screen console).
- **Braided runtime** (single-node, in-kernel projections + constraint application).
- **Projection exchange across 3 VMs** via IVSHMEM shared memory transport.
- **Block-backed persistence** via a minimal fixed-slot BlockFS mounted at `/persist`.

## Known Limitations

- No full user-mode isolation yet; ring3 exec works in UEFI with a bounded window, but most userspace remains cooperative/in-kernel.
- Network RX is working for basic ARP/UDP, but needs stress and driver hardening.
- No full IP/TCP stack (only minimal IPv4/UDP parsing + echo).
  - Raw frame mode can be enabled at build time: `RSE_NET_RAW=1`.
- BlockFS is minimal (flat namespace, fixed slots, no directories or journaling).
- Workload init is currently **one-shot** per boot; dashboard benchmarks rerun compute/I/O without re-initializing the OS layer.

---

## Benchmarks (QEMU, TSC cycles)

Cycle-counted benchmarks captured in headless QEMU (see `PROJECT_STATUS.md` for the latest run):

- **Compute**: 400,000 ops, 23,175,219 cycles (57 cycles/op)
- **Memory**: 67,108,864 bytes, 155,604,992 cycles (2 cycles/byte)
- **RAMFS File I/O**: 288 ops, 9,260,536 cycles (32154 cycles/op)
- **UEFI FAT File I/O (USB disk)**: 144 ops, 2,598,435,038 cycles (18044687 cycles/op)
- **UEFI Raw Block I/O (USB disk)**: 524,288 bytes, write 72,108,594 cycles (137 cycles/byte), read 90,153,268 cycles (171 cycles/byte)
- **Virtio-Block I/O (disk)**: 512 bytes, write 450,520,608 cycles (879923 cycles/byte), read 62,756,619 cycles (122571 cycles/byte)
- **Net ARP Probe (virtio-net RX)**: 64 bytes, 5,466,842 cycles
- **UDP/HTTP RX Server (raw)**: bench rx=0 udp=0 http=0, 804,265,218 cycles (proof: rx=393 udp=197 http=196; see `build/boot/proof.log`)
- **HTTP Loopback**: 50,000 requests, 101,421,019 cycles (2028 cycles/req)

Notes:
- These are **QEMU TSC cycle counts**, not wall-clock time.
- Parsed benchmark files live in `benchmarks/uefi_bench.json` (raw log: `benchmarks/uefi_serial.log`); refresh via scripts when needed.
- Linux baseline results live in `benchmarks/linux_baseline.json`.
- External UDP/HTTP RX proof log is saved at `build/boot/proof.log`.

---

## Build and Run

### UEFI Kernel (QEMU)
```bash
make -f boot/Makefile.uefi run-iso
```

To view the framebuffer dashboard, run the disk target:
```bash
make -f boot/Makefile.uefi run
```

Optional raw frame mode for `/dev/net0`:
```bash
make -f boot/Makefile.uefi RSE_NET_RAW=1 run-iso
```

### External RX Test (UDP/HTTP)
Run with UDP host-forwarding enabled:
```bash
NETDEV_HOSTFWD=,hostfwd=udp::8080-:8080,hostfwd=udp::40001-:40000 \
make -f boot/Makefile.uefi run
```
Send UDP payloads from the host:
```bash
scripts/send_udp_http.py --mode http --count 200 --port 8080
scripts/send_udp_http.py --mode udp --count 200 --port 40001
```

### Proof Run (build + boot + external RX)
```bash
scripts/run_proof.sh
```

### Dashboard Controls
- Arrow keys or A/D: change selection
- Tab: next icon
- Space/Enter: run selected action
- B/N/R: run Bench / Net / Reset directly

### Projection Exchange (3 VMs, IVSHMEM)
```bash
scripts/run_projection_exchange.sh
```

### Braided Runtime Demo (Userspace)
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
- `docs/phase_reports/` and `docs/status/` - legacy phase docs (historical, not current status)
- `docs/design/BRAIDED_TORUS_DESIGN.md` - projection model and braid coordinator
- `docs/design/RSE_PROJECTION_EXCHANGE_SPEC.md` - projection exchange wire spec
- `docs/design/RSE_PROJECTION_TASK_MAP.md` - implementation steps
- `docs/OS_ROADMAP.md` - OS roadmap and milestones
- `docs/RSE_Whitepaper.md` - theory and motivation

---

## License

MIT License. See `LICENSE`.
