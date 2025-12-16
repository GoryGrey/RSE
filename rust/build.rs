use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let cpp_kernel_path = PathBuf::from(&manifest_dir).join("../src/cpp_kernel");
    let out_dir = env::var("OUT_DIR").unwrap();
    
    // Build the C++ library using CMake
    let mut config = cmake::Config::new(&cpp_kernel_path);
    config.define("CMAKE_BUILD_TYPE", "Release");
    
    // Build without installing
    let _dst = config.build_target("betti_rdl_c").build();
    
    // The library will be in the build directory
    let build_dir = PathBuf::from(&out_dir)
        .join("build");
    
    // Emit cargo directives
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
    
    // Link libatomic on non-MSVC platforms
    if !cfg!(target_env = "msvc") {
        println!("cargo:rustc-link-lib=atomic");
    }
    
    // Rerun build script if C API files change
    println!("cargo:rerun-if-changed={}/betti_rdl_c_api.h", cpp_kernel_path.display());
    println!("cargo:rerun-if-changed={}/betti_rdl_c_api.cpp", cpp_kernel_path.display());
    println!("cargo:rerun-if-changed={}/CMakeLists.txt", cpp_kernel_path.display());
}
