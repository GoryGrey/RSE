use std::env;
use std::path::PathBuf;

fn emit_rpath(dir: &PathBuf) {
    if cfg!(target_os = "linux") {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", dir.display());
    }
}

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let project_root = PathBuf::from(&manifest_dir).join("..");
    let cpp_kernel_path = project_root.join("src/cpp_kernel");
    let shared_lib_dir = project_root.join("build/shared/lib");
    let out_dir = env::var("OUT_DIR").unwrap();

    // Check for shared library first, then fallback to building
    let shared_lib_path = shared_lib_dir.join("libbetti_rdl_c.so");

    if shared_lib_path.exists() {
        println!(
            "✅ Using shared Betti-RDL library from: {}",
            shared_lib_path.display()
        );

        println!("cargo:rustc-link-search=native={}", shared_lib_dir.display());
        println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
        emit_rpath(&shared_lib_dir);
    } else {
        println!("📦 Building Betti-RDL library from source...");

        let mut config = cmake::Config::new(&cpp_kernel_path);
        config.define("CMAKE_BUILD_TYPE", "Release");

        let _dst = config.build_target("betti_rdl_c").build();

        // The library will be in the build directory
        let build_dir = PathBuf::from(&out_dir).join("build");

        println!("cargo:rustc-link-search=native={}", build_dir.display());
        println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
        emit_rpath(&build_dir);
    }

    // Link libatomic on non-MSVC platforms
    if !cfg!(target_env = "msvc") {
        println!("cargo:rustc-link-lib=atomic");
    }

    // Rerun build script if C API files change
    println!(
        "cargo:rerun-if-changed={}/betti_rdl_c_api.h",
        cpp_kernel_path.display()
    );
    println!(
        "cargo:rerun-if-changed={}/betti_rdl_c_api.cpp",
        cpp_kernel_path.display()
    );
    println!(
        "cargo:rerun-if-changed={}/CMakeLists.txt",
        cpp_kernel_path.display()
    );

    // Also check for shared library changes
    println!(
        "cargo:rerun-if-changed={}/../scripts/run_binding_matrix.sh",
        project_root.display()
    );
}
