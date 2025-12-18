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

    // Check environment variable first
    let env_lib_dir = env::var("BETTI_RDL_SHARED_LIB_DIR").ok().map(PathBuf::from);
    let shared_lib_dir = project_root.join("build/shared/lib");
    let out_dir = env::var("OUT_DIR").unwrap();

    let found_lib_dir = if let Some(dir) = env_lib_dir {
        if dir.join("libbetti_rdl_c.so").exists() {
            Some(dir)
        } else {
            None
        }
    } else {
        None
    }
    .or_else(|| {
        if shared_lib_dir.join("libbetti_rdl_c.so").exists() {
            Some(shared_lib_dir.clone())
        } else {
            None
        }
    });

    if let Some(dir) = found_lib_dir {
        println!(
            "✅ Using shared Betti-RDL library from: {}",
            dir.display()
        );

        println!("cargo:rustc-link-search=native={}", dir.display());
        println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
        emit_rpath(&dir);
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
