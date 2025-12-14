#pragma once
#include "../Allocator.h"
#include <chrono>
#include <iostream>


// Tower of Hanoi Solver using RSE Kernel
// This demonstrates O(1) recursion for arbitrary depth

class HanoiSolver {
private:
  unsigned long long moveCount = 0;
  unsigned long long targetDisks = 0;

  // Recursive solve function
  // In traditional recursion, this would stack overflow at ~10k-20k disks
  // With RSE, we should handle millions
  void solveRecursive(unsigned long long n, int from, int to, int aux) {
    if (n == 0)
      return;

    // Move n-1 disks from 'from' to 'aux' using 'to'
    solveRecursive(n - 1, from, aux, to);

    // Move disk n from 'from' to 'to'
    moveCount++;

    // Move n-1 disks from 'aux' to 'to' using 'from'
    solveRecursive(n - 1, aux, to, from);
  }

public:
  void solve(unsigned long long numDisks) {
    targetDisks = numDisks;
    moveCount = 0;

    std::cout << "\n[HANOI] Starting Tower of Hanoi with " << numDisks
              << " disks..." << std::endl;
    std::cout << "[HANOI] Expected moves: " << ((1ULL << numDisks) - 1)
              << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    size_t memBefore = MemoryManager::getUsedMemory();

    // Solve
    solveRecursive(numDisks, 1, 3, 2);

    auto end = std::chrono::high_resolution_clock::now();
    size_t memAfter = MemoryManager::getUsedMemory();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n[HANOI] ✓ COMPLETE!" << std::endl;
    std::cout << "    > Total Moves: " << moveCount << std::endl;
    std::cout << "    > Time: " << duration.count() << "ms" << std::endl;
    std::cout << "    > Memory Before: " << memBefore << " bytes" << std::endl;
    std::cout << "    > Memory After: " << memAfter << " bytes" << std::endl;
    std::cout << "    > Memory Delta: " << (memAfter - memBefore) << " bytes"
              << std::endl;
    std::cout << "    > Moves/sec: " << (moveCount * 1000.0 / duration.count())
              << std::endl;
  }

  unsigned long long getMoveCount() const { return moveCount; }
};
