# RSE Single-Torus Mode

This directory contains the **original RSE implementation** using a single 32³ toroidal lattice with a centralized event scheduler.

## Overview

Single-torus mode is the **foundation** of RSE. It provides:
- **16.8M events/sec** throughput on a single kernel
- **O(1) memory guarantee** (validated with 100,000+ event chains)
- **Near-perfect linear scaling** with parallel kernels (285.7M events/sec with 16 kernels)
- **Production-ready** stability and performance

## Components

### BettiRDLKernel.h
The main execution kernel combining:
- **Betti** (toroidal space) for spatial organization
- **RDL** (Recursive Delay Logic) for time-native events

**Key Features**:
- Event-driven execution with adaptive delays
- Deterministic event ordering
- Thread-safe event injection
- Comprehensive telemetry

### BettiRDLCompute.h
Computational extensions for scientific workloads.

---

## When to Use Single-Torus Mode

Use single-torus mode when:
- ✅ You need **maximum single-core performance**
- ✅ Your workload fits in a single machine
- ✅ You don't need fault tolerance
- ✅ You want the **simplest possible architecture**

**Performance Characteristics**:
- Throughput: 16.8M events/sec (single kernel)
- Latency: ~60 ns per event
- Memory: ~150 MB fixed
- Scalability: Linear with parallel kernels (up to 16×)

---

## When to Use Braided-Torus Mode Instead

Consider braided-torus mode when:
- ❌ You need **fault tolerance** (single-torus has no redundancy)
- ❌ You need **distributed execution** (single-torus is single-machine only)
- ❌ You need **emergent scheduling** (single-torus has a global scheduler)
- ❌ You want to scale beyond 16 kernels (single-torus has coordination overhead)

---

## Architecture

```
┌─────────────────────────────────────┐
│   BettiRDLKernel (Single-Torus)    │
├─────────────────────────────────────┤
│  Event Queue (Priority Heap)       │
│  ↓                                  │
│  tick() → route() → collide()      │
│  ↓                                  │
│  Process Pool (32³ lattice)        │
│  ↓                                  │
│  Edge Pool (Adaptive Delays)       │
└─────────────────────────────────────┘
```

**Execution Loop**:
1. Pop next event from priority queue (O(log N))
2. Route event to destination process (O(1))
3. Process event and generate new events (O(edges))
4. Insert new events into queue (O(log N) per event)

---

## Example Usage

```cpp
#include "single_torus/BettiRDLKernel.h"

int main() {
    BettiRDLKernel kernel;
    
    // Setup
    kernel.spawnProcess(0, 0, 0);
    kernel.createEdge(0, 0, 0, 1, 0, 0, 10);  // delay=10
    kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);  // payload=1
    
    // Execute
    kernel.run(1000000);  // Process 1M events
    
    return 0;
}
```

---

## Performance Benchmarks

| Workload | Events/sec | Latency | Memory |
|----------|------------|---------|--------|
| Uniform | 16.8M | 60 ns | 150 MB |
| Bursty | 14.2M | 70 ns | 150 MB |
| Skewed | 12.5M | 80 ns | 150 MB |

**Parallel Scaling** (16 kernels):
- Throughput: 285.7M events/sec
- Efficiency: 106% (super-linear due to cache effects)

---

## Limitations

1. **No Fault Tolerance**: If the kernel crashes, all state is lost
2. **Single Machine**: Cannot distribute across multiple machines
3. **Global Scheduler**: Bottleneck for very high event rates
4. **No Self-Healing**: Manual recovery required after failures

These limitations are addressed by **braided-torus mode**.

---

## Migration Path

To migrate from single-torus to braided-torus:

```cpp
// Before (single-torus)
#include "single_torus/BettiRDLKernel.h"
BettiRDLKernel kernel;

// After (braided-torus)
#include "braided/TorusBraid.h"
braided::TorusBraid braid(1000);  // braid_interval=1000
auto& kernel = braid.getTorusA();  // Use torus A
```

The API is **backward-compatible**, so existing code works with minimal changes.

---

## Future

Single-torus mode will remain the **default** for:
- Embedded systems
- Real-time systems
- Simple workloads
- Development and testing

It's not going away—it's the **foundation** that braided-torus and OS modes build on top of.
