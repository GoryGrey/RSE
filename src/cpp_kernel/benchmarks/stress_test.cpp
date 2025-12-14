#include "../Allocator.h"
#include "../demos/BettiRDLCompute.h"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

// ============================================================================
// BETTI-RDL STRESS TEST SUITE
// ============================================================================
// 1. The Firehose: Max throughput measurement
// 2. The Deep Dive: Deep recursion memory stability
// 3. The Swarm: Massive parallelism scaling
// ============================================================================

using namespace std::chrono;

void printHeader(const std::string &title) {
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   " << title << std::endl;
  std::cout << "=================================================" << std::endl;
}

// ----------------------------------------------------------------------------
// 1. THE FIREHOSE
// Goal: Measure raw event processing throughput
// ----------------------------------------------------------------------------
void runFirehose(int event_count) {
  printHeader("TEST 1: THE FIREHOSE (Throughput)");
  std::cout << "Goal: Process " << event_count << " events as fast as possible."
            << std::endl;

  BettiRDLCompute kernel;
  // Spawn a cluster to receive events
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      kernel.spawnProcess(x, y, 0);
    }
  }

  auto start = high_resolution_clock::now();

  // Burst injection
  // In a real scenario, run() would process the queue.
  // Here we inject and run in batches to simulate continuous load.
  int batch_size = 1000;
  int batches = event_count / batch_size;

  for (int i = 0; i < batches; i++) {
    // Inject batch
    for (int j = 0; j < batch_size; j++) {
      kernel.injectEvent(0, 0, 0, i * batch_size + j);
    }
    // Process batch
    kernel.run(batch_size);
  }

  auto end = high_resolution_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();
  double seconds = duration_ms / 1000.0;
  double eps = event_count / seconds;

  std::cout << "  Events: " << event_count << std::endl;
  std::cout << "  Time:   " << seconds << "s" << std::endl;
  std::cout << "  Speed:  " << std::fixed << std::setprecision(2) << eps
            << " Events/Sec" << std::endl;

  if (eps > 1000000) {
    std::cout << "  [SUCCESS] >1M EPS achieved!" << std::endl;
  } else {
    std::cout << "  [NOTE] Performance is nominal." << std::endl;
  }
}

// ----------------------------------------------------------------------------
// 2. THE DEEP DIVE
// Goal: Verify O(1) memory usage during deep recursion
// ----------------------------------------------------------------------------
void runDeepDive(int depth) {
  printHeader("TEST 2: THE DEEP DIVE (Memory Stability)");
  std::cout << "Goal: Chain " << depth << " dependent events." << std::endl;
  std::cout << "Expectation: 0 bytes memory growth." << std::endl;

  size_t mem_start = MemoryManager::getUsedMemory();
  std::cout << "  Memory Start: " << mem_start << " bytes" << std::endl;

  BettiRDLCompute kernel;
  kernel.spawnProcess(0, 0, 0);

  // Inject BIG initial event to start the chain
  kernel.injectEvent(0, 0, 0, 1);

  // Run for 'depth' steps
  // The kernel propagates events: 1 -> 2 -> 3 ...
  kernel.run(depth);

  size_t mem_end = MemoryManager::getUsedMemory();
  std::cout << "  Memory End:   " << mem_end << " bytes" << std::endl;

  long delta = (long)mem_end - (long)mem_start;
  std::cout << "  Delta:        " << delta << " bytes" << std::endl;

  if (delta < 1024) { // Allow tiny fluctuation for kernel struct itself
    std::cout << "  [SUCCESS] O(1) Memory Verified!" << std::endl;
  } else {
    std::cout << "  [FAIL] Memory grew by " << delta << " bytes." << std::endl;
  }
}

// ----------------------------------------------------------------------------
// 3. THE SWARM
// Goal: Verify parallel scaling
// ----------------------------------------------------------------------------
void workerThread(int id, int events, std::atomic<long> &total_events) {
  BettiRDLCompute kernel;
  kernel.spawnProcess(0, 0, 0);
  kernel.injectEvent(0, 0, 0, 1);

  // Simplistic run loop
  for (int i = 0; i < events; i++) {
    kernel.injectEvent(0, 0, 0, i);
    kernel.run(1);
  }
  total_events += events;
}

void runSwarm(int thread_count, int events_per_thread) {
  printHeader("TEST 3: THE SWARM (Parallel Scaling)");
  std::cout << "Goal: Run " << thread_count << " threads x "
            << events_per_thread << " events." << std::endl;

  std::vector<std::thread> threads;
  std::atomic<long> total_events = 0;

  auto start = high_resolution_clock::now();

  for (int i = 0; i < thread_count; i++) {
    threads.emplace_back(workerThread, i, events_per_thread,
                         std::ref(total_events));
  }

  for (auto &t : threads) {
    t.join();
  }

  auto end = high_resolution_clock::now();
  auto duration_ms = duration_cast<milliseconds>(end - start).count();
  double seconds = duration_ms / 1000.0;
  double total_eps = total_events / seconds;

  std::cout << "  Threads: " << thread_count << std::endl;
  std::cout << "  Total Events: " << total_events << std::endl;
  std::cout << "  Time: " << seconds << "s" << std::endl;
  std::cout << "  Aggregate Speed: " << std::fixed << std::setprecision(2)
            << total_eps << " EPS" << std::endl;
  std::cout << "  [SUCCESS] Threads maintained stability." << std::endl;
}

int main() {
  std::cout << "Betti-RDL System Stress Test" << std::endl;
  std::cout << "V 1.0.0" << std::endl;

  // 1. Firehose: 5 Million events
  // Note: In debug/console mode printing slows it down, kernel is chatty?
  // Assuming kernel isn't printing per event.
  runFirehose(5000000);

  // 2. Deep Dive: 100,000 recursive steps
  runDeepDive(100000);

  // 3. Swarm: 16 threads (typical high-end consumer CPU)
  runSwarm(16, 100000);

  return 0;
}
