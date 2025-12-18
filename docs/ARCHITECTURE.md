---
**Last Updated**: December 18, 2025 at 13:31 UTC
**Status**: Current
---

# Betti-RDL Architecture Guide

This document explains the internal architecture, design decisions, and performance characteristics of Betti-RDL.

## Table of Contents

- [Overview](#overview)
- [Core Components](#core-components)
- [Memory Architecture](#memory-architecture)
- [Event Processing](#event-processing)
- [Thread Safety](#thread-safety)
- [Performance Characteristics](#performance-characteristics)
- [Design Decisions](#design-decisions)
- [Limitations & Workarounds](#limitations--workarounds)

---

## Overview

Betti-RDL is a **space-time computation runtime** that guarantees **O(1) memory** regardless of workload size.

### Key Innovation

Traditional event-driven systems use **unbounded queues** that can grow without limit:
```
Traditional: Queue grows → Memory grows → Eventually runs out
```

Betti-RDL uses **spatial reuse** in a bounded grid:
```
Betti-RDL: Events reuse grid cells → Memory constant → Never runs out
```

### Architecture Stack

```
┌─────────────────────────────────────┐
│  Language Bindings                  │  ← Python, Rust, Node.js, Go
│  (FFI wrappers)                     │
├─────────────────────────────────────┤
│  C API Layer                        │  ← betti_rdl_c_api.h
│  (extern "C" interface)             │
├─────────────────────────────────────┤
│  BettiRDLKernel / BettiRDLCompute   │  ← Event scheduler
│  (C++ event-driven scheduler)      │
├─────────────────────────────────────┤
│  ToroidalSpace (32³ grid)          │  ← Spatial substrate
├─────────────────────────────────────┤
│  BoundedAllocator                   │  ← O(1) memory pools
├─────────────────────────────────────┤
│  FixedStructures                    │  ← Bounded data structures
│  (FixedVector, FixedQueue, MinHeap)│
└─────────────────────────────────────┘
```

---

## Core Components

### 1. ToroidalSpace

**Location:** `src/cpp_kernel/ToroidalSpace.h`

A 32×32×32 grid with wrap-around boundaries (like Pac-Man).

```cpp
template<int DIM>
class ToroidalSpace {
    static constexpr int CAPACITY = DIM * DIM * DIM;  // 32,768 cells
    
    // Node storage (processes + their state)
    Node nodes[CAPACITY];
    
    // Coordinate mapping
    constexpr int index(int x, int y, int z) const {
        return (x * DIM * DIM) + (y * DIM) + z;
    }
};
```

**Key Features:**
- Compile-time size (32³)
- Wrapping coordinates (toroidal topology)
- O(1) lookup by (x, y, z)
- Contiguous memory layout (cache-friendly)

**Why 32³?**
- Fits in L2 cache on modern CPUs
- Power of 2 for efficient modulo arithmetic
- Large enough for most simulations
- Small enough to pre-allocate

---

### 2. BoundedAllocator

**Location:** `src/cpp_kernel/Allocator.h`

Custom memory allocator with **fixed-size pools** for O(1) guarantee.

```cpp
class BoundedAllocator {
    // Pre-allocated pools
    char process_pool[327680 * 64];      // 5,120 processes max
    char event_pool[1638400 * 32];       // 51,200 events max
    char edge_pool[163840 * 64];         // 2,560 edges max
    char generic_pool[67108864];         // 64 MB generic
    
    // Free lists for O(1) allocation
    FixedVector<void*, 8192> free_process_list;
    FixedVector<void*, 8192> free_event_list;
    // ...
};
```

**Memory Layout:**
```
Process Pool:   [P][P][P][P]...[P]  (5,120 × 64 bytes)
Event Pool:     [E][E][E][E]...[E]  (51,200 × 32 bytes)
Edge Pool:      [→][→][→]...[→]     (2,560 × 64 bytes)
Generic Pool:   [...............]    (64 MB)
```

**Allocation Strategy:**
1. All memory allocated at startup
2. Objects allocated from free lists (O(1))
3. Deallocated objects return to free lists
4. No `malloc()`/`new` during execution
5. No memory growth ever

---

### 3. FixedStructures

**Location:** `src/cpp_kernel/FixedStructures.h`

Bounded data structures that **never allocate**.

#### FixedVector

```cpp
template<typename T, size_t MAX_SIZE>
class FixedVector {
    T data[MAX_SIZE];
    size_t size_ = 0;
    
    bool push_back(const T& value) {
        if (size_ >= MAX_SIZE) return false;  // Capacity check
        data[size_++] = value;
        return true;
    }
};
```

#### FixedMinHeap

```cpp
template<typename T, size_t MAX_SIZE>
class FixedMinHeap {
    T data[MAX_SIZE];
    size_t size_ = 0;
    
    // Min-heap property: parent ≤ children
    // Used for event priority queue
};
```

**Why Fixed?**
- Prevents `std::bad_alloc` from unbounded growth
- Compile-time capacity checking
- Zero dynamic allocation
- Bounded worst-case behavior

---

### 4. Event Scheduler

**Location:** `src/cpp_kernel/demos/BettiRDLKernel.h`

The main scheduler processes events in **timestamp order**.

```cpp
class BettiRDLKernel {
    // Event queue (bounded)
    FixedMinHeap<RDLEvent, 16384> event_queue;
    
    // Thread-safe injection buffer
    std::mutex event_injection_lock;
    FixedVector<Event, 16384> pending_events;
    
    // Spatial substrate
    ToroidalSpace<32> space;
    
    // Statistics
    uint64_t events_processed = 0;
    uint64_t current_time = 0;
};
```

#### Event Processing Loop

```cpp
int BettiRDLKernel::run(int max_events) {
    int events_in_run = 0;
    
    // 1. Flush pending injections
    flushPendingEvents();
    
    // 2. Process events from priority queue
    while (!event_queue.empty() && events_in_run < max_events) {
        RDLEvent evt = event_queue.pop();
        
        // 3. Deliver to process
        Process* proc = space.getProcess(evt.dst_x, evt.dst_y, evt.dst_z);
        if (proc) {
            proc->state += evt.payload;  // Accumulate
            
            // 4. Process may generate new events
            if (should_cascade(evt)) {
                event_queue.push(new_event);
            }
        }
        
        current_time = evt.timestamp;
        events_in_run++;
        events_processed++;
    }
    
    return events_in_run;
}
```

---

## Memory Architecture

### Startup Allocation

```
[Initialization Phase]
1. Allocate ToroidalSpace (32³ × sizeof(Node))
2. Allocate BoundedAllocator pools (~150 MB)
3. Initialize FixedStructures (bounded sizes)
4. DONE - Memory now constant forever
```

### Runtime Behavior

```
[Steady State - No More Allocation]
- Events come from injection or cascades
- Events added to fixed-size priority queue
- If queue full, injection fails gracefully
- Events processed → removed from queue
- Processes reuse grid cells
- Zero dynamic allocation
- Memory graph: FLAT LINE
```

### Memory Budget Breakdown

| Component | Size | Purpose |
|-----------|------|---------|
| ToroidalSpace | ~2 MB | 32³ grid cells |
| Process Pool | ~20 MB | 5,120 max processes |
| Event Pool | ~52 MB | 51,200 max events |
| Edge Pool | ~10 MB | 2,560 max edges |
| Generic Pool | 64 MB | Misc allocations |
| **Total** | **~150 MB** | **Fixed at startup** |

**Key Insight:** This is the TOTAL memory regardless of workload!

---

## Event Processing

### Event Structure

```cpp
struct RDLEvent {
    uint64_t timestamp;    // Logical time
    int dst_x, dst_y, dst_z;  // Destination coordinates
    int src_x, src_y, src_z;  // Source (for routing)
    int payload;              // Value to deliver
    
    // Deterministic ordering
    bool operator<(const RDLEvent& other) const {
        if (timestamp != other.timestamp) 
            return timestamp < other.timestamp;
        if (dst_x != other.dst_x) 
            return dst_x < other.dst_x;
        if (dst_y != other.dst_y) 
            return dst_y < other.dst_y;
        return dst_z < other.dst_z;
    }
};
```

### Event Lifecycle

```
┌─────────────┐
│  INJECT     │  ← External thread calls injectEvent()
│  (Thread-   │
│   safe)     │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│  PENDING    │  ← Stored in FixedVector (mutex-protected)
│  BUFFER     │
└──────┬──────┘
       │ flushPendingEvents()
       ↓
┌─────────────┐
│  PRIORITY   │  ← MinHeap sorted by timestamp
│  QUEUE      │
└──────┬──────┘
       │ run()
       ↓
┌─────────────┐
│  PROCESS    │  ← Delivered to process, state updated
│  (Execute)  │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│  CASCADE    │  ← May generate new events (recursive)
│  (Optional) │
└─────────────┘
```

### Deterministic Execution

Events are processed in **canonical order**:
1. First by **timestamp** (logical time)
2. Then by **dst_x** (spatial tiebreaking)
3. Then by **dst_y**
4. Then by **dst_z**

**Why This Matters:**
- Same inputs → same outputs (reproducible)
- Debugging is deterministic
- Testing is reliable
- Distributed coordination possible (future)

---

## Thread Safety

### Design Philosophy

**Single-threaded scheduler** + **thread-safe injection**

```
Thread 1 (Scheduler)          Thread 2 (Injector)
────────────────────          ───────────────────
run() {                       injectEvent() {
  flushPending()                lock(mutex)
  while (...) {                 pending.push_back(evt)
    process_event()             unlock(mutex)
  }                           }
}
```

### Critical Section

Only the **injection buffer** is protected:

```cpp
void BettiRDLKernel::injectEvent(int x, int y, int z, uint64_t ts, int payload) {
    Event evt{x, y, z, ts, payload};
    
    std::lock_guard<std::mutex> lock(event_injection_lock);
    
    if (!pending_events.push_back(evt)) {
        // Buffer full - event dropped (graceful degradation)
    }
}
```

### Why Not Lock Everything?

**Performance:** Locking the entire scheduler would kill throughput.

**Design:** Single scheduler thread owns event queue and spatial state.

**Parallelism:** Use **multiple kernels** for parallel computation (process isolation).

---

## Performance Characteristics

### Throughput

| Scenario | Events/Second | Notes |
|----------|--------------|-------|
| Single kernel | 16.8M | Single-threaded |
| 4 parallel kernels | 71.4M | Linear scaling |
| 16 parallel kernels | 285.7M | Linear scaling |

### Latency

| Operation | Time | Notes |
|-----------|------|-------|
| Event injection | ~5 ns | Lock + push_back |
| Event processing | ~59 ns | Min-heap pop + deliver |
| Process spawn | ~100 ns | Grid lookup + alloc |

### Scaling

```
Throughput = O(1) per kernel
Parallelism = O(N) kernels
Total = O(N) throughput

Memory = O(1) per kernel
Parallelism = O(N) kernels
Total = O(N) memory
```

**Key Insight:** Single kernel is O(1) memory, multi-kernel is O(N) memory.

---

## Design Decisions

### Why 32³ Grid?

**Pros:**
- Fits in L2 cache (2 MB data)
- Power-of-2 arithmetic (fast modulo)
- Large enough for most simulations
- Small enough to pre-allocate

**Cons:**
- Limited capacity (32,768 cells)
- Not configurable at runtime

**Future:** Support 64³ or 128³ grids (compile-time config).

### Why Bounded Structures?

**Alternative 1:** `std::vector` (unbounded)
- ❌ Can grow without limit → O(N) memory
- ❌ Can throw `std::bad_alloc` → crashes

**Alternative 2:** Fixed-size arrays
- ✅ Bounded memory
- ✅ No allocation
- ✅ Graceful degradation on overflow

**Decision:** Use fixed-size with capacity checks.

### Why Single-Threaded Scheduler?

**Alternative 1:** Lock entire scheduler
- ❌ Kills throughput (contention)
- ❌ Complex locking logic

**Alternative 2:** Lock-free data structures
- ❌ Complex to implement correctly
- ❌ Still contention on shared state

**Alternative 3:** Multiple isolated kernels
- ✅ No contention
- ✅ Linear scaling
- ✅ Simple design

**Decision:** Single-threaded scheduler + parallel kernels.

### Why Process Counter Model?

**Alternative 1:** Arbitrary process logic
- ❌ Requires scripting or JIT
- ❌ Complex to implement
- ❌ Security concerns

**Alternative 2:** Simple accumulator
- ✅ Easy to implement
- ✅ Sufficient for many use cases
- ✅ Deterministic

**Future:** Support Grey language for user-defined logic.

---

## Limitations & Workarounds

### Limitation 1: Grid Size (32³)

**Workaround 1:** Use multiple kernels with spatial partitioning
```python
kernel1 = Kernel()  # Handles region (0-15, *, *)
kernel2 = Kernel()  # Handles region (16-31, *, *)
```

**Workaround 2:** Compile with larger grid (future)
```cmake
cmake -DGRID_SIZE=64  # 64³ grid
```

### Limitation 2: Event Queue Size (16K)

**Workaround:** Process events in smaller batches
```python
# Instead of run(100000)
while True:
    processed = kernel.run(1000)  # Small batches
    if processed == 0: break
```

### Limitation 3: Process Pool (5,120 max)

**Workaround:** Reuse processes instead of spawning new ones
```python
# Bad: Spawn many processes
for i in range(10000):
    kernel.spawn_process(i % 32, 0, 0)  # Will fail!

# Good: Spawn once, reuse
for x in range(32):
    kernel.spawn_process(x, 0, 0)  # Spawn 32 processes
```

### Limitation 4: Single Node (No Distribution)

**Workaround:** Use process-level parallelism
```python
from multiprocessing import Process

def run_kernel(region):
    kernel = Kernel()
    # Run simulation for this region
    
# Parallel kernels
p1 = Process(target=run_kernel, args=(1,))
p2 = Process(target=run_kernel, args=(2,))
p1.start()
p2.start()
```

**Future:** Distributed coordination layer (COG).

---

## Performance Tips

### 1. Batch Event Injection

```python
# Bad: Inject one-by-one
for i in range(1000):
    kernel.inject_event(0, 0, 0, i)
    kernel.run(1)  # Overhead!

# Good: Inject all, then run
for i in range(1000):
    kernel.inject_event(0, 0, 0, i)
kernel.run(1000)  # Single batch
```

### 2. Spatial Locality

```python
# Bad: Random placement
for i in range(100):
    kernel.spawn_process(random.randint(0, 31), ...)

# Good: Clustered placement
for x in range(10):
    for y in range(10):
        kernel.spawn_process(x, y, 0)  # Contiguous
```

### 3. Appropriate Batch Size

```python
# Too small: High overhead
kernel.run(10)

# Too large: Less control
kernel.run(1000000)

# Just right: Balance
kernel.run(1000)
```

### 4. Parallel Kernels

```python
# Single kernel: 16.8M events/sec
kernel = Kernel()

# 4 kernels: 67M events/sec
kernels = [Kernel() for _ in range(4)]
```

---

## Next Steps

- [API Reference](./API_REFERENCE.md) - Complete API documentation
- [Getting Started](./GETTING_STARTED.md) - Quick start guide
- [Validation Results](./VALIDATION_RESULTS.md) - Performance benchmarks
- [Grey Language](./grey_language_spec.md) - High-level DSL

---

**Questions or Feedback?**
- GitHub Issues: [Report bugs](https://github.com/betti-labs/betti-rdl/issues)
- Discussions: [Ask questions](https://github.com/betti-labs/betti-rdl/discussions)

---

**Last Updated:** December 2024  
**Version:** 1.0.0
