#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for C API
typedef struct BettiRDLCompute BettiRDLCompute;

typedef struct {
  uint64_t events_processed;
  uint64_t current_time;
  size_t process_count;
  size_t memory_used;
} BettiRDLTelemetry;

#ifdef _WIN32
    #ifdef BETTI_RDL_EXPORTS
        #define BETTI_RDL_API __declspec(dllexport)
    #else
        #define BETTI_RDL_API __declspec(dllimport)
    #endif
#else
    #define BETTI_RDL_API
#endif

// Lifecycle
BETTI_RDL_API BettiRDLCompute* betti_rdl_create();
BETTI_RDL_API void betti_rdl_destroy(BettiRDLCompute* kernel);

// Process management
BETTI_RDL_API void betti_rdl_spawn_process(BettiRDLCompute* kernel, int x, int y, int z);

// Event injection
BETTI_RDL_API void betti_rdl_inject_event(BettiRDLCompute* kernel, int x, int y, int z, int value);

// Execution
// Processes at most max_events NEW events (not based on lifetime total)
// Returns the number of events processed in this call
BETTI_RDL_API int betti_rdl_run(BettiRDLCompute* kernel, int max_events);

// Queries
// Get lifetime total events processed across all run() calls
BETTI_RDL_API uint64_t betti_rdl_get_events_processed(const BettiRDLCompute* kernel);
// Get current simulation time
BETTI_RDL_API uint64_t betti_rdl_get_current_time(const BettiRDLCompute* kernel);
// Get number of active processes
BETTI_RDL_API size_t betti_rdl_get_process_count(const BettiRDLCompute* kernel);

// Get runtime telemetry (deterministic, cross-language comparable)
BETTI_RDL_API BettiRDLTelemetry
betti_rdl_get_telemetry(const BettiRDLCompute* kernel);

// Get state of a specific process
BETTI_RDL_API int betti_rdl_get_process_state(const BettiRDLCompute* kernel, int pid);

#ifdef __cplusplus
}
#endif
