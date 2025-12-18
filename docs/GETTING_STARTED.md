---
**Last Updated**: December 18, 2025 at 13:31 UTC
**Status**: Current
---

# Getting Started with Betti-RDL

This guide will help you get up and running with Betti-RDL in under 5 minutes.

## Table of Contents

- [What is Betti-RDL?](#what-is-betti-rdl)
- [Quick Start by Language](#quick-start-by-language)
  - [Rust](#rust-recommended)
  - [Python](#python)
  - [Node.js](#nodejs)
  - [Go](#go)
  - [C++](#c)
- [Your First Program](#your-first-program)
- [Core Concepts](#core-concepts)
- [Common Use Cases](#common-use-cases)
- [Next Steps](#next-steps)

---

## What is Betti-RDL?

**Betti-RDL** is a space-time computation runtime with **O(1) memory guarantee**.

### Key Features

✅ **Constant Memory** - No matter how deep your recursion or how many events you process  
✅ **Thread-Safe** - Inject events from multiple threads safely  
✅ **High Performance** - 16.8M events/second on a single core  
✅ **Deterministic** - Same inputs always produce same outputs  
✅ **Multi-Language** - Use from Rust, Python, Node.js, Go, or C++

### Why Use It?

Perfect for:
- **Agent-based simulations** (logistics, epidemiology, crowds)
- **Neural network simulation** (spiking neural networks)
- **Recursive algorithms** (without stack overflow)
- **Event-driven systems** (IoT, message processing)
- **Grid-based computation** (cellular automata, spatial models)

---

## Quick Start by Language

### Rust (Recommended)

**Prerequisites:** Rust 1.70+, CMake 3.10+, C++ compiler

```bash
# Clone repository
git clone https://github.com/betti-labs/betti-rdl
cd betti-rdl/rust

# Build and run example (CMake builds kernel automatically)
cargo run --example basic
```

**Your first Rust program:**

```rust
use betti_rdl::Kernel;

fn main() {
    let mut kernel = Kernel::new();
    
    // Spawn a process at origin
    kernel.spawn_process(0, 0, 0);
    
    // Send it an event with value 42
    kernel.inject_event(0, 0, 0, 42);
    
    // Process events
    let processed = kernel.run(100);
    
    println!("Processed {} events", processed);
    println!("Current time: {}", kernel.get_current_time());
    println!("Total events: {}", kernel.get_events_processed());
}
```

---

### Python

**Prerequisites:** Python 3.7+, pip, CMake, C++ compiler

```bash
# Build C++ kernel first
cd betti-rdl/src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ../../..

# Install Python binding
cd python
pip install .
```

**Your first Python program:**

```python
import betti_rdl

# Create kernel
kernel = betti_rdl.Kernel()

# Spawn a process at origin
kernel.spawn_process(0, 0, 0)

# Send it an event with value 42
kernel.inject_event(0, 0, 0, 42)

# Process events
processed = kernel.run(100)

print(f"Processed {processed} events")
print(f"Current time: {kernel.get_current_time()}")
print(f"Total events: {kernel.get_events_processed()}")
```

---

### Node.js

**Prerequisites:** Node.js 14+, npm, CMake, C++ compiler

```bash
# Build C++ kernel first
cd betti-rdl/src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ../../..

# Install Node.js binding
cd nodejs
npm install
```

**Your first Node.js program:**

```javascript
const { Kernel } = require('betti-rdl');

// Create kernel
const kernel = new Kernel();

// Spawn a process at origin
kernel.spawn_process(0, 0, 0);

// Send it an event with value 42
kernel.inject_event(0, 0, 0, 42);

// Process events
const processed = kernel.run(100);

console.log(`Processed ${processed} events`);
console.log(`Current time: ${kernel.get_current_time()}`);
console.log(`Total events: ${kernel.get_events_processed()}`);
```

---

### Go

**Prerequisites:** Go 1.16+, CMake, C++ compiler

```bash
# Build C++ kernel first
cd betti-rdl/src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ../../..

# Run Go example
cd go
export LD_LIBRARY_PATH=../build/shared/lib:$LD_LIBRARY_PATH
go run example/main.go
```

**Your first Go program:**

```go
package main

import (
    "fmt"
    bettirdl "github.com/betti-labs/betti-rdl"
)

func main() {
    // Create kernel
    kernel := bettirdl.NewKernel()
    defer kernel.Close()
    
    // Spawn a process at origin
    kernel.SpawnProcess(0, 0, 0)
    
    // Send it an event with value 42
    kernel.InjectEvent(0, 0, 0, 42)
    
    // Process events
    processed := kernel.Run(100)
    
    fmt.Printf("Processed %d events\n", processed)
    fmt.Printf("Current time: %d\n", kernel.CurrentTime())
    fmt.Printf("Total events: %d\n", kernel.EventsProcessed())
}
```

---

### C++

**Prerequisites:** CMake 3.10+, C++17 compiler

```bash
# Build kernel
cd betti-rdl/src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make

# Run tests
ctest --output-on-failure
```

**Your first C++ program:**

```cpp
#include "demos/BettiRDLCompute.h"
#include <iostream>

int main() {
    BettiRDLCompute kernel;
    
    // Spawn a process at origin
    kernel.spawnProcess(0, 0, 0);
    
    // Send it an event with value 42
    kernel.injectEvent(0, 0, 0, 1, 42);  // timestamp=1, value=42
    
    // Process events
    int processed = kernel.run(100);
    
    std::cout << "Processed " << processed << " events\n";
    std::cout << "Current time: " << kernel.getCurrentTime() << "\n";
    std::cout << "Total events: " << kernel.getEventsProcessed() << "\n";
    
    return 0;
}
```

---

## Your First Program

Let's build a simple counter that demonstrates key concepts:

### The Counter Pattern

```python
import betti_rdl

# Create kernel
kernel = betti_rdl.Kernel()

# Spawn 3 processes at different locations
kernel.spawn_process(0, 0, 0)  # Process at origin
kernel.spawn_process(10, 10, 10)  # Process at (10,10,10)
kernel.spawn_process(20, 20, 20)  # Process at (20,20,20)

# Send each process 10 events with value 1
for i in range(10):
    kernel.inject_event(0, 0, 0, 1)
    kernel.inject_event(10, 10, 10, 1)
    kernel.inject_event(20, 20, 20, 1)

# Process all events
processed = kernel.run(100)

print(f"Processed {processed} events")
print(f"Process count: {kernel.get_process_count()}")

# Each process received 10 events, each incrementing its state by 1
# So each process should have state = 10
```

**What's happening:**
1. We create 3 processes at different grid locations
2. Each receives 10 events with value 1
3. Each process increments its internal state by the event value
4. After processing, each process has state = 10

---

## Core Concepts

### 1. The Grid

Betti-RDL operates on a **32×32×32 toroidal grid** (wrap-around boundaries):

```python
# Valid coordinates: 0-31 for x, y, z
kernel.spawn_process(0, 0, 0)     # Origin
kernel.spawn_process(15, 15, 15)  # Center
kernel.spawn_process(31, 31, 31)  # Far corner

# Toroidal wrap-around:
# Coordinates wrap like Pac-Man
# (-1, 0, 0) wraps to (31, 0, 0)
# (32, 0, 0) wraps to (0, 0, 0)
```

### 2. Processes

**Processes** are stateful units that:
- Live at a specific (x, y, z) coordinate
- Maintain an integer state (accumulator)
- Receive events
- Can generate new events when they receive events

```python
kernel.spawn_process(5, 5, 5)  # Create process at (5,5,5)

# Send event with value 10
kernel.inject_event(5, 5, 5, 10)
kernel.run(10)

# Process state is now 10 (accumulated from event)
```

### 3. Events

**Events** are messages that:
- Have a destination (x, y, z)
- Have a payload (integer value)
- Are processed in timestamp order
- Cause processes to update their state

```python
# Basic event injection
kernel.inject_event(x=0, y=0, z=0, value=42)

# Events are processed in order
kernel.inject_event(0, 0, 0, 1)  # Delivered first
kernel.inject_event(0, 0, 0, 2)  # Delivered second
kernel.inject_event(0, 0, 0, 3)  # Delivered third
```

### 4. Event Cascades (The Magic!)

When a process receives an event, it can generate **new events** for itself or neighbors:

```python
# Each event generates the next event (x=0 to x=9)
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 0)  # Seed with x=0

# This will generate events for x=1, 2, 3, ..., 9
# Each event generates the next one
kernel.run(100)

# Result: 10 events processed (x=0 through x=9)
```

This is how you get **recursive computation without stack overflow**!

### 5. O(1) Memory

No matter how many events you inject or how deep the recursion:

```python
# Even with 1,000,000 recursive events, memory stays constant!
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 0)  # Start cascade

# This will process 1,000,000 events
kernel.run(1_000_000)

# Memory usage: EXACTLY THE SAME as startup
# This is the O(1) guarantee!
```

---

## Common Use Cases

### Use Case 1: Agent-Based Simulation

```python
import betti_rdl
import random

kernel = betti_rdl.Kernel()

# Spawn 1000 agents at random locations
for _ in range(1000):
    x = random.randint(0, 31)
    y = random.randint(0, 31)
    z = random.randint(0, 31)
    kernel.spawn_process(x, y, z)

# Each agent receives a "tick" event
for agent in range(1000):
    x = random.randint(0, 31)
    y = random.randint(0, 31)
    z = random.randint(0, 31)
    kernel.inject_event(x, y, z, 1)

# Simulate for 100 time steps
for step in range(100):
    events = kernel.run(1000)
    print(f"Step {step}: {events} events")
```

### Use Case 2: Neural Network Simulation

```python
kernel = betti_rdl.Kernel()

# Create 32³ = 32,768 neurons (full grid)
for x in range(32):
    for y in range(32):
        for z in range(32):
            kernel.spawn_process(x, y, z)

# Inject sensory input (1000 spikes)
for _ in range(1000):
    x = random.randint(0, 31)
    y = random.randint(0, 31)
    z = 0  # Input layer
    kernel.inject_event(x, y, z, 1)

# Process spikes (will cascade through network)
processed = kernel.run(100_000)
print(f"Processed {processed} spikes")
```

### Use Case 3: Recursive Algorithm (No Stack Overflow!)

```python
# Fibonacci-like recursive computation
kernel = betti_rdl.Kernel()
kernel.spawn_process(0, 0, 0)

# Seed with initial value
kernel.inject_event(0, 0, 0, 0)

# Process until cascade completes
# Each event generates 2 more events (exponential growth)
# But memory stays O(1)!
while True:
    events = kernel.run(1000)
    if events == 0:
        break
    print(f"Processed {events} events")

print(f"Total: {kernel.get_events_processed()}")
```

---

## Next Steps

### Tutorials
- [Architecture Guide](./ARCHITECTURE.md) - Understand how it works
- [API Reference](./API_REFERENCE.md) - Complete API docs
- [Grey Language](./grey_language_spec.md) - High-level DSL

### Examples
- [Rust Examples](../rust/examples/) - Working code samples
- [Python Examples](../python/example.py) - Python demos
- [Node.js Examples](../nodejs/example.js) - JavaScript demos

### Advanced Topics
- [Performance Tuning](./ARCHITECTURE.md#performance) - Optimize throughput
- [Capacity Limits](./API_REFERENCE.md#limitations) - Understand bounds
- [Thread Safety](./API_REFERENCE.md#error-handling) - Multi-threaded patterns

### Community
- GitHub Issues: [Report bugs or request features](https://github.com/betti-labs/betti-rdl/issues)
- Discussions: [Ask questions or share ideas](https://github.com/betti-labs/betti-rdl/discussions)

---

## Troubleshooting

### "Shared library not found"

**Solution:** Build the C++ kernel first:

```bash
cd src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### "CMake not found"

**Solution:** Install CMake:

```bash
# Ubuntu/Debian
sudo apt-get install cmake

# macOS
brew install cmake
```

### "C++ compiler not found"

**Solution:** Install a C++ compiler:

```bash
# Ubuntu/Debian
sudo apt-get install g++

# macOS (install Xcode Command Line Tools)
xcode-select --install
```

### Binding fails to build

**Solution:** Ensure environment variables are set:

```bash
export BETTI_RDL_SHARED_LIB_DIR=/path/to/betti-rdl/build/shared/lib
export LD_LIBRARY_PATH=$BETTI_RDL_SHARED_LIB_DIR:$LD_LIBRARY_PATH
```

---

**Welcome to Betti-RDL! Happy computing! 🚀**
