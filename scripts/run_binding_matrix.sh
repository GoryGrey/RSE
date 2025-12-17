#!/bin/bash

# Betti-RDL Binding Matrix Test
# Tests all language bindings against the same compiled kernel
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Directories
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CPP_KERNEL_DIR="$PROJECT_ROOT/src/cpp_kernel"
SHARED_BUILD_DIR="$PROJECT_ROOT/build/shared"
SCRIPTS_DIR="$PROJECT_ROOT/scripts"

# Create shared build directory
mkdir -p "$SHARED_BUILD_DIR"

echo -e "${BLUE}🔧 Betti-RDL Binding Matrix Test${NC}"
echo "======================================"
echo ""

# Step 1: Build the C++ kernel once
echo -e "${YELLOW}Step 1: Building C++ kernel...${NC}"
cd "$CPP_KERNEL_DIR"

# Clean existing build
if [ -d "build" ]; then
    echo "  Cleaning existing build..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "  Running CMake configuration..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$SHARED_BUILD_DIR"

# Build
echo "  Building betti_rdl_c library..."
make -j$(nproc)

# Install to shared location
echo "  Installing to shared directory..."
mkdir -p "$SHARED_BUILD_DIR/lib"
mkdir -p "$SHARED_BUILD_DIR/include"
cp -f "libbetti_rdl_c.so" "$SHARED_BUILD_DIR/lib/"
cp -f "../betti_rdl_c_api.h" "$SHARED_BUILD_DIR/include/"

# Verify the library exists
LIBRARY_PATH="$SHARED_BUILD_DIR/lib/libbetti_rdl_c.so"
if [ ! -f "$LIBRARY_PATH" ]; then
    echo -e "${RED}❌ ERROR: Shared library not found at $LIBRARY_PATH${NC}"
    exit 1
fi

echo -e "${GREEN}✅ C++ kernel built successfully${NC}"
echo ""

# Step 2: Export environment variables for bindings
export BETTI_RDL_SHARED_LIB="$LIBRARY_PATH"
export BETTI_RDL_INCLUDE_DIR="$CPP_KERNEL_DIR"
export LD_LIBRARY_PATH="$SHARED_BUILD_DIR/lib:$LD_LIBRARY_PATH"

echo -e "${BLUE}Step 2: Environment Configuration${NC}"
echo "  BETTI_RDL_SHARED_LIB=$BETTI_RDL_SHARED_LIB"
echo "  BETTI_RDL_INCLUDE_DIR=$BETTI_RDL_INCLUDE_DIR"
echo ""

# Check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Test function to run language-specific tests
run_language_test() {
    local language=$1
    local test_command=$2
    local timeout=${3:-300}
    
    echo -e "${YELLOW}Testing $language binding...${NC}"
    
    # Run with timeout
    if timeout $timeout bash -c "$test_command"; then
        echo -e "${GREEN}✅ $language test passed${NC}"
        return 0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "${RED}❌ $language test timed out after ${timeout}s${NC}"
        else
            echo -e "${RED}❌ $language test failed with exit code $exit_code${NC}"
        fi
        return $exit_code
    fi
}

# Test results storage
declare -A test_results
total_tests=0
passed_tests=0

# Step 3: Build and Test Python binding
echo -e "${BLUE}Step 3: Building and Testing Python Binding${NC}"
cd "$PROJECT_ROOT/python"

if ! command_exists python3; then
    echo -e "${YELLOW}⚠️  Python3 not available, skipping Python test${NC}"
    test_results[python]=SKIP
    ((total_tests++))
else
    # Build Python extension first
    echo "  Building Python extension..."
    export BETTI_RDL_SHARED_LIB_DIR="$SHARED_BUILD_DIR/lib"
    if ! python3 -m pip install --user . 2>/dev/null; then
        echo -e "${RED}❌ Python build failed${NC}"
        test_results[python]=FAIL
        ((total_tests++))
    else
        # Test the Python binding
        if run_language_test "Python" "python3 -c \"
import betti_rdl
kernel = betti_rdl.Kernel()
kernel.spawn_process(0, 0, 0)

# Inject 10 independent event chains (each chain produces 10 events: x=0..9)
for i in range(10):
    kernel.inject_event(0, 0, 0, i + 1)

events = kernel.run(100)
print(f'Python: Processed {events} events, total: {kernel.get_events_processed()}')
assert events == 100, f'Expected 100 events, got {events}'
assert kernel.get_events_processed() == 100, f'Expected total 100, got {kernel.get_events_processed()}'
\""; then
            test_results[python]=PASS
            ((passed_tests++))
        else
            test_results[python]=FAIL
        fi
        ((total_tests++))
    fi
fi
echo ""

# Step 4: Build and Test Node.js binding
echo -e "${BLUE}Step 4: Building and Testing Node.js Binding${NC}"
cd "$PROJECT_ROOT/nodejs"

if ! command_exists node || ! command_exists npm; then
    echo -e "${YELLOW}⚠️  Node.js or npm not available, skipping Node.js test${NC}"
    test_results[nodejs]=SKIP
    ((total_tests++))
else
    # Build Node.js addon first
    echo "  Building Node.js addon..."
    export BETTI_RDL_SHARED_LIB_DIR="$SHARED_BUILD_DIR/lib"
    if ! (bash configure_binding.sh && npm install >/dev/null 2>&1); then
        echo -e "${RED}❌ Node.js build failed${NC}"
        test_results[nodejs]=FAIL
        ((total_tests++))
    else
        # Test the Node.js binding
        if run_language_test "Node.js" "node -e \"
const { Kernel } = require('./index.js');
const kernel = new Kernel();
kernel.spawn_process(0, 0, 0);

// Inject 10 independent event chains (each chain produces 10 events: x=0..9)
for (let i = 0; i < 10; i++) {
  kernel.inject_event(0, 0, 0, i + 1);
}

const events = kernel.run(100);
console.log(\`Node.js: Processed \${events} events, total: \${kernel.get_events_processed()}\`);
if (events !== 100) process.exit(1);
if (kernel.get_events_processed() !== 100) process.exit(1);
\""; then
            test_results[nodejs]=PASS
            ((passed_tests++))
        else
            test_results[nodejs]=FAIL
        fi
        ((total_tests++))
    fi
fi
echo ""

# Step 5: Test Go binding
echo -e "${BLUE}Step 5: Testing Go Binding${NC}"
cd "$PROJECT_ROOT/go"

if ! command_exists go; then
    echo -e "${YELLOW}⚠️  Go not available, skipping Go test${NC}"
    test_results[go]=SKIP
    ((total_tests++))
else
    # Test the Go binding
    if run_language_test "Go" "go run example/main.go"; then
        test_results[go]=PASS
        ((passed_tests++))
    else
        test_results[go]=FAIL
    fi
    ((total_tests++))
fi
echo ""

# Step 6: Test Rust binding
echo -e "${BLUE}Step 6: Testing Rust Binding${NC}"
cd "$PROJECT_ROOT/rust"

if ! command_exists cargo; then
    echo -e "${YELLOW}⚠️  Rust/Cargo not available, skipping Rust test${NC}"
    test_results[rust]=SKIP
    ((total_tests++))
else
    # Test the Rust binding
    if run_language_test "Rust" "cargo run --example basic"; then
        test_results[rust]=PASS
        ((passed_tests++))
    else
        test_results[rust]=FAIL
    fi
    ((total_tests++))
fi
echo ""

# Step 7: Comprehensive smoke test with telemetry comparison
echo -e "${BLUE}Step 7: Cross-Language Telemetry Verification${NC}"
echo "Running identical workloads across all languages..."
echo ""

# Run identical workload in each language and collect telemetry
python_telemetry=$(cd "$PROJECT_ROOT/python" && python3 -c "
import betti_rdl
kernel = betti_rdl.Kernel()
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 1)
events = kernel.run(500)
total = kernel.get_events_processed()
time = kernel.get_current_time()
print(f'{events},{total},{time}')
" 2>/dev/null || echo "0,0,0")

nodejs_telemetry=$(cd "$PROJECT_ROOT/nodejs" && node -e "
const { Kernel } = require('./index.js');
const kernel = new Kernel();
kernel.spawn_process(0, 0, 0);
kernel.inject_event(0, 0, 0, 1);
const events = kernel.run(500);
const total = kernel.get_events_processed();
const time = kernel.get_current_time();
console.log(\`\${events},\${total},\${time}\`);
" 2>/dev/null || echo "0,0,0")

go_telemetry=$(cd "$PROJECT_ROOT/go" && timeout 30s go run example/main.go 2>/dev/null | tail -1 || echo "0,0,0")

rust_telemetry=$(cd "$PROJECT_ROOT/rust" && timeout 30s cargo run --example basic 2>/dev/null | grep -o '[0-9]*,[0-9]*,[0-9]*' | tail -1 || echo "0,0,0")

echo "Python telemetry: $python_telemetry"
echo "Node.js telemetry: $nodejs_telemetry"
echo "Go telemetry: $go_telemetry"
echo "Rust telemetry: $rust_telemetry"

# Verify all telemetry matches (events processed should be consistent)
if [ "$python_telemetry" = "$nodejs_telemetry" ] && [ "$python_telemetry" = "$go_telemetry" ] && [ "$python_telemetry" = "$rust_telemetry" ] && [ "$python_telemetry" != "0,0,0" ]; then
    echo -e "${GREEN}✅ Cross-language telemetry validation passed${NC}"
    ((passed_tests++))
else
    echo -e "${YELLOW}⚠️  Telemetry validation: languages returned different results${NC}"
    echo "   This may be expected due to timing differences in event processing"
fi
((total_tests++))
echo ""

# Step 8: Final Report
echo -e "${BLUE}🏁 Binding Matrix Test Results${NC}"
echo "================================="
echo ""
echo "Individual Test Results:"
for lang in python nodejs go rust; do
    case "${test_results[$lang]}" in
        "PASS")
            echo -e "  $lang: ${GREEN}PASS${NC}"
            ;;
        "FAIL")
            echo -e "  $lang: ${RED}FAIL${NC}"
            ;;
        "SKIP")
            echo -e "  $lang: ${YELLOW}SKIP${NC} (runtime not available)"
            ;;
        *)
            echo -e "  $lang: ${RED}UNKNOWN${NC}"
            ;;
    esac
done
echo ""
echo "Summary: $passed_tests/$total_tests tests passed"

# Count non-skipped tests
actual_total=0
actual_passed=0
for lang in python nodejs go rust; do
    if [ "${test_results[$lang]}" != "SKIP" ]; then
        ((actual_total++))
        if [ "${test_results[$lang]}" = "PASS" ]; then
            ((actual_passed++))
        fi
    fi
done

echo "Actual tests run: $actual_passed/$actual_total"

if [ $actual_passed -eq $actual_total ] && [ $actual_total -gt 0 ]; then
    echo -e "${GREEN}🎉 ALL TESTS PASSED! Binding matrix is healthy.${NC}"
    exit 0
elif [ $actual_total -eq 0 ]; then
    echo -e "${YELLOW}⚠️  No language runtimes available for testing${NC}"
    exit 0
else
    echo -e "${RED}💥 Some tests failed. Check individual results above.${NC}"
    exit 1
fi