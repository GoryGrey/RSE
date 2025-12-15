#include "../Allocator.h"
#include "BettiRDLKernel.h"
#include <chrono>
#include <iomanip>
#include <iostream>


// Betti-RDL Stress Test & Benchmark Suite

void printHeader(const std::string &title) {
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   " << title << std::endl;
  std::cout << "=================================================" << std::endl;
}

void printMetrics(const std::string &test_name, unsigned long long events,
                  unsigned long long time_units, size_t processes, size_t edges,
                  long long mem_delta, double duration_ms) {
  std::cout << "\n[" << test_name << " RESULTS]" << std::endl;
  std::cout << "    > Events Processed: " << events << std::endl;
  std::cout << "    > Time Units: " << time_units << std::endl;
  std::cout << "    > Processes: " << processes << std::endl;
  std::cout << "    > Edges: " << edges << std::endl;
  std::cout << "    > Memory Delta: " << mem_delta << " bytes" << std::endl;
  std::cout << "    > Duration: " << std::fixed << std::setprecision(2)
            << duration_ms << "ms" << std::endl;
  std::cout << "    > Events/sec: " << std::fixed << std::setprecision(0)
            << (events * 1000.0 / duration_ms) << std::endl;
  std::cout << "    > Memory/Event: " << std::fixed << std::setprecision(2)
            << (mem_delta / (double)events) << " bytes" << std::endl;
}

// Test 1: Throughput - How many events can we process?
void testThroughput() {
  printHeader("TEST 1: THROUGHPUT");
  std::cout << "Processing 1 million events in a ring topology" << std::endl;

  BettiRDLKernel kernel;

  auto coordsForNode = [](int i, int &x, int &y, int &z) {
    x = i % 32;
    y = (i / 32) % 32;
    z = 0;
  };

  // Create ring of 100 nodes (mapped into the 32x32 plane to avoid torus wrap
  // collisions)
  for (int i = 0; i < 100; i++) {
    int x1, y1, z1;
    int x2, y2, z2;
    coordsForNode(i, x1, y1, z1);
    coordsForNode((i + 1) % 100, x2, y2, z2);

    kernel.spawnProcess(x1, y1, z1);
    kernel.createEdge(x1, y1, z1, x2, y2, z2, 1);
  }

  // Inject 100 initial events
  for (int i = 0; i < 100; i++) {
    int x, y, z;
    coordsForNode(i, x, y, z);
    kernel.injectEvent(x, y, z, x, y, z, 1);
  }

  auto start = std::chrono::high_resolution_clock::now();
  size_t mem_before = MemoryManager::getUsedMemory();

  kernel.run(1000000);

  auto end = std::chrono::high_resolution_clock::now();
  size_t mem_after = MemoryManager::getUsedMemory();

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  printMetrics("THROUGHPUT", kernel.getEventsProcessed(),
               kernel.getCurrentTime(), 100, 100, mem_after - mem_before,
               duration.count());
}

// Test 2: Scalability - Does memory stay O(1) with more events?
void testScalability() {
  printHeader("TEST 2: SCALABILITY");
  std::cout << "Testing memory stability across increasing event counts"
            << std::endl;

  int event_counts[] = {1000, 10000, 100000, 1000000};

  for (int count : event_counts) {
    BettiRDLKernel kernel;

    // Small ring
    for (int i = 0; i < 10; i++) {
      kernel.spawnProcess(i, 0, 0);
      kernel.createEdge(i, 0, 0, (i + 1) % 10, 0, 0, 1);
    }

    kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);

    size_t mem_before = MemoryManager::getUsedMemory();
    auto start = std::chrono::high_resolution_clock::now();

    kernel.run(count);

    auto end = std::chrono::high_resolution_clock::now();
    size_t mem_after = MemoryManager::getUsedMemory();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n  [" << count << " events]" << std::endl;
    std::cout << "    Memory Delta: " << (mem_after - mem_before) << " bytes"
              << std::endl;
    std::cout << "    Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "    Events/sec: " << (count * 1000.0 / duration.count())
              << std::endl;
  }
}

// Test 3: Large Topology - Can we handle many processes?
void testLargeTopology() {
  printHeader("TEST 3: LARGE TOPOLOGY");
  std::cout << "Creating 1000 processes in 3D space" << std::endl;

  BettiRDLKernel kernel;

  // Create 10x10x10 cube
  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 10; y++) {
      for (int z = 0; z < 10; z++) {
        kernel.spawnProcess(x, y, z);
        // Create edges to neighbors
        kernel.createEdge(x, y, z, (x + 1) % 10, y, z, 2);
        kernel.createEdge(x, y, z, x, (y + 1) % 10, z, 2);
        kernel.createEdge(x, y, z, x, y, (z + 1) % 10, 2);
      }
    }
  }

  // Inject events at multiple points
  for (int i = 0; i < 10; i++) {
    kernel.injectEvent(i, i, i, i, i, i, 1);
  }

  auto start = std::chrono::high_resolution_clock::now();
  size_t mem_before = MemoryManager::getUsedMemory();

  kernel.run(100000);

  auto end = std::chrono::high_resolution_clock::now();
  size_t mem_after = MemoryManager::getUsedMemory();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  printMetrics("LARGE TOPOLOGY", kernel.getEventsProcessed(),
               kernel.getCurrentTime(), 1000, 3000, mem_after - mem_before,
               duration.count());
}

// Test 4: Sustained Load - Memory stability over time
void testSustainedLoad() {
  printHeader("TEST 4: SUSTAINED LOAD");
  std::cout << "Running for extended period to check memory leaks" << std::endl;

  BettiRDLKernel kernel;

  auto coordsForNode = [](int i, int &x, int &y, int &z) {
    x = i % 32;
    y = (i / 32) % 32;
    z = 0;
  };

  // Ring topology (mapped into the 32x32 plane to avoid torus wrap collisions)
  for (int i = 0; i < 50; i++) {
    int x1, y1, z1;
    int x2, y2, z2;
    coordsForNode(i, x1, y1, z1);
    coordsForNode((i + 1) % 50, x2, y2, z2);

    kernel.spawnProcess(x1, y1, z1);
    kernel.createEdge(x1, y1, z1, x2, y2, z2, 1);
  }

  {
    int x, y, z;
    coordsForNode(0, x, y, z);
    kernel.injectEvent(x, y, z, x, y, z, 1);
  }

  size_t mem_start = MemoryManager::getUsedMemory();
  auto time_start = std::chrono::high_resolution_clock::now();

  // Run in batches, checking memory each time
  int batch_size = 100000;
  int num_batches = 10;

  for (int batch = 0; batch < num_batches; batch++) {
    kernel.run(batch_size);
    size_t mem_current = MemoryManager::getUsedMemory();

    std::cout << "  Batch " << (batch + 1) << "/" << num_batches
              << ": Events=" << kernel.getEventsProcessed()
              << ", Memory=" << (mem_current - mem_start) << " bytes"
              << std::endl;
  }

  auto time_end = std::chrono::high_resolution_clock::now();
  size_t mem_end = MemoryManager::getUsedMemory();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      time_end - time_start);

  printMetrics("SUSTAINED LOAD", kernel.getEventsProcessed(),
               kernel.getCurrentTime(), 50, 50, mem_end - mem_start,
               duration.count());
}

// Test 5: Comparison with RDL paper benchmarks
void testComparison() {
  printHeader("TEST 5: RDL PAPER COMPARISON");
  std::cout << "Comparing to RDL paper results (7.7M events/sec)" << std::endl;

  BettiRDLKernel kernel;

  // Similar to RDL paper: 1000 node network
  for (int i = 0; i < 1000; i++) {
    kernel.spawnProcess(i % 32, (i / 32) % 32, 0);
  }

  // Create ring edges
  for (int i = 0; i < 1000; i++) {
    int x1 = i % 32, y1 = (i / 32) % 32;
    int x2 = (i + 1) % 32, y2 = ((i + 1) / 32) % 32;
    kernel.createEdge(x1, y1, 0, x2, y2, 0, 1);
  }

  kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);

  auto start = std::chrono::high_resolution_clock::now();
  kernel.run(1000000);
  auto end = std::chrono::high_resolution_clock::now();

  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  double events_per_sec =
      kernel.getEventsProcessed() * 1000.0 / duration.count();

  std::cout << "\n  Betti-RDL: " << std::fixed << std::setprecision(0)
            << events_per_sec << " events/sec" << std::endl;
  std::cout << "  RDL Paper: 7,728,399 events/sec (1000 nodes)" << std::endl;
  std::cout << "  Ratio: " << std::fixed << std::setprecision(2)
            << (events_per_sec / 7728399.0) << "x" << std::endl;
}

int main() {
  printHeader("BETTI-RDL STRESS TEST & BENCHMARK SUITE");
  std::cout << "\nTesting space-time unified computation at scale" << std::endl;

  testThroughput();
  testScalability();
  testLargeTopology();
  testSustainedLoad();
  testComparison();

  printHeader("BENCHMARK COMPLETE");
  std::cout << "\nKey Findings:" << std::endl;
  std::cout << "  • Memory should stay O(1) across all tests" << std::endl;
  std::cout << "  • Throughput should be competitive with RDL" << std::endl;
  std::cout << "  • No memory leaks under sustained load" << std::endl;
  std::cout << "  • Scalability independent of event count" << std::endl;
  std::cout << "\n================================================="
            << std::endl;

  return 0;
}
