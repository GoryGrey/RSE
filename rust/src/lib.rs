// Rust FFI wrapper - links to C API
use std::os::raw::c_int;

extern "C" {
    fn betti_rdl_create() -> *mut std::ffi::c_void;
    fn betti_rdl_destroy(kernel: *mut std::ffi::c_void);
    fn betti_rdl_spawn_process(kernel: *mut std::ffi::c_void, x: c_int, y: c_int, z: c_int);
    fn betti_rdl_inject_event(kernel: *mut std::ffi::c_void, x: c_int, y: c_int, z: c_int, value: c_int);
    fn betti_rdl_run(kernel: *mut std::ffi::c_void, max_events: c_int) -> c_int;
    fn betti_rdl_get_events_processed(kernel: *const std::ffi::c_void) -> u64;
    fn betti_rdl_get_current_time(kernel: *const std::ffi::c_void) -> u64;
    fn betti_rdl_get_process_count(kernel: *const std::ffi::c_void) -> usize;
    fn betti_rdl_get_process_state(kernel: *const std::ffi::c_void, pid: c_int) -> c_int;
}

pub struct Kernel {
    inner: *mut std::ffi::c_void,
}

impl Kernel {
    pub fn new() -> Self {
        unsafe {
            let ptr = betti_rdl_create();
            assert!(!ptr.is_null(), "Failed to create Betti-RDL kernel");
            Kernel {
                inner: ptr,
            }
        }
    }

    pub fn spawn_process(&mut self, x: i32, y: i32, z: i32) {
        unsafe {
            betti_rdl_spawn_process(self.inner, x, y, z);
        }
    }

    pub fn inject_event(&mut self, x: i32, y: i32, z: i32, value: i32) {
        unsafe {
            betti_rdl_inject_event(self.inner, x, y, z, value);
        }
    }

    /// Run the kernel for at most `max_events` and return the number of events processed.
    pub fn run(&mut self, max_events: i32) -> i32 {
        unsafe {
            betti_rdl_run(self.inner, max_events)
        }
    }

    pub fn events_processed(&self) -> u64 {
        unsafe { betti_rdl_get_events_processed(self.inner) }
    }

    pub fn current_time(&self) -> u64 {
        unsafe { betti_rdl_get_current_time(self.inner) }
    }

    pub fn process_count(&self) -> usize {
        unsafe { betti_rdl_get_process_count(self.inner) }
    }

    pub fn process_state(&self, pid: i32) -> i32 {
        unsafe { betti_rdl_get_process_state(self.inner, pid) }
    }
}

impl Drop for Kernel {
    fn drop(&mut self) {
        unsafe {
            betti_rdl_destroy(self.inner);
        }
    }
}

unsafe impl Send for Kernel {}
unsafe impl Sync for Kernel {}
