# Troubleshooting Guide

Common issues and solutions for Betti-RDL.

## Table of Contents

- [Build Issues](#build-issues)
- [Runtime Issues](#runtime-issues)
- [Binding Issues](#binding-issues)
- [Performance Issues](#performance-issues)
- [Grey Compiler Issues](#grey-compiler-issues)
- [Getting Help](#getting-help)

---

## Build Issues

### "CMake not found"

**Error:**
```
bash: cmake: command not found
```

**Solution:**

Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install cmake ninja-build
```

macOS:
```bash
brew install cmake ninja
```

Windows:
- Download CMake from https://cmake.org/download/
- Add to PATH

---

### "C++ compiler not found"

**Error:**
```
CMake Error: CMAKE_CXX_COMPILER not set
```

**Solution:**

Ubuntu/Debian:
```bash
sudo apt-get install g++ build-essential
```

macOS:
```bash
xcode-select --install
```

Windows:
- Install Visual Studio 2019+ with C++ workload
- Or install MinGW-w64

---

### "libbetti_rdl_c.so not found"

**Error:**
```
error while loading shared library: libbetti_rdl_c.so: cannot open shared object file
```

**Solution:**

1. Build the C++ kernel first:
```bash
cd src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

2. Set library path:
```bash
export LD_LIBRARY_PATH=/path/to/betti-rdl/build/shared/lib:$LD_LIBRARY_PATH
```

3. Or copy library to system path:
```bash
sudo cp libbetti_rdl_c.so /usr/local/lib/
sudo ldconfig
```

---

### "undefined reference to `atomic`"

**Error:**
```
undefined reference to `__atomic_load_8'
```

**Solution:**

Link with libatomic explicitly:

```cmake
target_link_libraries(your_target PRIVATE atomic)
```

This is already handled in the CMakeLists.txt, but if you're building manually, add `-latomic` to linker flags.

---

## Runtime Issues

### "Assertion failed: Event queue full"

**Error:**
```
Assertion failed: (event_queue.size() < MAX_SIZE)
```

**Cause:** Too many events injected without processing.

**Solution 1:** Process events more frequently
```python
# Bad: Inject 100k events, then process
for i in range(100000):
    kernel.inject_event(0, 0, 0, i)
kernel.run(100000)  # Queue overflow!

# Good: Process in batches
for i in range(100000):
    kernel.inject_event(0, 0, 0, i)
    if i % 1000 == 0:
        kernel.run(1000)  # Drain periodically
```

**Solution 2:** Reduce event cascade depth
```python
# Limit recursion depth in your process logic
# Each event should generate fewer new events
```

---

### "Segmentation fault on kernel creation"

**Error:**
```
Segmentation fault (core dumped)
```

**Cause:** Kernel creation failed, returned null pointer.

**Solution:**

Check available memory:
```bash
free -h
```

Betti-RDL requires ~150 MB per kernel. Ensure sufficient memory available.

---

### "Events not processing"

**Symptom:** `run()` returns 0 immediately.

**Cause:** No processes spawned or no events injected.

**Solution:**

1. Spawn at least one process:
```python
kernel.spawn_process(0, 0, 0)
```

2. Inject events:
```python
kernel.inject_event(0, 0, 0, 1)
```

3. Then run:
```python
processed = kernel.run(100)
assert processed > 0
```

---

## Binding Issues

### Python: "externally-managed-environment"

**Error:**
```
error: externally-managed-environment
```

**Cause:** PEP 668 prevents system-wide package installation.

**Solution 1:** Use virtual environment (recommended)
```bash
cd python
python3 -m venv venv
source venv/bin/activate
pip install .
```

**Solution 2:** User install
```bash
pip install --user .
```

**Solution 3:** Override (not recommended)
```bash
pip install --break-system-packages .
```

---

### Python: "pybind11 not found"

**Error:**
```
ModuleNotFoundError: No module named 'pybind11'
```

**Solution:**
```bash
pip install pybind11
```

---

### Node.js: "binding.gyp not found"

**Error:**
```
gyp ERR! find failed
```

**Solution:**

1. Ensure `configure_binding.sh` ran:
```bash
cd nodejs
bash configure_binding.sh
```

2. Verify `binding.gyp` exists:
```bash
ls -l binding.gyp
```

3. Rebuild:
```bash
npm install
```

---

### Go: "could not determine kind of name"

**Error:**
```
could not determine kind of name for C.betti_rdl_create
```

**Cause:** CGo compilation issue with C API.

**Solution:**

Ensure library path is set:
```bash
export CGO_LDFLAGS="-L/path/to/build/shared/lib -lbetti_rdl_c"
export LD_LIBRARY_PATH=/path/to/build/shared/lib:$LD_LIBRARY_PATH
go run example/main.go
```

---

### Rust: "cmake not found" during cargo build

**Error:**
```
error: failed to run custom build command for `betti-rdl`
  cmake: command not found
```

**Solution:**

Install CMake (Rust binding uses it to build C++ kernel):
```bash
sudo apt-get install cmake
```

Then rebuild:
```bash
cd rust
cargo clean
cargo build
```

---

## Performance Issues

### "Throughput lower than expected"

**Expected:** 16.8M events/sec  
**Actual:** < 5M events/sec

**Diagnosis:**

1. Check if running in debug mode:
```bash
# Release mode is 3-5x faster
cmake -DCMAKE_BUILD_TYPE=Release ..
```

2. Check event batch size:
```python
# Too small: high overhead
kernel.run(10)  # Bad

# Better: larger batches
kernel.run(1000)  # Good
```

3. Check for excessive logging:
```bash
# Disable verbose logging in production builds
```

4. Profile with perf:
```bash
perf record -g ./stress_test
perf report
```

---

### "High memory usage"

**Expected:** ~150 MB per kernel  
**Actual:** > 500 MB per kernel

**Diagnosis:**

1. Check for memory leaks in bindings:
```python
import tracemalloc
tracemalloc.start()

# Run your code
kernel = betti_rdl.Kernel()
# ...

snapshot = tracemalloc.take_snapshot()
top_stats = snapshot.statistics('lineno')
for stat in top_stats[:10]:
    print(stat)
```

2. Check if running multiple kernels:
```python
# Each kernel uses ~150 MB
kernels = [Kernel() for _ in range(10)]  # = 1.5 GB total
```

3. Monitor with valgrind:
```bash
valgrind --leak-check=full ./your_program
```

---

### "Slow event injection"

**Symptom:** `inject_event()` takes > 100 ns per call.

**Diagnosis:**

1. Check if mutex contention (multiple threads):
```python
# Multiple threads injecting → lock contention
# Solution: Use fewer injector threads or batch injections
```

2. Batch injections:
```python
# Bad: Inject + run repeatedly
for i in range(1000):
    kernel.inject_event(0, 0, 0, i)
    kernel.run(1)  # Overhead!

# Good: Batch inject, then run
for i in range(1000):
    kernel.inject_event(0, 0, 0, i)
kernel.run(1000)  # Single run
```

---

## Grey Compiler Issues

### "Expected parameter name"

**Error:**
```
Error: Expected parameter name
```

**Cause:** Parser issue with Grey language syntax (known issue).

**Status:** Under investigation (Week 2 of Phase 3).

**Workaround:**

Use direct C++/Rust API instead of Grey language:
```rust
// Instead of .grey file, use Rust API directly
let mut kernel = Kernel::new();
kernel.spawn_process(0, 0, 0);
kernel.run(1000);
```

---

### "greyc command not found"

**Error:**
```
bash: greyc: command not found
```

**Solution:**

Build Grey compiler:
```bash
cd grey_compiler
cargo build --release
export PATH=$PATH:$(pwd)/target/release
greyc --version
```

---

### "SIR demo fails to compile"

**Error:**
```
Parse error in demos/sir_epidemic.grey
```

**Status:** Known parser issue (being fixed in Phase 3 Week 2).

**Workaround:**

Use reference C++ implementation:
```bash
cd src/cpp_kernel/build
./grey_sir_reference
```

---

## Getting Help

### Before Asking

1. **Check this guide** - Most issues are documented here
2. **Search issues** - https://github.com/betti-labs/betti-rdl/issues
3. **Read docs** - [Getting Started](./GETTING_STARTED.md), [API Reference](./API_REFERENCE.md)

### How to Ask

Include:
- **OS and version** (`uname -a` or `ver`)
- **Compiler version** (`g++ --version`, `clang --version`)
- **CMake version** (`cmake --version`)
- **Language binding** (Rust, Python, Node.js, Go, C++)
- **Error message** (full stacktrace)
- **Minimal reproduction** (code that reproduces the issue)

### Where to Ask

- **GitHub Issues:** https://github.com/betti-labs/betti-rdl/issues  
  For bugs, crashes, and build problems

- **GitHub Discussions:** https://github.com/betti-labs/betti-rdl/discussions  
  For questions, ideas, and general help

- **Stack Overflow:** Tag with `betti-rdl`  
  For programming questions and best practices

---

## Common Environment Setup

### Linux (Ubuntu/Debian)

```bash
# Install all prerequisites
sudo apt-get update
sudo apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    build-essential \
    python3 \
    python3-pip \
    python3-venv \
    nodejs \
    npm \
    golang-go

# Build C++ kernel
cd betti-rdl/src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install Python binding
cd ../../../python
python3 -m venv venv
source venv/bin/activate
pip install pybind11
pip install .

# Install Node.js binding
cd ../nodejs
bash configure_binding.sh
npm install

# Install Go binding
cd ../go
export LD_LIBRARY_PATH=../build/shared/lib:$LD_LIBRARY_PATH
go run example/main.go

# Install Rust binding
cd ../rust
cargo build --release
cargo run --example basic
```

### macOS

```bash
# Install Homebrew (if not installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install prerequisites
brew install cmake ninja go node rust

# Build C++ kernel
cd betti-rdl/src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)

# Follow same binding installation as Linux above
```

### Windows

```powershell
# Install Visual Studio 2019+ with C++ workload
# Install CMake from https://cmake.org/download/

# Build C++ kernel (PowerShell)
cd betti-rdl\src\cpp_kernel
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Python
pip install pybind11
cd ..\..\..\python
pip install .

# Node.js
cd ..\nodejs
npm install

# Rust
cd ..\rust
cargo build --release
```

---

## Debug vs Release Builds

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**Pros:**
- Assertions enabled
- Debug symbols included
- Easier to debug

**Cons:**
- 3-5x slower
- Larger binary

**When to use:** Development, debugging, testing

### Release Build

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**Pros:**
- Full optimizations (-O3)
- 3-5x faster
- Smaller binary

**Cons:**
- No assertions
- Harder to debug

**When to use:** Production, benchmarking

---

## Still Stuck?

If you've tried everything in this guide and still have issues:

1. Create a minimal reproduction case
2. Open a GitHub issue with full details
3. We'll help you resolve it!

**GitHub Issues:** https://github.com/betti-labs/betti-rdl/issues/new

---

**Last Updated:** December 2024  
**Version:** 1.0.0
