// Package bettirdl provides Go bindings for the Betti-RDL runtime
package bettirdl

/*
#cgo CXXFLAGS: -std=c++17 -I../src/cpp_kernel
#cgo LDFLAGS: -L../src/cpp_kernel/build/Release -lbetti_rdl_c -lstdc++

// Dynamic library path selection
#ifdef __linux__
    // On Linux, try to load from shared location first
    #ifndef BETTI_RDL_LIB_PATH
        #define BETTI_RDL_LIB_PATH "../build/shared/lib:../src/cpp_kernel/build/Release"
    #endif
#endif

#include <stdlib.h>

// Forward declarations
typedef struct BettiRDLCompute BettiRDLCompute;

extern "C" {
    BettiRDLCompute* betti_rdl_create();
    void betti_rdl_destroy(BettiRDLCompute* kernel);
    void betti_rdl_spawn_process(BettiRDLCompute* kernel, int x, int y, int z);
    void betti_rdl_inject_event(BettiRDLCompute* kernel, int x, int y, int z, int value);
    int betti_rdl_run(BettiRDLCompute* kernel, int max_events);
    uint64_t betti_rdl_get_events_processed(const BettiRDLCompute* kernel);
    uint64_t betti_rdl_get_current_time(const BettiRDLCompute* kernel);
    size_t betti_rdl_get_process_count(const BettiRDLCompute* kernel);
    int betti_rdl_get_process_state(const BettiRDLCompute* kernel, int pid);
}
*/
import "C"
import "unsafe"
import "os"
import "fmt"
import "path/filepath"

// checkSharedLibrary checks if the Betti-RDL shared library exists
func checkSharedLibrary() {
    // Try shared location first, then fallback
    possiblePaths := []string{
        "../build/shared/lib",
        "../src/cpp_kernel/build/Release",
    }
    
    if libDir := os.Getenv("BETTI_RDL_SHARED_LIB_DIR"); libDir != "" {
        possiblePaths = append([]string{libDir}, possiblePaths...)
    }
    
    for _, path := range possiblePaths {
        if libPath := filepath.Join(path, "libbetti_rdl_c.so"); fileExists(libPath) {
            return // Found it, proceed
        }
    }
    
    // Library not found
    fmt.Fprintf(os.Stderr, `
❌ Betti-RDL Shared Library Not Found
=====================================

Expected library at one of:
`)
    for _, path := range possiblePaths {
        fmt.Fprintf(os.Stderr, "  - %s/libbetti_rdl_c.so\n", path)
    }
    fmt.Fprintf(os.Stderr, `
To fix this, either:
1. Run the binding matrix test: ../scripts/run_binding_matrix.sh
2. Set BETTI_RDL_SHARED_LIB_DIR environment variable to the correct path
3. Build the C++ kernel manually:
   cd ../src/cpp_kernel/build && cmake .. && make
`)
    os.Exit(1)
}

func fileExists(path string) bool {
    if _, err := os.Stat(path); os.IsNotExist(err) {
        return false
    }
    return true
}

// Kernel represents a Betti-RDL computational kernel
// with 32x32x32 toroidal space and O(1) memory guarantee
type Kernel struct {
    ptr *C.BettiRDLCompute
}

// NewKernel creates a new Betti-RDL kernel
func NewKernel() *Kernel {
    checkSharedLibrary() // Verify shared library exists before proceeding
    return &Kernel{
        ptr: C.betti_rdl_create(),
    }
}

// Close destroys the kernel and frees resources
func (k *Kernel) Close() {
    if k.ptr != nil {
        C.betti_rdl_destroy(k.ptr)
        k.ptr = nil
    }
}

// SpawnProcess spawns a process at spatial coordinates (x, y, z)
func (k *Kernel) SpawnProcess(x, y, z int) {
    C.betti_rdl_spawn_process(k.ptr, C.int(x), C.int(y), C.int(z))
}

// InjectEvent injects an event at coordinates with value
func (k *Kernel) InjectEvent(x, y, z, value int) {
    C.betti_rdl_inject_event(k.ptr, C.int(x), C.int(y), C.int(z), C.int(value))
}

// Run executes computation for up to maxEvents
func (k *Kernel) Run(maxEvents int) {
    C.betti_rdl_run(k.ptr, C.int(maxEvents))
}

// EventsProcessed returns the number of events processed
func (k *Kernel) EventsProcessed() uint64 {
    return uint64(C.betti_rdl_get_events_processed(k.ptr))
}

// CurrentTime returns the current logical time
func (k *Kernel) CurrentTime() uint64 {
    return uint64(C.betti_rdl_get_current_time(k.ptr))
}

// ProcessCount returns the number of active processes
func (k *Kernel) ProcessCount() int {
    return int(C.betti_rdl_get_process_count(k.ptr))
}

// ProcessState returns the accumulated state for a process
func (k *Kernel) ProcessState(pid int) int {
    return int(C.betti_rdl_get_process_state(k.ptr, C.int(pid)))
}
