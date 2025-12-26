# Design Alignment

This file maps the design docs to current implementation status and code locations.

## Design Sources
- `docs/ARCHITECTURE.md` (Betti-RDL runtime architecture)
- `docs/design/BRAIDED_TORUS_DESIGN.md`
- `docs/design/RSE_BRAIDED_TORUS_ANALYSIS.md`
- `docs/design/BRAIDED_RSE_FINAL_REPORT.md`
- `docs/OS_ROADMAP.md`
- `docs/MEMORY_DESIGN.md`
- `src/cpp_kernel/os/*_DESIGN.md`
- `docs/grey_language_spec.md`, `docs/grey_compiler_architecture.md` (tracked, not in kernel yet)

## Braided Runtime (Betti-RDL + braided tori)
Implemented
- Betti-RDL core runtime (ToroidalSpace, allocator, fixed structures, event loop) in `src/cpp_kernel/`.
- Braided scaffolding (Projection, BraidCoordinator, TorusBraid, BraidedKernel variants) in `src/cpp_kernel/braided/` + demos/tests.

Partial
- Braided demos exist, but not integrated into the bootable kernel or OS scheduler.
- Phase 1 tests referenced in docs are not wired into the boot flow.

Missing
- Boundary coupling, self-correction, distributed projection exchange, torus reconstruction, adaptive braid interval.
- Hardware-aware integration (real I/O, memory, scheduling) in the braided kernel.

## OS Roadmap (docs/OS_ROADMAP.md)
Phase 1 Foundations: partial
- Process abstraction: OSProcess exists; fork is shallow; exec/wait/kill missing.
- Memory management: PhysicalAllocator + PageTable + VirtualAllocator exist; brk/mmap/munmap/mprotect wired; no faults/protection enforcement.
- I/O subsystem: VFS + MemFS + device layer exists; real hardware drivers minimal.
- Syscall interface: dispatcher present with core file and memory calls.

Phase 2-4: not started in OS kernel
- Emergent scheduling, process migration, distributed projection exchange, fault tolerance, bare-metal boot plus benchmarks.

## Syscalls (`src/cpp_kernel/os/SYSCALL_DESIGN.md`)
Implemented
- getpid/getppid/exit/fork
- open/close/read/write/lseek/unlink
- brk/mmap/munmap/mprotect

Partial / Missing
- exec/wait/kill semantics
- stat, time, sleep/nanosleep
- IPC (pipe/dup/dup2/signal)
- User/kernel separation and real syscall entry path

## Scheduler (`src/cpp_kernel/os/EMERGENT_SCHEDULER_DESIGN.md`)
Implemented
- TorusScheduler with per-torus queues, priorities, accounting.

Missing / Partial
- Real context switch + CPU register save/restore
- Cross-torus migration and projection-based constraints
- Emergent scheduling (no global scheduler) not yet enforced

## Memory (`docs/MEMORY_DESIGN.md` and `src/cpp_kernel/os/MEMORY_DESIGN.md`)
Implemented
- PhysicalAllocator + PageTable + VirtualAllocator types
- Per-process vmem wiring (OSProcess::initMemory, syscalls)

Missing / Partial
- Page faults, protection enforcement, CoW, demand paging
- Per-torus pools and torus-aware policies
- Actual MMU programming in boot kernel

## I/O + VFS (`src/cpp_kernel/os/IO_DESIGN.md`, `VFS_DESIGN.md`)
Implemented
- Device abstraction + DeviceManager
- Console, null, zero, loopback devices
- VFS + MemFS
- UEFI block device at `/dev/blk0`
- virtio-net (`/dev/net0`) with UEFI SNP fallback (raw frames)

Missing / Partial
- Directory ops, permissions, inode metadata, caching
- Filesystem on block device (only MemFS / raw block access)
- Full network stack (IP/TCP, sockets)
- Real driver model (interrupts, DMA, etc)

## Language / Compiler (`docs/grey_language_spec.md`, `docs/grey_compiler_architecture.md`)
Status
- Design docs exist; no kernel integration yet.

## Current Focus
- Keep bootable UEFI kernel stable while wiring OS layer to design intent.
- Fill missing OS foundations (exec/wait, memory faults, filesystem on `/dev/blk0`).
- Decide how and when the braided runtime becomes the kernel scheduler.

## Prioritized Implementation Track
1) Boot + visibility
   - Ensure kernel prints to serial and framebuffer, with a clear early init banner.
   - Keep UEFI boot path stable (ISO + FAT ESP).
2) Process + syscall completeness
   - exec/wait/kill, basic ELF loader, errno coverage, user/kernel boundary stubs.
3) Memory correctness
   - Page fault handler, protection flags, basic CoW, torus-local allocators.
4) Storage + filesystem
   - Directory ops and metadata; mount a simple FS on `/dev/blk0`.
5) Networking
   - virtio-net RX path, minimal IP/UDP stack, loopback HTTP server.
6) Braided integration
   - Decide runtime handoff: TorusScheduler vs braided kernel driver for OS events.
