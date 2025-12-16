use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let project_root = PathBuf::from(&manifest_dir).join("..");
    let cpp_kernel_path = project_root.join("src/cpp_kernel");
    let shared_lib_dir = project_root.join("build/shared/lib");
    let out_dir = env::var("OUT_DIR").unwrap();
    
    // Check for shared library first, then fallback to building
    let shared_lib_path = shared_lib_dir.join("libbetti_rdl_c.so");
    
    if shared_lib_path.exists() {
        println!("✅ Using shared Betti-RDL library from: {}", shared_lib_path.display());
        
        // Use shared library
        println!("cargo:rustc-link-search=native={}", shared_lib_dir.display());
        println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
    } else {
        println!("📦 Building Betti-RDL library from source...");
        
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
    }
    
    // Link libatomic on non-MSVC platforms
    if !cfg!(target_env = "msvc") {
        println!("cargo:rustc-link-lib=atomic");
    }
    
    // Rerun build script if C API files change
    println!("cargo:rerun-if-changed={}/betti_rdl_c_api.h", cpp_kernel_path.display());
    println!("cargo:rerun-if-changed={}/betti_rdl_c_api.cpp", cpp_kernel_path.display());
    println!("cargo:rerun-if-changed={}/CMakeLists.txt", cpp_kernel_path.display());
    
    // Also check for shared library changes
    println!("cargo:rerun-if-changed={}/../scripts/run_binding_matrix.sh", project_root.display());
}
