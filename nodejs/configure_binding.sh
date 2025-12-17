#!/bin/bash

# Node.js GYP configuration helper
# This script helps configure the binding.gyp with proper library paths

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SHARED_LIB_DIR="$PROJECT_ROOT/build/shared/lib"
FALLBACK_LIB_DIR="$PROJECT_ROOT/src/cpp_kernel/build"
LEGACY_LIB_DIR="$PROJECT_ROOT/src/cpp_kernel/build/Release"

# Check for shared library first, then fallback
if [ -f "$SHARED_LIB_DIR/libbetti_rdl_c.so" ]; then
    LIB_DIR="$SHARED_LIB_DIR"
    echo "Using shared library from: $LIB_DIR"
elif [ -f "$FALLBACK_LIB_DIR/libbetti_rdl_c.so" ]; then
    LIB_DIR="$FALLBACK_LIB_DIR"
    echo "Using fallback library from: $LIB_DIR"
elif [ -f "$LEGACY_LIB_DIR/libbetti_rdl_c.so" ]; then
    LIB_DIR="$LEGACY_LIB_DIR"
    echo "Using legacy fallback library from: $LIB_DIR"
else
    echo "❌ Betti-RDL library not found!"
    echo "Searched in: $SHARED_LIB_DIR, $FALLBACK_LIB_DIR, and $LEGACY_LIB_DIR"
    echo "Run: ../scripts/run_binding_matrix.sh to build the shared library"
    exit 1
fi

# Create a temporary gyp file with the correct library paths
cat > binding.gyp.tmp << EOF
{
  "targets": [
    {
      "target_name": "betti_rdl",
      "sources": [ "betti_rdl.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../src/cpp_kernel"
      ],
      "libraries": [
        "-L$LIB_DIR",
        "-lbetti_rdl_c",
        "-lstdc++",
        "-latomic",
        "-Wl,-rpath,$LIB_DIR"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "AdditionalOptions": [ "/std:c++17" ]
        }
      },
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.15",
        "OTHER_CFLAGS": [ "-std=c++17" ]
      }
    }
  ]
}
EOF

# Replace the original gyp file
mv binding.gyp.tmp binding.gyp
echo "✅ Updated binding.gyp with library path: $LIB_DIR"