---
**Last Updated**: December 18, 2025 at 13:31 UTC
**Status**: Current
---

# Betti-RDL Runtime
> **A Space-Time Native Computational Substrate**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-1.0.0-blue)](https://github.com/betti-labs/betti-rdl)
[![CI Status](https://github.com/betti-labs/betti-rdl/actions/workflows/ci.yml/badge.svg)](https://github.com/betti-labs/betti-rdl/actions)
![Platform](https://img.shields.io/badge/platform-win--x64%20%7C%20linux--x64-lightgrey)

Betti-RDL is a deterministic, event-driven runtime that guarantees **O(1) memory** (constant spatial complexity) by executing computation over a fixed-size **32×32×32 toroidal lattice**.

---

## Quick links

- **Getting started**: [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md)
- **API reference (all bindings)**: [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md)
- **Example code**: [Examples](#example-code)
- **Troubleshooting**: [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md)
- **Architecture deep dive**: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Contributing**: [`CONTRIBUTING.md`](CONTRIBUTING.md)
- **Phase 3 completion report**: [`docs/PHASE3_COMPLETION_REPORT.md`](docs/PHASE3_COMPLETION_REPORT.md)
- **Full project status / assessment**: [`COMPREHENSIVE_RSE_REVIEW.md`](COMPREHENSIVE_RSE_REVIEW.md)

---

## Status snapshot (Dec 2024)

| Area | Status | Notes |
|---|---|---|
| Core kernel (C++ “metal”) | ✅ Production-ready | O(1) memory + thread-safe injection validated |
| Rust binding | ✅ Works | Automatic CMake build via `build.rs` |
| Python binding | ✅ Works (validated) | Requires Python toolchain + shared lib build |
| Node.js binding | ✅ Works (validated) | Requires Node + node-gyp toolchain + shared lib build |
| Go binding | ✅ Works (validated) | Requires Go + CGO + shared lib build |
| Grey compiler | ⚠️ Partial | Tests/codegen validated; parser fails on some `.grey` demo files |

> For the full validation story (including environment/setup caveats), see:
> - [`docs/PHASE3_COMPLETION_REPORT.md`](docs/PHASE3_COMPLETION_REPORT.md)
> - [`docs/VALIDATION_RESULTS.md`](docs/VALIDATION_RESULTS.md)
> - [`COMPREHENSIVE_RSE_REVIEW.md`](COMPREHENSIVE_RSE_REVIEW.md)

---

## Killer demos (verified)

These are the “why this exists” scenarios, validated end-to-end:

| Demo | What it proves | Result |
|---|---|---|
| **Logistics Swarm** | Massive agent simulation | **13.7M deliveries/sec** |
| **Silicon Cortex** | Neuromorphic/spiking workloads | **13.9M spikes/sec** |
| **Global Contagion** | Deep recursive propagation | **O(1) memory** (0 bytes growth) |

---

## Performance benchmarks (validated)

- **Peak throughput (single kernel):** **16.8M events/sec**
- **Parallel scaling:** **285.7M aggregate events/sec** with **16 isolated kernels**
- **Memory stability:** **0 bytes delta** on 100,000+ event chains (and contagion-scale propagation)

Source-of-truth benchmark writeups:
- [`docs/VALIDATION_RESULTS.md`](docs/VALIDATION_RESULTS.md)
- [`docs/RSE_Status_Report.md`](docs/RSE_Status_Report.md)

---

## ⚡ Quick start (5 minutes)

**Rust (recommended)**

```bash
git clone https://github.com/betti-labs/betti-rdl
cd betti-rdl/rust
cargo run --example basic
```

For Python / Node.js / Go / C++ quick starts, see:
- [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md)

To validate all bindings against a single shared kernel build:

```bash
# From repo root
./scripts/run_binding_matrix.sh
```

---

## Example code

- Rust: [`rust/examples/basic.rs`](rust/examples/basic.rs)
- Python: [`python/example.py`](python/example.py)
- Node.js: [`nodejs/example.js`](nodejs/example.js)
- Go: [`go/example/main.go`](go/example/main.go)
- C++ mega demo: [`src/cpp_kernel/demos/scale_demos/mega_demo.cpp`](src/cpp_kernel/demos/scale_demos/mega_demo.cpp)

Grey (early-stage):
- Compiler docs: [`grey_compiler/README.md`](grey_compiler/README.md)
- Demo sources: [`grey_compiler/examples/`](grey_compiler/examples/)

---

## Documentation index

These docs already exist and are the recommended entry points:

- [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) — install/build/run by language
- [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md) — complete API for C++, C, Rust, Python, Node.js, Go
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — bounded-memory design, scheduler, thread-safety
- [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) — common build/runtime issues across bindings
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — dev setup, standards, PR process
- [`docs/PHASE3_COMPLETION_REPORT.md`](docs/PHASE3_COMPLETION_REPORT.md) — binding validation + documentation sprint results
- [`COMPREHENSIVE_RSE_REVIEW.md`](COMPREHENSIVE_RSE_REVIEW.md) — comprehensive project status and roadmap

---

## License

MIT License. Copyright (c) 2025 Betti Labs.
