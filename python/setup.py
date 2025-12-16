from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
import sys
import os

__version__ = "1.0.0"

# Shared library path from environment or relative path
possible_lib_dirs = [
    os.environ.get('BETTI_RDL_SHARED_LIB_DIR'),
    '../build/shared/lib',  # Shared build location
    '../src/cpp_kernel/build/Release',  # Fallback to local build
]

# Find the first existing library
shared_lib_path = None
for lib_dir in possible_lib_dirs:
    if lib_dir and os.path.exists(os.path.join(lib_dir, 'libbetti_rdl_c.so')):
        shared_lib_path = os.path.abspath(lib_dir)
        break

if not shared_lib_path:
    print(f"""
❌ Betti-RDL Shared Library Not Found
=====================================

Searched for libbetti_rdl_c.so in:
  - BETTI_RDL_SHARED_LIB_DIR: {os.environ.get('BETTI_RDL_SHARED_LIB_DIR', 'not set')}
  - ../build/shared/lib
  - ../src/cpp_kernel/build/Release

To fix this, either:
1. Run the binding matrix test: ../scripts/run_binding_matrix.sh
2. Set BETTI_RDL_SHARED_LIB_DIR environment variable to the correct path
3. Build the C++ kernel manually:
   cd ../src/cpp_kernel/build && cmake .. && make
    """)
    sys.exit(1)

include_dir = os.path.abspath('../src/cpp_kernel')

ext_modules = [
    Pybind11Extension(
        "betti_rdl",
        ["betti_rdl_bindings.cpp"],
        include_dirs=[include_dir, shared_lib_path],
        library_dirs=[shared_lib_path],
        libraries=["betti_rdl_c"],
        define_macros=[("VERSION_INFO", __version__)],
        cxx_std=17,
        extra_link_args=["-latomic"],
    ),
]

setup(
    name="betti-rdl",
    version=__version__,
    author="Gregory Betti",
    author_email="greg@betti.dev",
    description="Space-Time Native Computation Runtime with O(1) Memory",
    long_description=open("README.md").read(),
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.7",
)
