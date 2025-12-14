fn main() {
    // Link to the C API DLL
    println!("cargo:rustc-link-search=native=../src/cpp_kernel/build/Release");
    println!("cargo:rustc-link-lib=dylib=betti_rdl_c");
    println!("cargo:rerun-if-changed=../src/cpp_kernel/build/Release/betti_rdl_c.dll");
}
