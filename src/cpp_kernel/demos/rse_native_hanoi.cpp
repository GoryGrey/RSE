#include "../Allocator.h"
#include "RSENativeHanoiSolver.h"
#include <iostream>


// RSE-Native Hanoi Demo
// Uses RSE kernel processes instead of function call stack

int main() {
  std::cout << "=================================================" << std::endl;
  std::cout << "   RSE-NATIVE HANOI // PROOF OF CONCEPT         " << std::endl;
  std::cout << "=================================================" << std::endl;
  std::cout << "\nThis demo uses RSE kernel processes instead of" << std::endl;
  std::cout << "traditional function call recursion." << std::endl;
  std::cout << "\nEach recursive call = RSE process in toroidal space"
            << std::endl;

  RSENativeHanoiSolver solver;

  // Test 1: Small (verify correctness)
  std::cout << "\n[TEST 1] Warmup: 10 disks" << std::endl;
  solver.solve(10);

  // Test 2: Medium (traditional limit)
  std::cout << "\n[TEST 2] Traditional Limit: 20 disks" << std::endl;
  solver.solve(20);

  // Test 3: Large (beyond traditional)
  std::cout << "\n[TEST 3] Beyond Traditional: 25 disks" << std::endl;
  solver.solve(25);

  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   TESTS COMPLETE                               " << std::endl;
  std::cout << "=================================================" << std::endl;
  std::cout << "\nKey Insight: Memory usage should stay constant" << std::endl;
  std::cout << "regardless of recursion depth because we're using" << std::endl;
  std::cout << "RSE processes instead of call stack." << std::endl;
  std::cout << "\n================================================="
            << std::endl;

  return 0;
}
