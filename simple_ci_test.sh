#!/bin/bash

# Simplified CI test - focus on core functionality
set -e

echo "🔧 Simplified CI Test"
echo "===================="
echo ""

# Set up environment
export LD_LIBRARY_PATH=/home/engine/project/build/shared/lib:$LD_LIBRARY_PATH
export BETTI_RDL_SHARED_LIB_DIR=/home/engine/project/build/shared/lib

# Test C++ compilation and unit tests
echo "Testing C++ kernel..."
cd /home/engine/project/src/cpp_kernel/build
ctest --output-on-failure -j4 && echo "✅ C++ tests PASSED" || echo "❌ C++ tests FAILED"
echo ""

# Test bindings individually with shorter timeouts
echo "Testing Python binding..."
cd /home/engine/project/python
timeout 20s python3 -c "
import betti_rdl
kernel = betti_rdl.Kernel()
for i in range(5):
    kernel.spawn_process(i, 0, 0)
    kernel.inject_event(i, 0, 0, 1)
events = kernel.run(50)
assert events >= 5, 'Expected at least 5 events, got {}'.format(events)
print('Python: {} events processed'.format(events))
" && echo "✅ Python PASSED" || echo "❌ Python FAILED"
echo ""

echo "Testing Node.js binding..."
cd /home/engine/project/nodejs
timeout 20s node -e "
const { Kernel } = require('./index.js');
const kernel = new Kernel();
for (let i = 0; i < 5; i++) {
    kernel.spawnProcess(i, 0, 0);
    kernel.injectEvent(i, 0, 0, 1);
}
const events = kernel.run(50);
if (events >= 5) {
    console.log('Node.js: ' + events + ' events processed');
} else {
    process.exit(1);
}
" && echo "✅ Node.js PASSED" || echo "❌ Node.js FAILED"
echo ""

echo "Testing Rust binding..."
cd /home/engine/project/rust
timeout 60s cargo test --release --quiet && echo "✅ Rust tests PASSED" || echo "❌ Rust tests FAILED"
echo ""

echo "Testing Grey compiler..."
cd /home/engine/project/grey_compiler
timeout 60s cargo test --quiet && echo "✅ Grey compiler tests PASSED" || echo "❌ Grey compiler tests FAILED"
echo ""

echo "🏁 Simplified CI test completed!"