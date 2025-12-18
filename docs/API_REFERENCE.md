# Betti-RDL API Reference

Complete API documentation for all language bindings: C++, Rust, Python, Node.js, and Go.

## Table of Contents

- [Core Concepts](#core-concepts)
- [C++ API](#cpp-api)
- [C API (FFI Layer)](#c-api-ffi-layer)
- [Rust API](#rust-api)
- [Python API](#python-api)
- [Node.js API](#nodejs-api)
- [Go API](#go-api)
- [Common Patterns](#common-patterns)
- [Error Handling](#error-handling)
- [Performance Tips](#performance-tips)

---

## Core Concepts

### ToroidalSpace
- 32×32×32 3D grid with wrap-around boundaries
- Total capacity: 32,768 cells
- Coordinates: (x, y, z) where 0 ≤ x,y,z < 32

### Process
- Stateful computational unit located at a grid coordinate
- Maintains an internal state (integer counter)
- Can receive and send events
- Each process has a unique Process ID (PID)

### Event
- Message sent between processes
- Contains: timestamp, destination coordinates, payload (integer value)
- Processed in timestamp order (deterministic)

### Run Semantics
- `run(max_events)` processes **at most** max_events in this call
- Returns the actual number of events processed
- Events are processed in deterministic order (timestamp, then spatial tiebreaking)

### O(1) Memory Guarantee
- Total memory usage is bounded at compile-time
- No dynamic allocation during event processing
- Memory remains constant regardless of workload size

---

## C++ API

### BettiRDLKernel

Main event-driven scheduler with RDL (Recursive Delay Learning) support.

```cpp
#include "demos/BettiRDLKernel.h"

BettiRDLKernel kernel;
```

#### Methods

##### `void spawnProcess(int x, int y, int z)`
Creates a new process at the specified coordinates.

**Parameters:**
- `x, y, z`: Grid coordinates (0-31)

**Example:**
```cpp
kernel.spawnProcess(0, 0, 0);  // Spawn at origin
kernel.spawnProcess(15, 15, 15);  // Spawn at center
```

##### `void injectEvent(int dst_x, int dst_y, int dst_z, uint64_t timestamp, int payload)`
Thread-safe event injection from external threads.

**Parameters:**
- `dst_x, dst_y, dst_z`: Destination coordinates
- `timestamp`: Logical time for event delivery
- `payload`: Integer value to deliver

**Thread Safety:** Yes, uses internal mutex

**Example:**
```cpp
// Inject event to deliver at time 10
kernel.injectEvent(0, 0, 0, 10, 42);
```

##### `int run(int max_events)`
Executes the scheduler for at most `max_events`.

**Parameters:**
- `max_events`: Maximum events to process (0 = run until empty)

**Returns:** Actual number of events processed

**Example:**
```cpp
int processed = kernel.run(1000);
std::cout << "Processed " << processed << " events\n";
```

##### `uint64_t getEventsProcessed() const`
Returns cumulative total of events processed across all `run()` calls.

**Returns:** Total event count

##### `uint64_t getCurrentTime() const`
Returns current logical time of the scheduler.

**Returns:** Current timestamp

##### `size_t getProcessCount() const`
Returns number of active processes.

**Returns:** Process count

##### `int getProcessState(int pid) const`
Returns accumulated state for a process.

**Parameters:**
- `pid`: Process ID

**Returns:** Process state value

---

### BettiRDLCompute

Compute-focused variant (no RDL adaptation, pure computation).

Same API as `BettiRDLKernel`, optimized for non-adaptive workloads.

```cpp
#include "demos/BettiRDLCompute.h"

BettiRDLCompute kernel;
// Same API as BettiRDLKernel
```

---

## C API (FFI Layer)

C99-compatible interface for language bindings.

```c
#include "betti_rdl_c_api.h"
```

### Functions

```c
// Create/Destroy
BettiRDLCompute* betti_rdl_create();
void betti_rdl_destroy(BettiRDLCompute* kernel);

// Process Management
void betti_rdl_spawn_process(BettiRDLCompute* kernel, int x, int y, int z);

// Event Injection
void betti_rdl_inject_event(BettiRDLCompute* kernel, int x, int y, int z, int value);

// Execution
int betti_rdl_run(BettiRDLCompute* kernel, int max_events);

// Query
uint64_t betti_rdl_get_events_processed(const BettiRDLCompute* kernel);
uint64_t betti_rdl_get_current_time(const BettiRDLCompute* kernel);
size_t betti_rdl_get_process_count(const BettiRDLCompute* kernel);
int betti_rdl_get_process_state(const BettiRDLCompute* kernel, int pid);
```

---

## Rust API

### Installation

```toml
[dependencies]
betti-rdl = "1.0.0"
```

The Rust binding automatically builds the C++ kernel via `build.rs` using CMake.

### Kernel

```rust
use betti_rdl::Kernel;

let mut kernel = Kernel::new();
```

#### Methods

##### `fn new() -> Kernel`
Creates a new kernel instance.

**Panics:** If kernel creation fails (null pointer from C API)

##### `fn spawn_process(&mut self, x: i32, y: i32, z: i32)`
Spawns a process at coordinates.

**Example:**
```rust
kernel.spawn_process(0, 0, 0);
```

##### `fn inject_event(&mut self, x: i32, y: i32, z: i32, value: i32)`
Injects an event (thread-safe).

**Example:**
```rust
kernel.inject_event(5, 5, 5, 42);
```

##### `fn run(&mut self, max_events: i32) -> i32`
Executes scheduler.

**Returns:** Number of events processed

**Example:**
```rust
let processed = kernel.run(1000);
println!("Processed {} events", processed);
```

##### `fn get_events_processed(&self) -> u64`
Returns total events processed.

##### `fn get_current_time(&self) -> u64`
Returns current logical time.

##### `fn get_process_count(&self) -> usize`
Returns active process count.

##### `fn get_process_state(&self, pid: i32) -> i32`
Returns process state.

---

## Python API

### Installation

```bash
# Install prerequisites
pip install pybind11

# Build and install
cd python
pip install .
```

### Kernel

```python
import betti_rdl

kernel = betti_rdl.Kernel()
```

#### Methods

##### `Kernel()` → Kernel
Constructor, creates new kernel instance.

##### `spawn_process(x: int, y: int, z: int) -> None`
Spawns a process.

**Example:**
```python
kernel.spawn_process(0, 0, 0)
```

##### `inject_event(x: int, y: int, z: int, value: int) -> None`
Injects an event.

**Example:**
```python
kernel.inject_event(10, 10, 10, 42)
```

##### `run(max_events: int) -> int`
Executes scheduler.

**Returns:** Events processed

**Example:**
```python
processed = kernel.run(1000)
print(f"Processed {processed} events")
```

##### `get_events_processed() -> int`
Returns total events processed.

##### `get_current_time() -> int`
Returns current time.

##### `get_process_count() -> int`
Returns process count.

##### `get_process_state(pid: int) -> int`
Returns process state.

---

## Node.js API

### Installation

```bash
cd nodejs
npm install
```

### Kernel

```javascript
const { Kernel } = require('betti-rdl');

const kernel = new Kernel();
```

#### Methods

##### `new Kernel()`
Constructor, creates new kernel instance.

##### `spawn_process(x, y, z)`
Spawns a process at coordinates.

**Example:**
```javascript
kernel.spawn_process(0, 0, 0);
```

##### `inject_event(x, y, z, value)`
Injects an event.

**Example:**
```javascript
kernel.inject_event(5, 5, 5, 42);
```

##### `run(maxEvents)`
Executes scheduler.

**Returns:** Number of events processed

**Example:**
```javascript
const processed = kernel.run(1000);
console.log(`Processed ${processed} events`);
```

##### `get_events_processed()`
Returns total events processed.

##### `get_current_time()`
Returns current time.

##### `get_process_count()`
Returns process count.

##### `get_process_state(pid)`
Returns process state.

---

## Go API

### Installation

```bash
cd go
go get github.com/betti-labs/betti-rdl
```

### Kernel

```go
import "github.com/betti-labs/betti-rdl"

kernel := bettirdl.NewKernel()
defer kernel.Close()
```

#### Methods

##### `func NewKernel() *Kernel`
Creates new kernel instance.

##### `func (k *Kernel) Close()`
Destroys kernel and frees resources. **Always call this!**

##### `func (k *Kernel) SpawnProcess(x, y, z int)`
Spawns a process.

**Example:**
```go
kernel.SpawnProcess(0, 0, 0)
```

##### `func (k *Kernel) InjectEvent(x, y, z, value int)`
Injects an event.

**Example:**
```go
kernel.InjectEvent(5, 5, 5, 42)
```

##### `func (k *Kernel) Run(maxEvents int) int`
Executes scheduler.

**Returns:** Events processed

**Example:**
```go
processed := kernel.Run(1000)
fmt.Printf("Processed %d events\n", processed)
```

##### `func (k *Kernel) EventsProcessed() uint64`
Returns total events processed.

##### `func (k *Kernel) CurrentTime() uint64`
Returns current time.

##### `func (k *Kernel) ProcessCount() int`
Returns process count.

##### `func (k *Kernel) ProcessState(pid int) int`
Returns process state.

---

## Common Patterns

### 1. Spawning and Injecting Events

```python
# Python
kernel = betti_rdl.Kernel()
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 1)
kernel.run(100)
```

```rust
// Rust
let mut kernel = Kernel::new();
kernel.spawn_process(0, 0, 0);
kernel.inject_event(0, 0, 0, 1);
kernel.run(100);
```

### 2. Process Counter Pattern

Each process maintains a counter that increments with each received event:

```python
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 10)  # Increment by 10
kernel.inject_event(0, 0, 0, 5)   # Increment by 5
kernel.run(100)
# Process state will be 15
```

### 3. Event Cascades (Recursive Events)

Processes can generate new events when they receive events:

```python
# Each event generates the next event in sequence
for i in range(10):
    kernel.inject_event(0, 0, 0, i)

kernel.run(100)  # Will process all 100 events in cascade
```

### 4. Multi-threaded Event Injection

```cpp
// C++: Thread-safe injection
std::thread t1([&]() {
    for (int i = 0; i < 1000; i++) {
        kernel.injectEvent(0, 0, 0, 1, i);
    }
});

std::thread t2([&]() {
    for (int i = 0; i < 1000; i++) {
        kernel.injectEvent(1, 1, 1, 1, i);
    }
});

// Main thread runs scheduler
while (kernel.run(100) > 0) {
    // Process events as they arrive
}

t1.join();
t2.join();
```

### 5. Stepping Through Events

```javascript
// Node.js: Process events in small batches
while (true) {
    const processed = kernel.run(10);  // Process 10 events at a time
    if (processed === 0) break;
    
    console.log(`Time: ${kernel.get_current_time()}, Events: ${processed}`);
}
```

---

## Error Handling

### C++
- Assertions for contract violations (debug builds)
- Bounded data structures prevent overflow
- No exceptions thrown

### Rust
- Panics on kernel creation failure
- Safe FFI wrappers ensure memory safety

### Python
- Raises Python exceptions on errors
- Memory management handled by pybind11

### Node.js
- Throws JavaScript Error on failures
- Addon handles C++ exceptions

### Go
- Panics on critical errors
- Use `defer kernel.Close()` for cleanup

---

## Performance Tips

### 1. Batch Event Injection
Inject multiple events before calling `run()` to reduce overhead.

### 2. Appropriate Batch Size
Use `run(1000)` instead of `run(1)` for better throughput.

### 3. Spatial Locality
Place related processes near each other in the grid for better cache performance.

### 4. Avoid Excessive Process Creation
Reuse processes when possible (bounded at compile-time anyway).

### 5. Profile Before Optimizing
Measure throughput with `getEventsProcessed()` and `getCurrentTime()`.

---

## Limitations

### Hard Limits
- **Grid Size:** 32×32×32 (32,768 cells)
- **Event Queue:** 8,192 events (bounded)
- **Process Pool:** 5,120 processes maximum
- **Single Node:** No multi-node coordination (yet)

### Design Constraints
- All memory pre-allocated (O(1) guarantee)
- Single-threaded scheduler (use multiple kernels for parallelism)
- Deterministic event ordering (not wall-clock based)

---

## Next Steps

- [Architecture Guide](./ARCHITECTURE.md) - Understand the internal design
- [Getting Started](./GETTING_STARTED.md) - Quick start tutorials
- [Examples](../rust/examples/) - Working code samples
- [Grey Language](./grey_language_spec.md) - High-level DSL for Betti-RDL

---

**Version:** 1.0.0  
**Last Updated:** December 2024
