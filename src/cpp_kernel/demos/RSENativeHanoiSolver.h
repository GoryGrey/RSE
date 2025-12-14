#pragma once
#include "../Allocator.h"
#include "../ToroidalSpace.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <queue>


// RSE-Native Hanoi Solver
// Each recursive call becomes an RSE process in toroidal space

struct HanoiTask {
  int n;       // Number of disks
  int from;    // Source peg
  int to;      // Destination peg
  int aux;     // Auxiliary peg
  int x, y, z; // Position in toroidal space

  HanoiTask(int _n, int _from, int _to, int _aux, int _x, int _y, int _z)
      : n(_n), from(_from), to(_to), aux(_aux), x(_x), y(_y), z(_z) {}
};

class RSENativeHanoiSolver {
private:
  ToroidalSpace<32, 32, 32> space;
  std::queue<HanoiTask *> taskQueue;
  unsigned long long moveCount = 0;
  unsigned long long taskCount = 0;

public:
  RSENativeHanoiSolver() {
    std::cout << "[RSE-HANOI] Initializing toroidal space..." << std::endl;
  }

  void solve(int numDisks) {
    std::cout << "\n[RSE-HANOI] Starting RSE-native Hanoi with " << numDisks
              << " disks..." << std::endl;
    std::cout
        << "[RSE-HANOI] Each recursive call = RSE process in toroidal space"
        << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    size_t memBefore = MemoryManager::getUsedMemory();

    // Initial task at origin
    HanoiTask *initial = new HanoiTask(numDisks, 1, 3, 2, 0, 0, 0);
    taskQueue.push(initial);
    taskCount++;

    // Process tasks (simulates RSE kernel scheduling)
    while (!taskQueue.empty()) {
      HanoiTask *task = taskQueue.front();
      taskQueue.pop();

      if (task->n == 0) {
        delete task;
        continue;
      }

      if (task->n == 1) {
        // Base case: move disk
        moveCount++;
        delete task;
        continue;
      }

      // Recursive case: spawn 3 sub-tasks in toroidal space
      // Each sub-task gets a new position in the torus

      // Task 1: Move n-1 disks from 'from' to 'aux' using 'to'
      int x1 = space.wrap(task->x + 1, 32);
      int y1 = space.wrap(task->y, 32);
      int z1 = space.wrap(task->z, 32);
      HanoiTask *task1 = new HanoiTask(task->n - 1, task->from, task->aux,
                                       task->to, x1, y1, z1);
      taskQueue.push(task1);
      taskCount++;

      // Task 2: Move disk n from 'from' to 'to' (the actual move)
      moveCount++;

      // Task 3: Move n-1 disks from 'aux' to 'to' using 'from'
      int x2 = space.wrap(task->x, 32);
      int y2 = space.wrap(task->y + 1, 32);
      int z2 = space.wrap(task->z, 32);
      HanoiTask *task2 = new HanoiTask(task->n - 1, task->aux, task->to,
                                       task->from, x2, y2, z2);
      taskQueue.push(task2);
      taskCount++;

      delete task;

      // Progress indicator
      if (taskCount % 1000000 == 0) {
        std::cout << "    > Tasks processed: " << taskCount
                  << ", Moves: " << moveCount
                  << ", Queue size: " << taskQueue.size() << std::endl;
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    size_t memAfter = MemoryManager::getUsedMemory();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n[RSE-HANOI] ✓ COMPLETE!" << std::endl;
    std::cout << "    > Total Moves: " << moveCount << std::endl;
    std::cout << "    > Total Tasks: " << taskCount << std::endl;
    std::cout << "    > Time: " << duration.count() << "ms" << std::endl;
    std::cout << "    > Memory Before: " << memBefore << " bytes" << std::endl;
    std::cout << "    > Memory After: " << memAfter << " bytes" << std::endl;
    std::cout << "    > Memory Delta: " << (memAfter - memBefore) << " bytes"
              << std::endl;
    std::cout << "    > Moves/sec: " << (moveCount * 1000.0 / duration.count())
              << std::endl;
    std::cout << "    > Tasks/sec: " << (taskCount * 1000.0 / duration.count())
              << std::endl;
  }

  unsigned long long getMoveCount() const { return moveCount; }
  unsigned long long getTaskCount() const { return taskCount; }
};
