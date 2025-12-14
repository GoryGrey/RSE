#include "betti_rdl_c_api.h"
#include "demos/BettiRDLCompute.h"

extern "C" {

BettiRDLCompute *betti_rdl_create() { return new BettiRDLCompute(); }

void betti_rdl_destroy(BettiRDLCompute *kernel) { delete kernel; }

void betti_rdl_spawn_process(BettiRDLCompute *kernel, int x, int y, int z) {
  if (kernel) {
    kernel->spawnProcess(x, y, z);
  }
}

void betti_rdl_inject_event(BettiRDLCompute *kernel, int x, int y, int z,
                            int value) {
  if (kernel) {
    kernel->injectEvent(x, y, z, value);
  }
}

void betti_rdl_run(BettiRDLCompute *kernel, int max_events) {
  if (kernel) {
    kernel->run(max_events);
  }
}

unsigned long long
betti_rdl_get_events_processed(const BettiRDLCompute *kernel) {
  return kernel ? kernel->getEventsProcessed() : 0;
}

unsigned long long betti_rdl_get_current_time(const BettiRDLCompute *kernel) {
  return kernel ? kernel->getCurrentTime() : 0;
}

size_t betti_rdl_get_process_count(const BettiRDLCompute *kernel) {
  return kernel ? kernel->getProcessCount() : 0;
}

int betti_rdl_get_process_state(const BettiRDLCompute *kernel, int pid) {
  return kernel ? kernel->getProcessState(pid) : 0;
}

} // extern "C"
