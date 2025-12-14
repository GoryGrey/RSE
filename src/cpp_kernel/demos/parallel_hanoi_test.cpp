#include "../Allocator.h"
#include "HanoiSolver.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>


// Parallel Hanoi Test
// Validates the theory: O(1) memory enables massive parallelism

std::atomic<unsigned long long> totalMoves{0};

void runSolver(int solverId, int numDisks) {
  HanoiSolver solver;

  std::cout << "[Thread " << solverId << "] Starting Hanoi with " << numDisks
            << " disks..." << std::endl;

  solver.solve(numDisks);

  totalMoves += solver.getMoveCount();

  std::cout << "[Thread " << solverId
            << "] Complete! Moves: " << solver.getMoveCount() << std::endl;
}

int main() {
  std::cout << "=================================================" << std::endl;
  std::cout << "   PARALLEL RECURSION TEST // RSE v1.0          " << std::endl;
  std::cout << "=================================================" << std::endl;

  const int NUM_THREADS = 10;
  const int DISKS_PER_SOLVER = 25;

  std::cout << "\n[CONFIG]" << std::endl;
  std::cout << "    > Parallel Solvers: " << NUM_THREADS << std::endl;
  std::cout << "    > Disks per Solver: " << DISKS_PER_SOLVER << std::endl;
  std::cout << "    > Total Problems: " << NUM_THREADS << std::endl;

  // Measure baseline memory
  size_t memoryBefore = MemoryManager::getUsedMemory();
  std::cout << "\n[MEMORY] Before: " << memoryBefore << " bytes" << std::endl;

  // ===== TEST 1: SEQUENTIAL =====
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   TEST 1: SEQUENTIAL EXECUTION                  " << std::endl;
  std::cout << "=================================================" << std::endl;

  totalMoves = 0;
  auto seqStart = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_THREADS; i++) {
    runSolver(i, DISKS_PER_SOLVER);
  }

  auto seqEnd = std::chrono::high_resolution_clock::now();
  auto seqDuration =
      std::chrono::duration_cast<std::chrono::milliseconds>(seqEnd - seqStart);
  unsigned long long seqMoves = totalMoves;

  size_t memoryAfterSeq = MemoryManager::getUsedMemory();

  std::cout << "\n[SEQUENTIAL RESULTS]" << std::endl;
  std::cout << "    > Total Time: " << seqDuration.count() << "ms" << std::endl;
  std::cout << "    > Total Moves: " << seqMoves << std::endl;
  std::cout << "    > Memory After: " << memoryAfterSeq << " bytes"
            << std::endl;
  std::cout << "    > Memory Delta: " << (memoryAfterSeq - memoryBefore)
            << " bytes" << std::endl;

  // ===== TEST 2: PARALLEL =====
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   TEST 2: PARALLEL EXECUTION                    " << std::endl;
  std::cout << "=================================================" << std::endl;

  // Reset
  totalMoves = 0;
  size_t memoryBeforeParallel = MemoryManager::getUsedMemory();

  auto parStart = std::chrono::high_resolution_clock::now();

  // Spawn threads
  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_THREADS; i++) {
    threads.emplace_back(runSolver, i, DISKS_PER_SOLVER);
  }

  // Wait for all to complete
  for (auto &t : threads) {
    t.join();
  }

  auto parEnd = std::chrono::high_resolution_clock::now();
  auto parDuration =
      std::chrono::duration_cast<std::chrono::milliseconds>(parEnd - parStart);
  unsigned long long parMoves = totalMoves;

  size_t memoryAfterParallel = MemoryManager::getUsedMemory();

  std::cout << "\n[PARALLEL RESULTS]" << std::endl;
  std::cout << "    > Total Time: " << parDuration.count() << "ms" << std::endl;
  std::cout << "    > Total Moves: " << parMoves << std::endl;
  std::cout << "    > Memory After: " << memoryAfterParallel << " bytes"
            << std::endl;
  std::cout << "    > Memory Delta: "
            << (memoryAfterParallel - memoryBeforeParallel) << " bytes"
            << std::endl;

  // ===== ANALYSIS =====
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   ANALYSIS                                      " << std::endl;
  std::cout << "=================================================" << std::endl;

  double speedup = (double)seqDuration.count() / parDuration.count();
  double efficiency = (speedup / NUM_THREADS) * 100.0;

  std::cout << "\n[PERFORMANCE]" << std::endl;
  std::cout << "    > Speedup: " << speedup << "x" << std::endl;
  std::cout << "    > Parallel Efficiency: " << efficiency << "%" << std::endl;
  std::cout << "    > Expected Speedup (ideal): " << NUM_THREADS << "x"
            << std::endl;

  std::cout << "\n[MEMORY SCALING]" << std::endl;
  long long seqMemDelta = memoryAfterSeq - memoryBefore;
  long long parMemDelta = memoryAfterParallel - memoryBeforeParallel;
  double memoryScaling = (double)parMemDelta / seqMemDelta;

  std::cout << "    > Sequential Memory Growth: " << seqMemDelta << " bytes"
            << std::endl;
  std::cout << "    > Parallel Memory Growth: " << parMemDelta << " bytes"
            << std::endl;
  std::cout << "    > Memory Scaling Factor: " << memoryScaling << "x"
            << std::endl;
  std::cout << "    > Expected (traditional): " << NUM_THREADS << "x"
            << std::endl;

  // ===== VERDICT =====
  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   VERDICT                                       " << std::endl;
  std::cout << "=================================================" << std::endl;

  bool speedupGood = speedup >= (NUM_THREADS * 0.5); // At least 50% efficiency
  bool memoryGood = memoryScaling < 2.0; // Less than 2x memory growth

  if (speedupGood && memoryGood) {
    std::cout << "\n✓ THEORY VALIDATED!" << std::endl;
    std::cout << "    > Achieved " << speedup << "x speedup with "
              << memoryScaling << "x memory scaling" << std::endl;
    std::cout << "    > O(1) memory enables massive parallelism!" << std::endl;
    std::cout << "\n    READY FOR FULL-SCALE DEMOS." << std::endl;
  } else {
    std::cout << "\n✗ THEORY NEEDS REFINEMENT" << std::endl;
    if (!speedupGood) {
      std::cout << "    > Speedup below expectations (" << speedup << "x vs "
                << NUM_THREADS << "x)" << std::endl;
    }
    if (!memoryGood) {
      std::cout << "    > Memory scaling too high (" << memoryScaling
                << "x vs expected ~1x)" << std::endl;
    }
  }

  std::cout << "\n================================================="
            << std::endl;

  return 0;
}
