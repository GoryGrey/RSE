# RSE Repo Map

This map is a quick index of where the *real code* lives vs. design docs vs. status reports.
Use it to navigate without chasing scattered files.

## Boot + Kernel (what actually runs)
- `boot/` - UEFI loader, kernel entry, build scripts (Limine/GRUB/UEFI).
  - `boot/kernel.c` - kernel entry + device/bench code + dashboard UI/inputs/console
  - `boot/kernel_os.cpp` - OS layer glue (hooks `src/cpp_kernel/os` into kernel)
  - `boot/Makefile.uefi` - UEFI ISO build + QEMU run

## OS Layer (core design + implementation)
- `src/cpp_kernel/os/` - OS concepts + implementation
  - `README.md` - OS vision/roadmap
  - `*_DESIGN.md` - syscall, memory, IO, VFS, scheduler designs
  - `Syscall*.h`, `VFS.h`, `MemFS.h`, `BlockFS.h`, `OSProcess.h`, `TorusScheduler.h` - working code

## Core RSE Architecture + Theory
- `docs/ARCHITECTURE.md` - high-level system description
- `docs/design/` - braided torus design + analysis
- `docs/design/RSE_PROJECTION_EXCHANGE_SPEC.md` - projection exchange network spec
- `docs/design/RSE_PROJECTION_TASK_MAP.md` - projection exchange implementation map
- `docs/RSE_Whitepaper.md` - primary research paper

## Status / Phase Reports (historical claims)
- `PROJECT_STATUS.md` - repo-level status summary
- `docs/status/` - executive summaries + review docs
- `docs/phase_reports/` - phase completion writeups

## Simulator + Tooling
- `COG/` - genesis sim + visor UI + CLI tooling
- `benchmarks/` - workload harnesses (if any)
- `scripts/` - helper scripts (baseline + UDP/HTTP sender)

## Compiler / Language
- `grey_compiler/` - compiler work
- `docs/grey_language_spec.md` - language spec
- `docs/grey_compiler_architecture.md` - compiler design

## Quick Guidance
- If you want **what boots today**, start in `boot/`.
- If you want **your OS design**, start in `src/cpp_kernel/os/` + `docs/design/`.
- If you want **claims/history**, check `PROJECT_STATUS.md` + `docs/status/`.
