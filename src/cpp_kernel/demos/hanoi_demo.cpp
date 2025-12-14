#include <iostream>
#include "../Allocator.h"
#include "HanoiSolver.h"

// Demo: Tower of Hanoi Benchmark
// Tests RSE kernel with pure recursive algorithm

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "   TOWER OF HANOI // RSE DEMO // v1.0           " << std::endl;
    std::cout << "=================================================" << std::endl;
    
    HanoiSolver solver;
    
    // Test 1: Small test (verify correctness)
    std::cout << "\n[TEST 1] Warmup: 10 disks" << std::endl;
    solver.solve(10);
    
    // Test 2: Medium test (traditional recursion limit)
    std::cout << "\n[TEST 2] Traditional Limit: 20 disks" << std::endl;
    solver.solve(20);
    
    // Test 3: Large test (would stack overflow traditionally)
    std::cout << "\n[TEST 3] Beyond Traditional: 30 disks" << std::endl;
    solver.solve(30);
    
    // Test 4: Massive test (the real challenge)
    // Note: 40 disks = 1 trillion moves, will take a while
    // For quick demo, we'll do 35 disks = 34 billion moves
    std::cout << "\n[TEST 4] ULTIMATE TEST: 35 disks" << std::endl;
    std::cout << "[WARNING] This will take several minutes..." << std::endl;
    solver.solve(35);
    
    std::cout << "\n=================================================" << std::endl;
    std::cout << "   ALL TESTS COMPLETE                           " << std::endl;
    std::cout << "=================================================" << std::endl;
    
    return 0;
}
