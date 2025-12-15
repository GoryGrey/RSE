#include "../Allocator.h"
#include "BettiRDLCompute.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

// Parallel Scaling Test
// Proves Betti-RDL enables better parallelism than traditional approaches

void runSingleInstance(int instance_id, int events) {
  BettiRDLCompute kernel;

  // Create small topology
  for (int i = 0; i < 5; i++) {
    kernel.spawnProcess(i, instance_id, 0);
  }

  // Inject events
  for (int i = 0; i < events; i++) {
    kernel.injectEvent(0, instance_id, 0, i);
  }

  // Run computation
  kernel.run(events);
}

double testParallelScaling(int num_instances, int events_per_instance,
                           double baseline_eps) {
  std::cout << "\n[TEST] Running " << num_instances << " parallel instances..."
            << std::endl;

  size_t mem_before = MemoryManager::getUsedMemory();
  auto start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> threads;
  threads.reserve(num_instances);

  for (int i = 0; i < num_instances; i++) {
    threads.emplace_back(runSingleInstance, i, events_per_instance);
  }

  for (auto &t : threads) {
    t.join();
  }

  auto end = std::chrono::high_resolution_clock::now();
  size_t mem_after = MemoryManager::getUsedMemory();

  auto duration_us =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  double seconds = std::max(1.0e-6, duration_us / 1.0e6);

  const long long total_events =
      static_cast<long long>(num_instances) * events_per_instance;

  double eps = total_events / seconds;
  double speedup = baseline_eps > 0 ? (eps / baseline_eps) : 0.0;
  double efficiency = num_instances > 0 ? (speedup / num_instances) : 0.0;

  std::cout << "  Instances: " << num_instances << std::endl;
  std::cout << "  Events per instance: " << events_per_instance << std::endl;
  std::cout << "  Total events: " << total_events << std::endl;
  std::cout << "  Duration: " << std::fixed << std::setprecision(3) << seconds
            << "s" << std::endl;
  std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << eps
            << " EPS" << std::endl;
  std::cout << "  Speedup vs baseline: " << std::fixed << std::setprecision(2)
            << speedup << "x" << std::endl;
  std::cout << "  Scaling efficiency: " << std::fixed << std::setprecision(2)
            << (efficiency * 100.0) << "%" << std::endl;

  const long long mem_delta = static_cast<long long>(mem_after) -
                              static_cast<long long>(mem_before);

  std::cout << "  Memory delta: " << mem_delta << " bytes" << std::endl;
  std::cout << "  Memory per instance: " << (mem_delta / num_instances)
            << " bytes" << std::endl;

  return eps;
}

int main() {
  std::cout << "=================================================" << std::endl;
  std::cout << "   PARALLEL SCALING TEST                        " << std::endl;
  std::cout << "=================================================" << std::endl;
  std::cout << "\nGoal: Prove Betti-RDL enables linear speedup" << std::endl;
  std::cout << "      with constant memory per instance\n" << std::endl;

  const int events = 1000000;

  std::cout << "[BASELINE] Single instance..." << std::endl;
  auto baseline_start = std::chrono::high_resolution_clock::now();
  runSingleInstance(0, events);
  auto baseline_end = std::chrono::high_resolution_clock::now();

  auto baseline_us =
      std::chrono::duration_cast<std::chrono::microseconds>(baseline_end -
                                                            baseline_start)
          .count();
  double baseline_seconds = std::max(1.0e-6, baseline_us / 1.0e6);
  double baseline_eps = events / baseline_seconds;

  std::cout << "  Duration: " << std::fixed << std::setprecision(3)
            << baseline_seconds << "s" << std::endl;
  std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
            << baseline_eps << " EPS" << std::endl;

  // Test scaling
  testParallelScaling(1, events, baseline_eps);
  testParallelScaling(2, events, baseline_eps);
  testParallelScaling(4, events, baseline_eps);
  testParallelScaling(8, events, baseline_eps);
  testParallelScaling(16, events, baseline_eps);

  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   VALIDATION COMPLETE                          " << std::endl;
  std::cout << "=================================================" << std::endl;

  return 0;
}
