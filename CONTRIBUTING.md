---
**Last Updated**: December 18, 2025 at 13:31 UTC
**Status**: Current
---

# Contributing to Betti-RDL

Thank you for your interest in contributing to Betti-RDL! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Testing](#testing)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Review Process](#review-process)
- [Community](#community)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors, regardless of experience level, background, or identity.

### Expected Behavior

- Be respectful and constructive in discussions
- Welcome newcomers and help them get started
- Focus on what is best for the project and community
- Show empathy towards other contributors

### Unacceptable Behavior

- Harassment, discrimination, or offensive comments
- Personal attacks or trolling
- Publishing others' private information
- Other conduct inappropriate in a professional setting

### Reporting

Report Code of Conduct violations to greg@betti.dev. All reports will be reviewed and investigated confidentially.

---

## Getting Started

### Types of Contributions

We welcome many types of contributions:

1. **Bug Reports** - Found a bug? Let us know!
2. **Feature Requests** - Have an idea? Share it!
3. **Documentation** - Improve or clarify docs
4. **Bug Fixes** - Fix an open issue
5. **New Features** - Implement requested features
6. **Performance** - Optimize existing code
7. **Tests** - Add or improve test coverage
8. **Examples** - Create example programs

### Good First Issues

Look for issues labeled `good first issue` for beginner-friendly contributions:
https://github.com/betti-labs/betti-rdl/labels/good%20first%20issue

---

## Development Setup

### Prerequisites

- C++17 compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.10+
- Git
- One or more of: Rust, Python 3.7+, Node.js 14+, Go 1.16+

### Clone Repository

```bash
git clone https://github.com/betti-labs/betti-rdl.git
cd betti-rdl
```

### Build C++ Kernel

```bash
cd src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
ctest --output-on-failure
```

### Build Language Bindings

**Rust:**
```bash
cd rust
cargo build
cargo test
```

**Python:**
```bash
cd python
python3 -m venv venv
source venv/bin/activate
pip install pybind11
pip install -e .
python tests/test_end_to_end.py
```

**Node.js:**
```bash
cd nodejs
bash configure_binding.sh
npm install
node test.js
```

**Go:**
```bash
cd go
export LD_LIBRARY_PATH=../build/shared/lib:$LD_LIBRARY_PATH
go run example/main.go
```

### Verify Setup

Run the binding matrix test:
```bash
./scripts/run_binding_matrix.sh
```

All tests should pass ✅

---

## Making Changes

### 1. Create a Branch

```bash
git checkout -b feature/my-awesome-feature
# or
git checkout -b bugfix/issue-123
```

Branch naming:
- `feature/` - New features
- `bugfix/` - Bug fixes
- `docs/` - Documentation changes
- `perf/` - Performance improvements
- `test/` - Test additions/improvements

### 2. Make Your Changes

Edit the relevant files. Keep changes focused and atomic.

### 3. Follow Coding Standards

See [Coding Standards](#coding-standards) below.

### 4. Add Tests

For C++ changes:
```cpp
// src/cpp_kernel/tests/my_test.cpp
#include "BettiRDLKernel.h"
#include <cassert>

void testMyFeature() {
    BettiRDLKernel kernel;
    // Test your feature
    assert(kernel.myFeature() == expected);
}

int main() {
    testMyFeature();
    return 0;
}
```

For Rust changes:
```rust
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_my_feature() {
        let mut kernel = Kernel::new();
        // Test your feature
        assert_eq!(kernel.my_feature(), expected);
    }
}
```

### 5. Run Tests

```bash
# C++ tests
cd src/cpp_kernel/build
ctest --output-on-failure

# Rust tests
cd rust
cargo test

# Python tests
cd python
python tests/test_end_to_end.py

# All bindings
./scripts/run_binding_matrix.sh
```

### 6. Update Documentation

- Update relevant .md files in `docs/`
- Add docstrings/comments to new functions
- Update `CHANGELOG.md` (if applicable)

### 7. Commit Your Changes

```bash
git add .
git commit -m "Add feature: brief description

Detailed explanation of what changed and why.

Fixes #123"
```

Commit message format:
- First line: Brief summary (50 chars or less)
- Blank line
- Detailed explanation (72 chars per line)
- Reference issues: `Fixes #123`, `Closes #456`

---

## Testing

### Test Types

1. **Unit Tests** - Test individual functions/classes
2. **Integration Tests** - Test component interactions
3. **End-to-End Tests** - Test complete workflows
4. **Performance Tests** - Validate throughput/latency

### Running Tests

**All tests:**
```bash
./scripts/run_all_tests.sh
```

**C++ only:**
```bash
cd src/cpp_kernel/build
ctest --output-on-failure
```

**Specific test:**
```bash
./build/threadsafe_scheduler_test
```

**With verbose output:**
```bash
ctest --output-on-failure --verbose
```

### Writing Good Tests

**Good Test:**
```cpp
void testEventOrdering() {
    BettiRDLKernel kernel;
    kernel.spawnProcess(0, 0, 0);
    
    // Inject events out of order
    kernel.injectEvent(0, 0, 0, 10, 3);  // ts=10
    kernel.injectEvent(0, 0, 0, 5, 2);   // ts=5
    kernel.injectEvent(0, 0, 0, 1, 1);   // ts=1
    
    // Should process in timestamp order
    kernel.run(3);
    
    // Verify current time is from last event
    assert(kernel.getCurrentTime() == 10);
}
```

**Bad Test:**
```cpp
void testSomething() {
    BettiRDLKernel kernel;
    kernel.run(100);  // No setup, unclear intent
    assert(true);     // Meaningless assertion
}
```

**Test Principles:**
- Test one thing at a time
- Clear setup, execution, verification
- Descriptive test names
- Include edge cases

---

## Coding Standards

### C++

**Style:**
- Follow existing code style
- Use camelCase for functions: `spawnProcess()`
- Use PascalCase for classes: `BettiRDLKernel`
- Use UPPER_CASE for constants: `MAX_SIZE`
- 4-space indentation
- No trailing whitespace

**Example:**
```cpp
class BettiRDLKernel {
public:
    void spawnProcess(int x, int y, int z) {
        if (x < 0 || x >= DIM) {
            throw std::invalid_argument("Invalid x coordinate");
        }
        // Implementation...
    }
    
private:
    static constexpr int DIM = 32;
    ToroidalSpace<DIM> space;
};
```

**Key Rules:**
- Use `const` wherever possible
- Prefer `constexpr` for compile-time constants
- Use RAII for resource management
- No raw `new`/`delete` (use smart pointers or pools)
- Prefer `std::array` over C arrays
- Use `[[nodiscard]]` for functions that return important values

### Rust

**Style:**
- Follow `rustfmt` defaults
- Use snake_case for functions: `spawn_process()`
- Use PascalCase for types: `Kernel`
- Use SCREAMING_SNAKE_CASE for constants: `MAX_SIZE`
- Run `cargo fmt` before committing

**Example:**
```rust
pub struct Kernel {
    ptr: *mut BettiRDLCompute,
}

impl Kernel {
    pub fn spawn_process(&mut self, x: i32, y: i32, z: i32) {
        assert!(!self.ptr.is_null(), "Kernel not initialized");
        unsafe {
            betti_rdl_spawn_process(self.ptr, x, y, z);
        }
    }
}
```

**Key Rules:**
- Run `cargo clippy` and fix warnings
- Use `unsafe` only when necessary (FFI)
- Add safety comments to `unsafe` blocks
- Prefer `Result` over panics for recoverable errors

### Python

**Style:**
- Follow PEP 8
- Use snake_case for functions: `spawn_process()`
- Use PascalCase for classes: `Kernel`
- 4-space indentation
- Run `black` formatter

**Example:**
```python
class Kernel:
    def __init__(self):
        """Create a new Betti-RDL kernel instance."""
        self._kernel = betti_rdl.Kernel()
    
    def spawn_process(self, x: int, y: int, z: int) -> None:
        """Spawn a process at coordinates (x, y, z).
        
        Args:
            x: X coordinate (0-31)
            y: Y coordinate (0-31)
            z: Z coordinate (0-31)
        """
        self._kernel.spawn_process(x, y, z)
```

**Key Rules:**
- Type hints for function signatures
- Docstrings for public functions
- Run `pylint` or `flake8`

### Documentation

**Code Comments:**
- Explain **why**, not **what**
- Keep comments up-to-date with code
- Use `//` for C++ single-line comments
- Use `/* */` for multi-line comments

**Example:**
```cpp
// Flush pending events before processing.
// This ensures thread-safe injection works correctly
// by moving events from the injection buffer to the main queue.
void flushPendingEvents() {
    std::lock_guard<std::mutex> lock(event_injection_lock);
    for (const auto& evt : pending_events) {
        event_queue.push(evt);
    }
    pending_events.clear();
}
```

**Markdown:**
- Use proper headings (`#`, `##`, `###`)
- Include code blocks with language tags
- Add links to related docs
- Keep lines under 100 characters

---

## Submitting Changes

### 1. Push Your Branch

```bash
git push origin feature/my-awesome-feature
```

### 2. Create Pull Request

Go to https://github.com/betti-labs/betti-rdl/pulls and click "New Pull Request".

**PR Title:**
- Clear and descriptive
- Example: "Add thread-safe event injection buffer"

**PR Description Template:**
```markdown
## Description
Brief summary of changes.

## Motivation
Why is this change needed?

## Changes
- Added feature X
- Fixed bug Y
- Updated docs Z

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests pass
- [ ] Manual testing performed

## Checklist
- [ ] Code follows style guidelines
- [ ] Documentation updated
- [ ] Tests added/updated and passing
- [ ] No new compiler warnings
- [ ] CHANGELOG.md updated (if applicable)

Fixes #123
```

### 3. Respond to Reviews

- Address reviewer feedback promptly
- Ask questions if something is unclear
- Push additional commits to the same branch
- Resolve conversations after addressing them

### 4. Squash and Merge

Once approved, maintainers will merge your PR.

---

## Review Process

### Timeline

- **Initial Review:** Within 3 days
- **Follow-up:** Within 2 days
- **Merge:** After approval and CI passes

### Review Criteria

Reviewers will check:
- Code quality and style
- Test coverage
- Documentation
- Performance impact
- Backward compatibility

### Common Feedback

**"Add tests for edge cases"**
```cpp
// Example: Test boundary conditions
void testGridBoundaries() {
    BettiRDLKernel kernel;
    kernel.spawnProcess(0, 0, 0);    // Min
    kernel.spawnProcess(31, 31, 31); // Max
    // Test wraparound
}
```

**"Improve documentation"**
```rust
/// Creates a new kernel instance.
///
/// # Panics
/// Panics if kernel creation fails (e.g., out of memory).
///
/// # Examples
/// ```
/// let mut kernel = Kernel::new();
/// kernel.spawn_process(0, 0, 0);
/// ```
pub fn new() -> Kernel {
    // ...
}
```

**"Extract to helper function"**
```cpp
// Before: Duplicate code
kernel.injectEvent(0, 0, 0, 1, 1);
kernel.injectEvent(0, 0, 0, 2, 2);
kernel.injectEvent(0, 0, 0, 3, 3);

// After: Helper function
void injectSequence(BettiRDLKernel& kernel, int count) {
    for (int i = 1; i <= count; i++) {
        kernel.injectEvent(0, 0, 0, i, i);
    }
}
```

---

## Community

### Communication Channels

- **GitHub Issues:** Bug reports, feature requests
- **GitHub Discussions:** Questions, ideas, announcements
- **Email:** greg@betti.dev (project lead)

### Getting Help

New to contributing? We're here to help!

- Ask questions in GitHub Discussions
- Tag issues with `question` label
- Reach out to maintainers

### Recognition

Contributors are recognized in:
- `CONTRIBUTORS.md` file
- Release notes
- Special thanks in documentation

---

## Development Tips

### Debugging

**GDB (C++):**
```bash
gdb ./build/my_test
(gdb) run
(gdb) bt  # Backtrace
(gdb) print variable_name
```

**Valgrind (Memory Leaks):**
```bash
valgrind --leak-check=full ./build/my_test
```

**AddressSanitizer:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
make
./my_test
```

### Performance Profiling

**perf (Linux):**
```bash
perf record -g ./stress_test
perf report
```

**Hotspot:**
```bash
perf record -g ./stress_test
hotspot perf.data
```

### IDE Setup

**Visual Studio Code:**
- Install C/C++ extension
- Install Rust Analyzer extension
- Use `.vscode/tasks.json` for build tasks

**CLion:**
- Open `src/cpp_kernel/CMakeLists.txt`
- Configure CMake profile
- Run/Debug configurations auto-detected

---

## Questions?

If you have questions not covered here:

1. Check existing documentation
2. Search GitHub Issues/Discussions
3. Ask in GitHub Discussions
4. Email greg@betti.dev

---

**Thank you for contributing to Betti-RDL! 🚀**
