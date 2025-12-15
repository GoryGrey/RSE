# Betti‑RDL Validation & Product Report

Date: 2025‑12‑15

## Executive summary

Betti‑RDL is presented as a deterministic, event‑driven runtime that maps computation onto a fixed 3‑torus lattice to avoid stack growth (“recursion as replacement”) and to enable highly parallel workloads.

In this repo’s current **prototype** implementation, the core “compute” kernel is built on STL containers (`std::priority_queue`, `std::unordered_map`) plus a global `operator new` hook used only for coarse memory accounting. As shipped, the design intent (bounded memory, parallel isolation) is compelling, but the implementation is not yet a strict, mechanically‑enforced O(1) allocator/scheduler.

This ticket validated:
- The C++ Release build and benchmark executables run successfully.
- The “Mega Demo” scenarios execute end‑to‑end with measurable throughput.
- Python (pybind11) and Node.js (N‑API) bindings compile and run end‑to‑end.
- Benchmark claims were compared against measured results on the provided VM.

All raw outputs are saved under `docs/reports/*.txt`.

## Test environment

See `docs/reports/env.txt`.

Highlights:
- CPU: Intel Xeon Platinum 8581C @ 2.10GHz
- Cores/threads available in VM: 3 (single thread per core)
- RAM: ~10 GiB

This is important for interpreting scaling claims that reference 16 threads.

## What was required to make benchmarks meaningful

During validation, two correctness issues were found that made published benchmark numbers misleading:

1. `run(max_events)` semantics in `BettiRDLCompute` / `BettiRDLKernel` were implemented as “run until total events_processed reaches max_events”, which caused repeated `run()` calls to do no work after the first batch.
2. The C API header used `size_t` without including `<stddef.h>` and the CMake project did not enable C, preventing the C API test from compiling on Linux.

These were fixed so that:
- `run(n)` processes up to **n additional** events.
- The deep recursion benchmark actually executes the requested number of steps.

## Objective 1 — Reproduce core benchmarks

### 1) Mega demo (“killer app” scenarios)
Command:
```bash
cd src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./mega_demo
```
Raw output: `docs/reports/mega_demo.txt`

Measured results:

| Scenario | Claimed in README | Measured (this VM) | Notes |
|---|---:|---:|---|
| Logistics swarm (1,000,000 deliveries) | 2.4M deliveries/sec | 4.26M deliveries/sec (235ms) | Implemented as batched inject+run; measures event processing throughput more than a realistic routing model. |
| Silicon cortex (500,000 spikes) | 2.4M spikes/sec | 7.69M spikes/sec (65ms) | Batched inject+run; not a biophysically accurate SNN model yet. |
| Contagion (1,000,000 infection steps) | “0 bytes memory growth” | +24 bytes (1311076B → 1311100B) | Uses a single recursive chain to avoid queue growth; demonstrates “infinite steps without storing 1M events”. |

### 2) Stress test suite
Command:
```bash
./stress_test
```
Raw output: `docs/reports/stress_test.txt`

Measured results:

| Test | Measured result | Repo claim comparison |
|---|---:|---|
| Firehose throughput (5,000,000 events) | 35.7M events/sec (0.14s) | README claims 4.33M EPS peak; measured is higher on this VM, but the “compute” per event is still lightweight. |
| Deep Dive recursion (100,000 dependent events) | 100,000 events processed; +380 bytes net tracked | README claims “0 bytes growth” at scale; this prototype shows small fixed overhead. The memory tracker is not OS RSS; it is a global counter in `Allocator.h`. |
| Swarm (16 threads × 100,000 events) | 133M EPS aggregate (time rounded to 0.01s) | This VM has 3 cores; 16 threads is oversubscribed. Also output interleaves across threads. |

### 3) Parallel scaling efficiency
Command:
```bash
./parallel_scaling_test_v2
```
Raw output: `docs/reports/parallel_scaling_test.txt`

Measured results (1,000,000 events per instance):

| Instances | Throughput (EPS) | Speedup | Efficiency |
|---:|---:|---:|---:|
| 1 | 12.96M | 1.00x | 100% |
| 2 | 24.48M | 1.89x | 94% |
| 4 | 28.98M | 2.24x | 56% |
| 8 | 24.50M | 1.89x | 24% |
| 16 | 12.37M | 0.95x | 6% |

Interpretation:
- Scaling is close to linear up to the **available core count** (here: ~2× is good on a 3‑core VM).
- Above that, oversubscription dominates and throughput falls.
- The current implementation also relies on STL containers and a global allocator hook (`g_memory_used`) that is **not thread‑safe**, which can distort parallel measurements and must be addressed before making strong scaling claims.

## Objective 2 — Test language bindings

### Python (pybind11)
Steps executed:
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e python
python python/example.py
```
Raw output: `docs/reports/python_example.txt`

Status: Works end‑to‑end (spawn, inject, run, read counters).

Limitations observed:
- The Python binding compiles C++ sources directly and does not link against the built `libbetti_rdl_c.so`; packaging/versioning across languages will be harder until a single shared core library is used.
- The prototype overrides global `operator new` (via `Allocator.h`) inside the extension module, which is risky in real Python processes.

### Node.js (N‑API)
Steps executed:
```bash
cd nodejs
npm install
node example.js
```
Raw output: `docs/reports/node_example.txt`

Status: Works end‑to‑end (spawn, inject, run, read counters).

Limitations observed:
- Like Python, the addon compiles C++ directly rather than consuming a stable C ABI library.
- Native addon distribution requires toolchains per platform (typical for N‑API addons but relevant for product packaging).

## Objective 3 — Product angle evaluation

### 1) Agent‑Based Simulation (drones, logistics, trading)
**Strengths**
- Deterministic discrete‑event execution is a strong fit for ABM.
- The contagion demo pattern (drive many steps from a small state footprint) is useful for “simulate huge populations without materializing all agents”, if generalized.

**Realistic use cases**
- Epidemic spread where most agents are homogeneous and can be represented as counters/compartments.
- Logistics / order routing / inventory flow models where event scheduling dominates.
- Market microstructure simulations where determinism and reproducibility matter.

**Performance characteristics**
- Very high single‑instance event throughput in this prototype (tens of M EPS).
- Scaling is good up to available cores; beyond that, oversubscription and current implementation details reduce efficiency.

**Competitive context**
- Many established ABM frameworks exist (Mesa, Repast, MASON, GAMA, AnyLogic, FLAME GPU).
- Differentiation must be: (1) determinism, (2) bounded‑memory recursion/event processing, (3) “fast enough in Python” via a C++ core.

**Challenges / limitations**
- The current data structures are STL‑based and do not enforce bounded memory.
- ToroidalSpace uses a string key map, which is not suitable for a performance‑critical core.

**Feasibility**: High (as a library/runtime for simulation).

### 2) Neuromorphic AI / SNNs
**Strengths**
- Event‑driven runtimes map naturally to spike processing.

**Realistic use cases**
- Research simulators, small‑to‑medium networks, event‑driven inference.

**Competitive context**
- Strong incumbents: Brian2, Nengo, Norse, Lava, SpikingJelly/snnTorch.

**Challenges**
- Needs real neuron/synapse models, plasticity rules, GPU/vectorization, and interoperability with ML tooling.

**Feasibility**: Medium (longer R&D cycle).

### 3) Serverless backend (Node.js, Python services)
**Strengths**
- Determinism and bounded memory are attractive in multi‑tenant environments.

**Competitive context**
- Extremely competitive: V8 isolates, WASM runtimes (Wasmtime), Cloudflare Workers, AWS Lambda, etc.

**Challenges**
- Requires sandboxing, isolation, billing/metering, multi‑tenant scheduling, security hardening, observability.

**Feasibility**: Low in the short term.

### 4) Scientific computing (massive recursion / recursive algorithms)
**Strengths**
- The “Deep Dive” pattern is a clear wedge: run extremely deep iterative/recursive workflows without stack growth.

**Realistic use cases**
- Backtracking search, constraint solving, symbolic execution, tree/graph traversal with bounded memory.
- Deterministic replayable simulations for research.

**Competitive context**
- Many languages mitigate recursion via TCO/trampolines, but general “bounded memory recursion runtime” is uncommon as a drop‑in library.

**Challenges**
- Must prove correctness on real algorithms (DFS, SAT‑like workloads) and provide ergonomic APIs.

**Feasibility**: Medium‑high (library product, but needs a clearer API and examples).

## Primary recommendation

**Primary product angle: Agent‑based / discrete‑event simulation core (Python‑first), positioned as a deterministic high‑throughput event engine with bounded‑memory execution patterns.**

Why this is the best immediate opportunity:
- Fastest time‑to‑market: the demos and bindings already point in this direction.
- Clear buyer/user: simulation engineers, researchers, ops/logistics analysts.
- Value proposition is easy to communicate: reproducibility + high event throughput + bounded memory patterns.
- Lower competitive risk than “serverless platform”; more direct than “neuromorphic AI” which requires heavy domain R&D.

## Secondary recommendations

1. **Scientific recursion/search kernel** as a specialized library layer on top of the same runtime (DFS/backtracking examples, constraint solving).
2. **Neuromorphic/SNN simulation** as a longer‑term vertical once the core scheduling/allocator story is hardened.

## Technical debt / improvements needed (to support the recommendation)

Highest‑impact items:
1. Replace STL containers in the hot path with bounded / preallocated structures (ring buffers, fixed heaps) and/or `std::pmr` backed by a custom arena.
2. Remove or isolate the global `operator new` override; make memory tracking thread‑safe and measure RSS/peak RSS in benchmarks.
3. Make the kernel thread‑safe (or explicitly single‑threaded) and provide a clear concurrency model.
4. Replace `ToroidalSpace` string keys with a flat index (`idx = x + W*(y + H*z)`) and fixed arrays.
5. Provide benchmark CLI options (event counts, thread counts) and report percentile latencies, not just average EPS.
6. Unify bindings around the C API shared library (`libbetti_rdl_c`) so Python/Node/Rust/Go all consume the same core binary.

## Suggested next steps

1. Create a “benchmark harness” executable that runs:
   - throughput, latency percentiles, memory peak
   - scaling tests up to physical core count
2. Implement a real ABM reference model (e.g., SIR epidemic with parameter sweeps) and publish reproducible results.
3. Package Python wheels (manylinux) and prebuilt Node binaries for key platforms.
4. Add CI tests that run:
   - `stress_test` at smaller sizes
   - Python and Node example smoke tests

---

### Appendix: raw outputs
- `docs/reports/env.txt`
- `docs/reports/mega_demo.txt`
- `docs/reports/stress_test.txt`
- `docs/reports/parallel_scaling_test.txt`
- `docs/reports/python_example.txt`
- `docs/reports/node_example.txt`
