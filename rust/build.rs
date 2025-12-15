fn main() {
    // Link to the C API library
    println!("cargo:rustc-link-search=native=../src/cpp_kernel/build");
    println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
    println!("cargo:rerun-if-changed=../src/cpp_kernel/build/libbetti_rdl_c.so");
}
