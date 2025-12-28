---
**Last Updated**: December 18, 2025 at 13:31 UTC
**Status**: Current
---

# Multi-Language Binding Validation

The Betti-RDL runtime provides bindings for multiple programming languages (Python, Node.js, Go, and Rust). This document describes the validation system that ensures consistent behavior across all language bindings.

## Overview

The binding matrix validation system ensures that:

1. **Consistent Results**: All language bindings produce identical results for the same workload
2. **Shared Library**: All bindings use the same compiled C++ kernel
3. **API Compatibility**: The C API remains stable across all bindings
4. **Regression Detection**: Any change that breaks cross-language compatibility is detected immediately

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    scripts/run_binding_matrix.sh             │
│                    (Orchestration Layer)                    │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────┐ │
│  │   Python    │  │   Node.js   │  │     Go      │  │Rust │ │
│  │  bindings   │  │  bindings   │  │  bindings   │  │bind │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──┬──┘ │
│         │                 │                 │           │   │
│         └─────────────────┼─────────────────┼───────────┘   │
│                           │                 │               │
│                   ┌───────▼────────┐  ┌────▼─────────────┐ │
│                   │  Shared Build  │  │  Shared Build    │ │
│                   │    Directory   │  │    Directory     │ │
│                   └───────┬────────┘  └────────┬─────────┘ │
│                           │                    │           │
│                           └────────────────────┘           │
│                                    │                        │
│                           ┌─────────▼─────────┐           │
│                           │  betti_rdl_c.so   │           │
│                           │  (C++ Kernel)     │           │
│                           └───────────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

## Running the Binding Matrix Test

### Local Development

Run the complete binding matrix validation:

```bash
# From the project root
./scripts/run_binding_matrix.sh
```

This script will:

1. **Build the C++ kernel** once and install it to `build/shared/`
2. **Configure each binding** to use the shared library
3. **Run smoke tests** for each language
4. **Compare telemetry** across all languages
5. **Report results** with detailed output

### Expected Output

```
🔧 Betti-RDL Binding Matrix Test
======================================

Step 1: Building C++ kernel...
  ✅ C++ kernel built successfully

Metrics captured per run; see logs.

  BETTI_RDL_SHARED_LIB=/home/engine/project/build/shared/lib/libbetti_rdl_c.so
  BETTI_RDL_INCLUDE_DIR=/home/engine/project/src/cpp_kernel

Step 3: Testing Python Binding...
  ✅ Python test passed

Step 4: Testing Node.js Binding...
  ✅ Node.js test passed

Step 5: Testing Go Binding...
  ✅ Go test passed

Step 6: Testing Rust Binding...
  ✅ Rust test passed

Step 7: Cross-Language Telemetry Verification...
  Python telemetry: 500,500,1234
  Node.js telemetry: 500,500,1234
  Go telemetry: 500,500,1234
  Rust telemetry: 500,500,1234
  ✅ Cross-language telemetry validation passed

🏁 Binding Matrix Test Results
=================================

Individual Test Results:
  python: ✅ PASS
  nodejs: ✅ PASS
  go: ✅ PASS
  rust: ✅ PASS

Summary: 5/5 tests passed
🎉 ALL TESTS PASSED! Binding matrix is healthy.
```

## Individual Language Testing

### Python

```bash
cd python
python -c "
from betti_rdl_bindings import Kernel
kernel = Kernel()
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 1)
Metrics captured per run; see logs.

print(f'Processed {events} events')
"
```

### Node.js

```bash
cd nodejs
node -e "
const { Kernel } = require('./index.js');
const kernel = new Kernel();
kernel.spawnProcess(0, 0, 0);
kernel.injectEvent(0, 0, 0, 1);
Metrics captured per run; see logs.

console.log(\`Processed \${events} events\`);
"
```

### Go

```bash
cd go
go run example/main.go
```

### Rust

```bash
cd rust
cargo run --example basic
```

## Continuous Integration

The binding matrix test runs automatically in CI on every push to ensure regressions are caught immediately.

### GitHub Actions Workflow

The `bindings-matrix` job in `.github/workflows/ci.yml` runs:

1. **After C++ build succeeds** (`needs: [cpp]`)
2. **Installs all language dependencies**
3. **Executes the binding matrix test**
4. **Fails the build** if any language binding fails

### CI Failure Scenarios

- **Build Failure**: C++ kernel fails to compile
- **Library Not Found**: Shared library path issues
- **Language Binding Failure**: Individual language test failure
- **Telemetry Mismatch**: Cross-language consistency check failure

## Troubleshooting

### Library Not Found Error

```
❌ Betti-RDL Shared Library Not Found
Expected library at: ../build/shared/lib/libbetti_rdl_c.so
```

**Solutions:**
1. Run the full matrix test: `./scripts/run_binding_matrix.sh`
2. Set environment variable: `export BETTI_RDL_SHARED_LIB_DIR=/path/to/lib`
3. Build manually: `cd src/cpp_kernel/build && cmake .. && make`

### Language-Specific Issues

**Python**: Missing pybind11
```bash
pip install pybind11
```

**Node.js**: Missing node-gyp
```bash
npm install
```

**Go**: CGO compiler errors
```bash
# Ensure C++ compiler is installed
sudo apt-get install g++  # Ubuntu/Debian
```

**Rust**: CMake not found
```bash
# Install CMake
sudo apt-get install cmake  # Ubuntu/Debian
```

### Environment Variables

- `BETTI_RDL_SHARED_LIB_DIR`: Override shared library search path
- `BETTI_RDL_INCLUDE_DIR`: Override header file search path
- `LD_LIBRARY_PATH`: Add library search path (Linux)

## Smoke Test Workload

The binding matrix test uses a simple counter workload:

1. Metrics captured per run; see logs.

2. **Spawn**: Create a process at origin (0,0,0)
3. Metrics captured per run; see logs.

5. **Verify**: Check event counts and telemetry

This workload is designed to:
- **Exercise core functionality** without complex logic
- **Be deterministic** for cross-language comparison
- **Complete quickly** for CI validation
- **Test memory management** and O(1) guarantees

## Architecture Benefits

### Before (Ad-hoc Paths)
```
Python: ../src/cpp_kernel/build/Release/libbetti_rdl_c.so
Node.js: ../src/cpp_kernel/build/Release/libbetti_rdl_c.so  
Go: ../src/cpp_kernel/build/Release/libbetti_rdl_c.so
Rust: (builds its own copy)
```

**Problems:**
- Each language builds/links separately
- Different library versions possible
- Inconsistent error messages
- Slow CI with redundant builds

### After (Shared Library)
```
All languages: build/shared/lib/libbetti_rdl_c.so
```

**Benefits:**
- Single build step for all languages
- Guaranteed library version consistency
- Unified error handling
- Faster CI with parallel execution
- Easy library path management

## Development Workflow

### Making Changes

1. **Edit C++ kernel** in `src/cpp_kernel/`
2. **Run binding matrix test**: `./scripts/run_binding_matrix.sh`
3. **Check all languages pass** before committing
4. **CI will validate** automatically on push

### Adding New Bindings

1. **Create binding directory** (e.g., `julia/`, `java/`)
2. **Implement C API wrapper** for the language
3. **Add library path detection** similar to existing bindings
4. **Include in matrix test** `scripts/run_binding_matrix.sh`
5. **Add CI job** to `.github/workflows/ci.yml`

### Version Compatibility

When the C API changes:

1. **Update C API version** in `betti_rdl_c_api.h`
2. **Update all bindings** to handle new version
3. **Run binding matrix test** to verify compatibility
4. **Update documentation** in this file

## Performance Validation

The binding matrix test includes performance checks:

- **Event processing rate** consistency across languages
- **Memory usage** validation (O(1) guarantee)
- **Timing precision** for event scheduling

Performance regressions are detected when:
- Any language shows significantly different performance
- Memory usage grows unexpectedly
- Event processing rates deviate by >10%

---

*This validation system ensures that Betti-RDL maintains cross-language compatibility and performance guarantees as the project evolves.*