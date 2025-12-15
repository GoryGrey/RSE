#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for C API
typedef struct BettiRDLCompute BettiRDLCompute;

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
BETTI_RDL_API void betti_rdl_run(BettiRDLCompute* kernel, int max_events);

// Queries
BETTI_RDL_API unsigned long long betti_rdl_get_events_processed(const BettiRDLCompute* kernel);
BETTI_RDL_API unsigned long long betti_rdl_get_current_time(const BettiRDLCompute* kernel);
BETTI_RDL_API size_t betti_rdl_get_process_count(const BettiRDLCompute* kernel);
BETTI_RDL_API int betti_rdl_get_process_state(const BettiRDLCompute* kernel, int pid);

#ifdef __cplusplus
}
#endif
