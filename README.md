# RSE (Resilient Spatial Execution)

RSE is an experimental OS + runtime that explores braided-torus coordination (A → B → C → A) instead of a global scheduler, keeping coordination bounded while each torus remains autonomous. It produces a bootable UEFI ISO that runs real workloads and benchmarks. This repository bundles:

- **Runtime:** Betti-RDL single-torus engine plus braided projection exchange routines.
- **OS:** Bootable UEFI kernel with syscalls, VFS, virtio drivers, raw TCP, and a lightweight ring-3 supervisor for deterministic workloads and diagnostics.

## Highlights

- Bootable UEFI kernel with framebuffer dashboard, serial logging, compute/memory/RAMFS/HTTP workloads, plus virtio/UEFI FS and block benchmarks.
- Braided runtime that executes three 32³ toroidal lattices exchanging fixed-size projections via IVSHMEM.
- Ring3 scheduler with signal handling (SIGSTOP/SIGCONT, EINTR on wait, wake-on-signal semantics).
- Raw Ethernet/IP/TCP backend (`RSE_NET_RAW=1`) with retransmit/backoff, window updates, handshake cleanup, and loopback diagnostics.
- NET_LITE + virtio-net sockets honoring FIN/EPIPE/ECONNREFUSED, retransmit/seq-ack, TX/RX backpressure, and descriptor validation.
- BlockFS persistence with permission hygiene, zeroing on create/remove, and checksum recovery.
- IST-protected fault handlers plus int80 logging capture bad syscalls for the raw-TCP path.

## Getting Started

Install prerequisites and build the native kernel/tests:

```bash
sudo apt install cmake ninja-build qemu-system-x86_64 xorriso mcopy mmd sgdisk ripgrep
cmake -S src/cpp_kernel -B build/cpp_kernel
cmake --build build/cpp_kernel
```

### Build & Boot UEFI ISO

```bash
make -f boot/Makefile.uefi run-iso
```

Use the envvars below for specific flows:

```bash
RSE_NET_RAW=1 make -f boot/Makefile.uefi run-iso     # raw TCP smoke
RSE_BENCH_SMOKE=0 make -f boot/Makefile.uefi run-iso # full benchmarks
RSE_SINGLE_TORUS=1 make -f boot/Makefile.uefi run-iso # single-torus smoke
make -f boot/Makefile.uefi run                       # framebuffer dashboard
```

In the UI, use `B` (bench), `N` (net), `R` (reset), and `P`/`Q` (power off). Headless scripts set `RSE_AUTO_EXIT=1` so the ISO shuts down after benchmarks.

## Verification / Production Trace

```bash
QEMU_BOOT_TIMEOUT=300 RSE_NET_RAW_TEST=1 ./scripts/run_full_system_test.sh
```

This rebuilds the native tests, braided demo, and boots the headless UEFI ISO with the raw-TCP smoke. Verified artifacts:

- `build/boot/boot.log` – serial output showing `[init] rawtcp ok` plus compute/memory/ramfs/fastio/UEFI/virtio/http stats ending with `[RSE] benchmarks end`.
- `build/boot/boot.make.log` – the make/QEMU invocation log.
- `benchmarks/net_exchange/torus0.log` … `torus2.log` – projection exchange logs.

Keep these logs with release notes as the production trace for the bootable ISO.

## Hardware Benchmarking (Apples-to-Apples)

For real hardware comparisons (not QEMU), follow:

- `docs/HARDWARE_BENCH.md`

## Projection Exchange

```bash
scripts/run_projection_exchange.sh
```

Boots three VMs that coordinate via IVSHMEM.

## Braided Runtime Demo

```bash
cd src/cpp_kernel/braided
mkdir -p build && cd build
cmake ..
make
./braided_demo
```

## Documentation

- `PROJECT_STATUS.md` – implementation status + benchmark snapshots.
- `docs/OS_ROADMAP.md` – high-level roadmap toward production.
- `docs/design/` – detailed component writeups.
- `docs/phase_reports/`, `docs/status/` – historical updates.

## License

MIT License. See `LICENSE`.
