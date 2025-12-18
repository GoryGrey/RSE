# Phase 4: Optimization - Design Document

**Goal**: Maximize throughput and minimize latency through parallel execution and adaptive algorithms.

**Target**: 50M+ events/sec (3× single-torus performance)

---

## Overview

Phase 4 focuses on **performance optimization** without changing the core architecture. We'll implement:

1. **Parallel Torus Execution** - Run three tori in separate threads
2. **Adaptive Braid Interval** - Dynamically adjust based on workload
3. **Lock-Free Projection Exchange** - Eliminate synchronization overhead
4. **Performance Monitoring** - Real-time metrics and bottleneck detection

---

## Current Performance

### Single-Torus Baseline
- **Throughput**: 16.8M events/sec
- **Latency**: ~60ns per event
- **Memory**: 150MB per kernel

### Braided-Torus (Phase 3)
- **Throughput**: ~16M events/sec (similar to single-torus)
- **Overhead**: <2% from braid coordination
- **Bottleneck**: Sequential execution of three tori

**Why no speedup yet?** The three tori run sequentially in the same thread:
```cpp
torus_a_->tick();  // Sequential
torus_b_->tick();  // Sequential
torus_c_->tick();  // Sequential
```

---

## Design: Parallel Torus Execution

### Architecture

Each torus runs in its own thread:

```
Thread 1: Torus A → tick() → tick() → tick() → ...
Thread 2: Torus B → tick() → tick() → tick() → ...
Thread 3: Torus C → tick() → tick() → tick() → ...

Main Thread: Braid Coordinator
  ├─ Collect projections (non-blocking)
  ├─ Apply constraints
  └─ Detect failures
```

### Implementation Strategy

1. **Thread Pool**: Create 3 worker threads, one per torus
2. **Work Queue**: Each thread has a lock-free queue of tasks
3. **Synchronization Points**: Only at braid exchanges (every N ticks)
4. **Lock-Free Projections**: Use atomic pointers for projection exchange

### Synchronization Model

```cpp
// Each torus runs independently
while (running) {
    torus->tick();
    
    // Check if braid exchange needed (atomic flag)
    if (should_exchange.load()) {
        // Write projection (lock-free)
        projection_buffer[torus_id].store(torus->extractProjection());
        
        // Wait for coordinator
        barrier.wait();
        
        // Read projections from other tori
        apply_constraints();
        
        // Resume execution
        barrier.wait();
    }
}
```

### Expected Performance

- **Throughput**: 50M+ events/sec (3× single-torus)
- **Latency**: ~60ns per event (unchanged)
- **Scalability**: Near-linear (3 cores → 3× throughput)

---

## Design: Adaptive Braid Interval

### Problem

Fixed braid interval (e.g., 1000 ticks) is suboptimal:
- **High workload**: Too frequent exchanges → overhead
- **Low workload**: Too infrequent exchanges → stale constraints

### Solution: Dynamic Adjustment

Adjust braid interval based on:
1. **Constraint violation rate**: More violations → shorter interval
2. **Event processing rate**: Higher rate → longer interval (amortize overhead)
3. **Failure detection latency**: Balance consistency vs. performance

### Algorithm

```cpp
void adjustBraidInterval() {
    // Measure metrics
    double violation_rate = total_violations / braid_cycles;
    double event_rate = total_events / elapsed_time;
    
    // Compute optimal interval
    if (violation_rate > VIOLATION_THRESHOLD) {
        // Too many violations → exchange more frequently
        braid_interval = max(MIN_INTERVAL, braid_interval * 0.8);
    } else if (violation_rate < VIOLATION_THRESHOLD / 2) {
        // Few violations → exchange less frequently
        braid_interval = min(MAX_INTERVAL, braid_interval * 1.2);
    }
    
    // Clamp to reasonable range
    braid_interval = clamp(braid_interval, 100, 10000);
}
```

### Expected Impact

- **High workload**: Interval increases to 5000+ ticks → lower overhead
- **Low workload**: Interval decreases to 500 ticks → tighter consistency
- **Adaptive**: System self-tunes to workload characteristics

---

## Design: Lock-Free Projection Exchange

### Problem

Current implementation uses locks for projection exchange:
```cpp
std::lock_guard<std::mutex> lock(projection_lock);
last_proj_a_ = proj_a;
```

This creates contention when three tori try to exchange simultaneously.

### Solution: Lock-Free Atomic Pointers

Use `std::atomic<ProjectionV3*>` for lock-free exchange:

```cpp
class LockFreeProjectionBuffer {
private:
    std::atomic<ProjectionV3*> projections[3];
    ProjectionV3 storage[3][2];  // Double buffering
    std::atomic<int> write_index[3];
    
public:
    // Writer (torus thread)
    void write(int torus_id, const ProjectionV3& proj) {
        int idx = write_index[torus_id].load();
        int next_idx = 1 - idx;
        
        // Write to inactive buffer
        storage[torus_id][next_idx] = proj;
        
        // Atomic swap
        projections[torus_id].store(&storage[torus_id][next_idx]);
        write_index[torus_id].store(next_idx);
    }
    
    // Reader (coordinator thread)
    ProjectionV3 read(int torus_id) {
        ProjectionV3* ptr = projections[torus_id].load();
        return *ptr;  // Copy
    }
};
```

### Expected Impact

- **Latency**: Projection exchange drops from ~100μs to ~10μs
- **Throughput**: Eliminates contention bottleneck
- **Scalability**: Enables true parallel execution

---

## Design: Performance Monitoring

### Metrics to Track

1. **Throughput**
   - Events/sec per torus
   - Total system events/sec
   - Utilization per core

2. **Latency**
   - Event processing latency (p50, p95, p99)
   - Braid exchange latency
   - Constraint application latency

3. **Consistency**
   - Violation rate (boundary + global)
   - Corrective events generated
   - Convergence time

4. **Resource Usage**
   - CPU utilization per thread
   - Memory usage (should stay O(1))
   - Cache hit rate

### Implementation

```cpp
struct PerformanceMetrics {
    // Throughput
    std::atomic<uint64_t> events_processed[3];
    std::atomic<uint64_t> total_ticks[3];
    
    // Latency (ring buffer for histogram)
    std::array<std::atomic<uint64_t>, 100> latency_histogram;
    
    // Consistency
    std::atomic<uint64_t> boundary_violations;
    std::atomic<uint64_t> global_violations;
    std::atomic<uint64_t> corrective_events;
    
    // Resource usage
    std::atomic<double> cpu_utilization[3];
    std::atomic<size_t> memory_usage;
    
    void print() {
        double total_throughput = 0;
        for (int i = 0; i < 3; i++) {
            double torus_throughput = events_processed[i].load() / elapsed_time;
            total_throughput += torus_throughput;
            std::cout << "Torus " << i << ": " << torus_throughput << " events/sec" << std::endl;
        }
        std::cout << "Total: " << total_throughput << " events/sec" << std::endl;
    }
};
```

---

## Implementation Plan

### Week 1: Parallel Execution
- **Day 1-2**: Thread pool and work queue
- **Day 3-4**: Synchronization barriers
- **Day 5**: Testing and debugging

### Week 2: Lock-Free Projections
- **Day 1-2**: Lock-free buffer implementation
- **Day 3-4**: Integration with parallel execution
- **Day 5**: Performance testing

### Week 3: Adaptive Braid Interval
- **Day 1-2**: Metrics collection
- **Day 3-4**: Adaptive algorithm
- **Day 5**: Tuning and validation

### Week 4: Performance Monitoring
- **Day 1-2**: Metrics infrastructure
- **Day 3-4**: Visualization and logging
- **Day 5**: Final benchmarking

---

## Success Criteria

### Performance
- ✅ **Throughput ≥ 50M events/sec** (3× single-torus)
- ✅ **Latency < 100ns** per event (p95)
- ✅ **CPU utilization > 90%** per core

### Consistency
- ✅ **Violation rate < 1%** of braid exchanges
- ✅ **Convergence time < 10 braid cycles**

### Scalability
- ✅ **Linear scaling** (3 cores → 3× throughput)
- ✅ **Memory stays O(1)** (no growth over time)

---

## Risks and Mitigation

### Risk 1: Thread Contention
**Impact**: High  
**Probability**: Medium  
**Mitigation**: Lock-free data structures, minimize shared state

### Risk 2: Cache Coherence Overhead
**Impact**: Medium  
**Probability**: High  
**Mitigation**: Align data structures to cache lines, minimize false sharing

### Risk 3: Adaptive Algorithm Instability
**Impact**: Medium  
**Probability**: Low  
**Mitigation**: Conservative tuning, hysteresis in adjustment

### Risk 4: Debugging Complexity
**Impact**: High  
**Probability**: High  
**Mitigation**: Extensive logging, deterministic replay, unit tests

---

## Future Optimizations (Phase 5+)

1. **SIMD Vectorization**: Process multiple events in parallel
2. **GPU Acceleration**: Offload constraint checking to GPU
3. **NUMA Awareness**: Pin threads to specific cores
4. **Zero-Copy Projections**: Avoid memcpy overhead
5. **Incremental Projections**: Only send changes, not full state

---

## Conclusion

Phase 4 will transform the braided-torus system from a **proof-of-concept** to a **high-performance runtime** capable of 50M+ events/sec. The key innovations are:

1. **Parallel execution** - Unlock multi-core performance
2. **Adaptive algorithms** - Self-tune to workload
3. **Lock-free coordination** - Eliminate contention
4. **Performance monitoring** - Visibility into bottlenecks

This sets the foundation for Phase 5 (distributed mode) and beyond (full OS).

---

**Phase 4 is the performance breakthrough that makes the braided system competitive with state-of-the-art runtimes.**
