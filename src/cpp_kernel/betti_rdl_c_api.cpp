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

int betti_rdl_run(BettiRDLCompute *kernel, int max_events) {
  if (kernel) {
    return kernel->run(max_events);
  }
  return 0;
}

uint64_t
betti_rdl_get_events_processed(const BettiRDLCompute *kernel) {
  return kernel ? kernel->getEventsProcessed() : 0;
}

uint64_t betti_rdl_get_current_time(const BettiRDLCompute *kernel) {
  return kernel ? kernel->getCurrentTime() : 0;
}

size_t betti_rdl_get_process_count(const BettiRDLCompute *kernel) {
  return kernel ? kernel->getProcessCount() : 0;
}

BettiRDLTelemetry betti_rdl_get_telemetry(const BettiRDLCompute *kernel) {
  BettiRDLTelemetry telemetry{};
  if (!kernel) {
    return telemetry;
  }

  const auto cpp_telemetry = kernel->getTelemetry();
  telemetry.events_processed = cpp_telemetry.events_processed;
  telemetry.current_time = cpp_telemetry.current_time;
  telemetry.process_count = cpp_telemetry.process_count;
  telemetry.memory_used = cpp_telemetry.memory_used;

  return telemetry;
}

int betti_rdl_get_process_state(const BettiRDLCompute *kernel, int pid) {
  return kernel ? kernel->getProcessState(pid) : 0;
}

} // extern "C"
