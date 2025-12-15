#include "../demos/BettiRDLKernel.h"
#include "../demos/BettiRDLCompute.h"
#include <thread>
#include <vector>
#include <cassert>
#include <iostream>
#include <atomic>
#include <chrono>

// ============================================================================
// THREAD-SAFE SCHEDULER TESTS
// ============================================================================

void printTestHeader(const std::string &title) {
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   " << title << std::endl;
  std::cout << "=================================================" << std::endl;
}

void testRunMaxEventsSemantics() {
  printTestHeader("TEST 1: run(max_events) Returns Count Processed");
  std::cout << "Validates that run(max_events) processes AT MOST max_events"
            << " and returns the count\n" << std::endl;

  BettiRDLKernel kernel;

  // Create simple ring
  for (int i = 0; i < 5; i++) {
    kernel.spawnProcess(i, 0, 0);
    kernel.createEdge(i, 0, 0, (i + 1) % 5, 0, 0, 1);
  }

  // Inject initial event
  kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);

  // First run: 10 events max
  int processed_1 = kernel.run(10);
  std::cout << "  First run(10): " << processed_1 << " events processed" << std::endl;
  assert(processed_1 <= 10);
  assert(processed_1 > 0);  // Should process at least 1

  unsigned long long total_after_1 = kernel.getEventsProcessed();
  std::cout << "  Lifetime total after first run: " << total_after_1 << std::endl;
  assert(total_after_1 == processed_1);

  // Second run: 5 events max
  int processed_2 = kernel.run(5);
  std::cout << "  Second run(5): " << processed_2 << " events processed" << std::endl;
  assert(processed_2 <= 5);

  unsigned long long total_after_2 = kernel.getEventsProcessed();
  std::cout << "  Lifetime total after second run: " << total_after_2 << std::endl;
  assert(total_after_2 == processed_1 + processed_2);

  std::cout << "\n  [✓] PASS: run() correctly processes at most max_events"
            << " and returns count\n" << std::endl;
}

void testConcurrentEventInjection() {
  printTestHeader("TEST 2: Concurrent Event Injection");
  std::cout << "Validates that injectEvent() is thread-safe and events"
            << " are properly queued\n" << std::endl;

  BettiRDLKernel kernel;

  // Create single process
  kernel.spawnProcess(0, 0, 0);

  // Concurrent injection from multiple threads
  int num_threads = 4;
  int events_per_thread = 25;
  std::vector<std::thread> threads;

  std::cout << "  Spawning " << num_threads << " threads injecting "
            << events_per_thread << " events each...\n" << std::endl;

  for (int t = 0; t < num_threads; t++) {
    threads.emplace_back([&kernel, t, events_per_thread]() {
      for (int i = 0; i < events_per_thread; i++) {
        kernel.injectEvent(0, 0, 0, t, 0, 0, i);
      }
    });
  }

  // Wait for all injections to complete
  for (auto &t : threads) {
    t.join();
  }

  std::cout << "  All injections complete. Running scheduler...\n" << std::endl;

  // Run the scheduler to process all events
  int total_events = num_threads * events_per_thread;
  int processed = kernel.run(total_events);

  std::cout << "  Events processed: " << processed << " / " << total_events << std::endl;
  assert(processed == total_events);

  std::cout << "\n  [✓] PASS: Thread-safe event injection works correctly\n" << std::endl;
}

void testDeterminismWithConcurrentInjection() {
  printTestHeader("TEST 3: Determinism Despite Concurrent Injection");
  std::cout << "Validates that results are deterministic regardless of"
            << " thread interleaving\n" << std::endl;

  const int NUM_RUNS = 3;
  unsigned long long first_run_final_time;

  for (int run = 0; run < NUM_RUNS; run++) {
    BettiRDLKernel kernel;

    // Create ring topology
    for (int i = 0; i < 3; i++) {
      kernel.spawnProcess(i, 0, 0);
      kernel.createEdge(i, 0, 0, (i + 1) % 3, 0, 0, 1);
    }

    // Inject events concurrently
    std::vector<std::thread> threads;
    for (int t = 0; t < 2; t++) {
      threads.emplace_back([&kernel, t]() {
        for (int i = 0; i < 5; i++) {
          kernel.injectEvent(t, 0, 0, t, 0, 0, i);
        }
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    // Process events
    (void)kernel.run(100);

    unsigned long long final_time = kernel.getCurrentTime();

    if (run == 0) {
      first_run_final_time = final_time;
      std::cout << "  Run 1: Final time = " << final_time << std::endl;
    } else {
      std::cout << "  Run " << (run + 1) << ": Final time = " << final_time << std::endl;
      assert(final_time == first_run_final_time);
      (void)first_run_final_time;  // Suppress unused variable warning
    }
  }

  std::cout << "\n  [✓] PASS: All runs produced identical results\n" << std::endl;
}

void testBettiRDLComputeRunSemantics() {
  printTestHeader("TEST 4: BettiRDLCompute run() Semantics");
  std::cout << "Validates that BettiRDLCompute also uses correct run()"
            << " semantics\n" << std::endl;

  BettiRDLCompute kernel;

  // Create processes
  for (int i = 0; i < 3; i++) {
    kernel.spawnProcess(i, 0, 0);
  }

  // Inject events
  kernel.injectEvent(0, 0, 0, 1);
  kernel.injectEvent(1, 0, 0, 2);

  // First batch
  int batch1 = kernel.run(5);
  std::cout << "  First batch (max 5): " << batch1 << " events" << std::endl;
  assert(batch1 <= 5);

  unsigned long long total1 = kernel.getEventsProcessed();

  // Second batch
  int batch2 = kernel.run(5);
  std::cout << "  Second batch (max 5): " << batch2 << " events" << std::endl;
  assert(batch2 <= 5);

  unsigned long long total2 = kernel.getEventsProcessed();
  assert(total2 >= total1);
  (void)total1;  // Suppress unused variable warning
  (void)total2;  // Suppress unused variable warning

  std::cout << "\n  [✓] PASS: BettiRDLCompute run() semantics correct\n" << std::endl;
}

void testLifetimeEventCounter() {
  printTestHeader("TEST 5: Lifetime Event Counter");
  std::cout << "Validates that getEventsProcessed() returns lifetime total,"
            << " not per-run count\n" << std::endl;

  BettiRDLKernel kernel;

  // Create ring
  for (int i = 0; i < 3; i++) {
    kernel.spawnProcess(i, 0, 0);
    kernel.createEdge(i, 0, 0, (i + 1) % 3, 0, 0, 1);
  }

  kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);

  unsigned long long total = 0;

  // Multiple runs
  for (int i = 0; i < 3; i++) {
    int processed = kernel.run(10);
    total += processed;

    unsigned long long lifetime = kernel.getEventsProcessed();
    std::cout << "  Run " << (i + 1) << ": processed=" << processed
              << ", lifetime=" << lifetime << std::endl;

    assert(lifetime == total);
  }

  std::cout << "\n  [✓] PASS: Lifetime counter correctly accumulates\n" << std::endl;
}

void testCurrentTimeTracking() {
  printTestHeader("TEST 6: Current Time Tracking");
  std::cout << "Validates that getCurrentTime() progresses correctly\n" << std::endl;

  BettiRDLKernel kernel;

  // Single process
  kernel.spawnProcess(0, 0, 0);

  kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);

  unsigned long long time1 = kernel.getCurrentTime();
  std::cout << "  Initial time: " << time1 << std::endl;

  (void)kernel.run(10);

  unsigned long long time2 = kernel.getCurrentTime();
  std::cout << "  Time after run(10): " << time2 << std::endl;

  assert(time2 >= time1);

  std::cout << "\n  [✓] PASS: Time tracking works correctly\n" << std::endl;
}

int main() {
  std::cout << "================================================="
            << std::endl;
  std::cout << "   BETTI-RDL THREAD-SAFE SCHEDULER TEST SUITE"
            << std::endl;
  std::cout << "=================================================" << std::endl;

  try {
    testRunMaxEventsSemantics();
    testConcurrentEventInjection();
    testDeterminismWithConcurrentInjection();
    testBettiRDLComputeRunSemantics();
    testLifetimeEventCounter();
    testCurrentTimeTracking();

    std::cout << "\n================================================="
              << std::endl;
    std::cout << "   ALL TESTS PASSED ✓"
              << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "\n[FAILED] " << e.what() << std::endl;
    return 1;
  }
}
