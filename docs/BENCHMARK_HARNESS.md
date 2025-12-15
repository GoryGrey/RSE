# Betti-RDL Benchmark Harness Documentation

## Overview

The Betti-RDL Benchmark Harness is a comprehensive performance validation suite that measures the runtime's capabilities across three critical scenarios:

1. **The Firehose** - Raw event processing throughput
2. **The Deep Dive** - Memory stability under deep recursion
3. **The Swarm** - Parallel scaling across multiple threads

This document explains how to build, run, and interpret the benchmark results.

## Quick Start

### Building the Benchmark Harness

```bash
cd src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Running All Benchmarks

```bash
./benchmark_harness
```

This will run all three scenarios and generate reports in JSON, CSV, and text formats:
- `benchmark_results.json` - Structured JSON output for programmatic analysis
- `benchmark_results.csv` - Spreadsheet-friendly CSV format
- `benchmark_results.txt` - Human-readable text summary

### Running Specific Scenarios

```bash
# Firehose only
./benchmark_harness --firehose

# Deep Dive only
./benchmark_harness --deep-dive

# Swarm only
./benchmark_harness --swarm

# Multiple scenarios
./benchmark_harness --firehose --swarm
```

### Output Formats

```bash
# JSON only (default)
./benchmark_harness --format=json

# CSV only
./benchmark_harness --format=csv

# Text only
./benchmark_harness --format=text

# All formats
./benchmark_harness --format=all
```

## Benchmark Scenarios

### Scenario 1: The Firehose (Throughput)

**Goal**: Measure raw event processing throughput under sustained load.

**What it does**:
- Creates a 4×4×1 cluster of processes
- Injects 1,000,000 events in batches
- Processes events in controlled chunks to maintain queue bounds
- Measures events per second (EPS)

**Key Metrics**:
- **Throughput (EPS)**: Events processed per second
- **Avg Latency (us)**: Average latency per event batch
- **P95/P99 Latency**: Percentile latencies for SLA analysis
- **Memory Delta**: Should remain minimal and flat

**Expected Results**:
- **Excellent**: >4 Million EPS
- **Good**: >1 Million EPS
- **Acceptable**: >500K EPS

**Memory Behavior**:
The Firehose should maintain flat memory usage despite processing millions of events. Any memory growth indicates potential queue buildup or memory leak.

```
Memory (initial):  X bytes
Memory (final):    X bytes (±1% tolerance)
Memory (delta):    ~0 bytes
Memory (stability): ~100%
```

### Scenario 2: The Deep Dive (Memory Stability)

**Goal**: Verify O(1) memory usage during deep recursion chains.

**What it does**:
- Spawns a single process at (0,0,0)
- Injects an initial event with payload=1
- Runs the kernel for 100,000 iterations (~10M event processing steps)
- Monitors memory at 10K-iteration checkpoints
- Verifies zero growth despite deep event chains

**Key Metrics**:
- **Events Processed**: Total event processing operations
- **Memory Initial/Final**: RSS snapshots at start and end
- **Memory Delta**: Should be <5MB for O(1) validation
- **Memory Stability**: Percentage indicating flatness

**Expected Results**:
- **Pass**: Memory delta <5MB
- **Fail**: Memory delta >5MB (indicates unbounded growth)

**Why This Matters**:
Traditional recursive algorithms grow stack memory linearly: O(N) = N * StackFrameSize. This would consume gigabytes for 1M iterations.

Betti-RDL maintains O(1) by replacing processes in a fixed 32³ grid:
- Grid size: 32MB (32,768 cells × 1KB per cell)
- Memory usage: constant regardless of recursion depth
- Stack frames: never grow

**Memory Inspection**:

```
Initial: 150 MB (baseline process memory)
After 10K iters: 150 MB (checkpoint)
After 100K iters: 150 MB (final)
Delta: 0 MB
Stability: 100%
✓ O(1) Memory Validated
```

### Scenario 3: The Swarm (Parallel Scaling)

**Goal**: Measure parallel scaling efficiency across multiple threads.

**What it does**:
- Spawns 4 independent kernel instances (one per thread)
- Each kernel processes 250K events (1M total)
- Events are injected to random locations in the lattice
- Measures aggregate throughput and per-thread latency

**Key Metrics**:
- **Total Events**: Sum of all thread events processed
- **Aggregate Throughput**: Total EPS across all threads
- **Per-Thread Latency**: Average, median, P95, P99
- **Scaling Efficiency**: Actual speedup vs. ideal linear speedup

**Expected Results**:
- **Linear Scaling (4 threads)**: 4× single-thread throughput at 100% efficiency
- **Good Scaling**: 3.2× speedup (80% efficiency) or better
- **Acceptable Scaling**: 2× speedup (50% efficiency) or better

**Scaling Analysis**:

```
Single thread: 270K EPS
4 threads (ideal): 1.08M EPS
4 threads (actual): 900K EPS
Efficiency: 83% (very good)
```

High efficiency indicates that **spatial isolation eliminates lock contention**. Each thread can process events independently without synchronization overhead.

## Memory Telemetry

All benchmarks use the Betti-RDL Memory Telemetry system to track:

### System RSS (Resident Set Size)
- Platform-specific memory measurement:
  - **Linux**: `/proc/self/statm` (page counts × page size)
  - **macOS**: `mach_task_basic_info` (resident_size)
  - **Windows**: `GetProcessMemoryInfo` (WorkingSetSize)

### Memory Snapshots
- Initial RSS: captured before benchmark starts
- Final RSS: captured after benchmark completes
- Peak RSS: maximum RSS reached during execution

### Memory Delta
- Calculated as: `Final RSS - Initial RSS`
- Negative values indicate memory reclamation
- Positive values <5MB are acceptable for O(1) validation

## Interpreting Results

### JSON Output Format

```json
{
  "benchmarks": [
    {
      "scenario": "Firehose (Throughput)",
      "duration_seconds": 2.345,
      "events_processed": 1000000,
      "throughput_eps": 426206.5,
      "latency_avg_us": 2.345,
      "latency_median_us": 2.100,
      "latency_p95_us": 3.500,
      "latency_p99_us": 4.200,
      "latency_min_us": 0.5,
      "latency_max_us": 10.0,
      "memory_initial_bytes": 157286912,
      "memory_final_bytes": 157286912,
      "memory_delta_bytes": 0,
      "memory_stability_percent": 100.0
    }
  ]
}
```

### CSV Output Format

```csv
Scenario,Duration(s),Events,Throughput(EPS),LatencyAvg(us),LatencyMedian(us),LatencyP95(us),LatencyP99(us),MemInitial(B),MemFinal(B),MemDelta(B),MemStability(%)
Firehose (Throughput),2.345000,1000000,426206.500000,2.345000,2.100000,3.500000,4.200000,157286912,157286912,0,100.000000
```

### Key Fields Explained

| Field | Meaning | Interpretation |
|-------|---------|-----------------|
| `throughput_eps` | Events per second | Higher is better. Target: >1M EPS |
| `latency_avg_us` | Average event latency | Lower is better. P95/P99 more important than average |
| `latency_p95_us` | 95th percentile latency | 95% of events complete within this time |
| `latency_p99_us` | 99th percentile latency | 99% of events complete within this time |
| `memory_delta_bytes` | RSS change during test | Should be <5MB for O(1) validation |
| `memory_stability_percent` | 1 - (delta/initial) × 100 | 100% = flat memory, <95% = potential leak |

## Assertions and Validation

The benchmark harness includes automatic assertions that validate key properties:

### Assertion: Throughput Baseline

```
if (eps > 500000) {
    status = "[SUCCESS] >500K EPS achieved"
} else {
    status = "[WARNING] Low throughput detected"
}
```

### Assertion: Memory Flatness (O(1))

```
if (abs(memory_delta) < 5000000) {  // 5MB
    status = "[SUCCESS] O(1) Memory validated! Delta < 5MB"
} else {
    status = "[WARNING] Memory growth detected"
}
```

### Assertion: Parallel Scaling

```
scaling_efficiency = (actual_throughput / single_thread_throughput) / num_threads * 100
if (scaling_efficiency > 80%) {
    status = "[EXCELLENT] Near-linear scaling achieved"
}
```

## Advanced Usage

### Building with Sanitizers (AddressSanitizer/LeakSanitizer)

```bash
cd src/cpp_kernel
mkdir -p build-asan && cd build-asan
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_SANITIZERS=ON
cmake --build . --config Release

# Run with memory safety checks enabled
./mega_demo_asan
./parallel_scaling_test_asan
./betti_rdl_stress_test_asan
```

### Custom Event Counts

To modify benchmark parameters, edit `benchmark_harness.cpp`:

```cpp
// In main() function
results.push_back(runFirehose(2000000));  // 2M events
results.push_back(runDeepDive(200000));   // 200K iterations
results.push_back(runSwarm(8, 500000));   // 8 threads, 500K events each
```

### Profiling with Perf

```bash
# Profile the Firehose scenario
perf record -g ./benchmark_harness --firehose
perf report

# Generate flame graph
perf script > perf.txt
# Use flamegraph.pl to visualize
```

### Comparing Results Across Runs

```bash
# Store baseline
./benchmark_harness --format=json > baseline.json

# Run test
./benchmark_harness --format=json > test.json

# Compare (requires jq)
jq -r '.benchmarks[] | "\(.scenario): \(.throughput_eps) EPS, Δmem: \(.memory_delta_bytes)"' test.json
```

## Performance Tuning

### If Throughput is Low

1. **Check CPU Frequency Scaling**: Disable frequency scaling
   ```bash
   sudo cpupower frequency-set -g performance
   ```

2. **Reduce Background Load**: Close unnecessary applications

3. **Check Batch Size**: Edit `benchmark_harness.cpp`:
   ```cpp
   int batch_size = 500;  // Reduce from 1000
   ```

4. **Profile the Code**: Use `perf record` to identify bottlenecks

### If Memory Grows

1. **Check for Event Queue Buildup**: Increase `run()` processing chunk size
   ```cpp
   (void)kernel.run(batch_size * 20);  // Process more per iteration
   ```

2. **Verify Allocator State**: Check if arena pools are exhausted
   ```cpp
   allocator.printAllStats();
   ```

3. **Run with Sanitizers**: Use ASAN/LSAN to detect leaks
   ```bash
   cmake .. -DENABLE_SANITIZERS=ON
   ```

## CI/CD Integration

The benchmark harness is automatically run in GitHub Actions:

- **Build and Test Job**: Runs on every commit/PR
- **Sanitizer Job**: Validates memory safety with ASAN/LSAN
- **Python Bindings**: Smoke test Python FFI bindings
- **Node.js Bindings**: Smoke test Node.js N-API bindings
- **Benchmark Comparison**: Full suite with artifact uploads

Results are:
- Stored in GitHub Actions artifacts
- Commented on PRs (when applicable)
- Tracked for performance regressions

## References

- [Betti-RDL Architecture](../README.md)
- [Memory Telemetry System](../src/cpp_kernel/Allocator.h)
- [Event-Driven Scheduler](../src/cpp_kernel/demos/BettiRDLKernel.h)
- [CI/CD Workflow](./.github/workflows/ci.yml)

## Troubleshooting

### "benchmark_harness: command not found"

Make sure you built the benchmark harness:
```bash
cmake --build . --config Release
```

### JSON output is malformed

The harness generates valid JSON without external dependencies. If malformed:
1. Check that `benchmark_results.json` doesn't already exist
2. Ensure write permissions in build directory
3. Run again with verbose output: `./benchmark_harness --help`

### Memory measurements are zero

On restricted environments (containers, sandbox):
- Linux: `/proc/self/statm` may not be readable
- macOS: `mach_task_basic_info` may return 0
- Windows: `GetProcessMemoryInfo` requires appropriate privileges

The benchmark will still run but memory metrics may be unavailable.

### Sanitizer builds fail

Ensure you have ASAN/LSAN support:
```bash
# Ubuntu/Debian
sudo apt-get install libasan5

# Verify
clang++ -fsanitize=address -c test.cpp
```

## Contact & Support

For questions about the benchmark harness, refer to:
- GitHub Issues: Report bugs or feature requests
- Pull Requests: Submit improvements
- Documentation: Update this file with new findings
