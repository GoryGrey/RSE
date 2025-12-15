from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
import sys

__version__ = "1.0.0"

ext_modules = [
    Pybind11Extension(
        "betti_rdl",
        ["betti_rdl_bindings.cpp"],
        include_dirs=["../src/cpp_kernel"],
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
