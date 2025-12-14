// Package bettirdl provides Go bindings for the Betti-RDL runtime
package bettirdl

/*
#cgo CXXFLAGS: -std=c++17 -I../src/cpp_kernel
#cgo LDFLAGS: -L../src/cpp_kernel/build/Release -lbetti_rdl_c -lstdc++

#include <stdlib.h>

// Forward declarations
typedef struct BettiRDLCompute BettiRDLCompute;

extern "C" {
    BettiRDLCompute* betti_rdl_create();
    void betti_rdl_destroy(BettiRDLCompute* kernel);
    void betti_rdl_spawn_process(BettiRDLCompute* kernel, int x, int y, int z);
    void betti_rdl_inject_event(BettiRDLCompute* kernel, int x, int y, int z, int value);
    void betti_rdl_run(BettiRDLCompute* kernel, int max_events);
    unsigned long long betti_rdl_get_events_processed(const BettiRDLCompute* kernel);
    unsigned long long betti_rdl_get_current_time(const BettiRDLCompute* kernel);
    size_t betti_rdl_get_process_count(const BettiRDLCompute* kernel);
    int betti_rdl_get_process_state(const BettiRDLCompute* kernel, int pid);
}
*/
import "C"
import "unsafe"

// Kernel represents a Betti-RDL computational kernel
// with 32x32x32 toroidal space and O(1) memory guarantee
type Kernel struct {
	ptr *C.BettiRDLCompute
}

// NewKernel creates a new Betti-RDL kernel
func NewKernel() *Kernel {
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
