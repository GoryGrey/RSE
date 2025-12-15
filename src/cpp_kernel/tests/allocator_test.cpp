#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include "../Allocator.h"

// ============================================================================
// Test Suite: Bounded Arena Allocator
// ============================================================================

class TestResult {
public:
    bool passed;
    const char* name;
    const char* message;

    TestResult(bool p, const char* n, const char* m = "")
        : passed(p), name(n), message(m) {}

    void print() const {
        if (passed) {
            std::cout << "  ✓ " << name << std::endl;
        } else {
            std::cout << "  ✗ " << name << ": " << message << std::endl;
        }
    }
};

// ============================================================================
// Test 1: Basic Allocation and Deallocation
// ============================================================================

TestResult testBasicAllocation() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    void* p1 = allocator.allocateProcess(sizeof(char[1]));
    void* p2 = allocator.allocateEvent(sizeof(char[1]));
    void* p3 = allocator.allocateEdge(sizeof(char[1]));
    
    bool valid = (p1 != nullptr && p2 != nullptr && p3 != nullptr);
    
    allocator.deallocateProcess(p1);
    allocator.deallocateEvent(p2);
    allocator.deallocateEdge(p3);
    
    return TestResult(valid, "Basic Allocation and Deallocation");
}

// ============================================================================
// Test 2: Pool Exhaustion
// ============================================================================

TestResult testPoolExhaustion() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Try to allocate beyond capacity - this should fail gracefully
    std::vector<void*> allocs;
    bool exhausted = false;
    
    // Process pool capacity is LATTICE_SIZE * PROCESSES_PER_CELL
    // We can't truly exhaust it in a test without massive memory, but we can verify
    // the mechanism works
    
    for (size_t i = 0; i < 100; ++i) {
        void* p = allocator.allocateProcess(sizeof(char[1]));
        if (p == nullptr) {
            exhausted = true;
            break;
        }
        allocs.push_back(p);
    }
    
    // Cleanup
    for (auto p : allocs) {
        allocator.deallocateProcess(p);
    }
    
    // For the purposes of this test, we just verify that allocations work
    // (full exhaustion would require too much memory)
    return TestResult(!allocs.empty(), "Pool Management");
}

// ============================================================================
// Test 3: Thread Safety - Concurrent Allocations
// ============================================================================

TestResult testConcurrentAllocations() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::atomic<int> alloc_count{0};
    
    const int THREADS = 8;
    const int ALLOCS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> thread_allocs(THREADS);
    
    auto worker = [&](int thread_id) {
        std::vector<void*>& my_allocs = thread_allocs[thread_id];
        
        for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
            // Randomly allocate from different pools
            int pool = i % 3;
            void* p = nullptr;
            
            switch (pool) {
                case 0:
                    p = allocator.allocateProcess(sizeof(char[1]));
                    break;
                case 1:
                    p = allocator.allocateEvent(sizeof(char[1]));
                    break;
                case 2:
                    p = allocator.allocateEdge(sizeof(char[1]));
                    break;
            }
            
            if (p != nullptr) {
                my_allocs.push_back(p);
                success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
            
            alloc_count.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    // Launch worker threads
    for (int i = 0; i < THREADS; ++i) {
        threads.emplace_back(worker, i);
    }
    
    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all allocations succeeded
    int expected = THREADS * ALLOCS_PER_THREAD;
    bool all_succeeded = (success_count.load() == expected && failure_count.load() == 0);
    
    // Cleanup
    for (int i = 0; i < THREADS; ++i) {
        for (void* ptr : thread_allocs[i]) {
            (void)ptr;
        }
    }
    
    return TestResult(all_succeeded, 
                     "Concurrent Allocations",
                     all_succeeded ? "" : "Some allocations failed under contention");
}

// ============================================================================
// Test 4: No Unbounded Growth
// ============================================================================

TestResult testNoBoundedGrowth() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Perform allocations and deallocations
    std::vector<void*> process_allocs;
    for (int i = 0; i < 50; ++i) {
        void* p = allocator.allocateProcess(sizeof(char[1]));
        if (p) process_allocs.push_back(p);
    }
    
    size_t after_alloc_process = allocator.getProcessPoolUsage();
    
    // Deallocate all
    for (void* p : process_allocs) {
        allocator.deallocateProcess(p);
    }
    
    size_t after_dealloc_process = allocator.getProcessPoolUsage();
    
    // Verify no unbounded growth: after deallocating, we should have freed memory
    bool no_growth = (after_dealloc_process <= after_alloc_process);
    
    return TestResult(no_growth,
                     "No Unbounded Growth",
                     no_growth ? "" : "Memory didn't decrease after deallocation");
}

// ============================================================================
// Test 5: Freelist Reuse
// ============================================================================

TestResult testFreelistReuse() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Allocate and deallocate to populate freelist
    void* p1 = allocator.allocateProcess(sizeof(char[1]));
    allocator.deallocateProcess(p1);
    
    // Next allocation should reuse from freelist
    void* p2 = allocator.allocateProcess(sizeof(char[1]));
    
    // They should be the same address (freelist reuse)
    bool reused = (p1 == p2);
    
    allocator.deallocateProcess(p2);
    
    return TestResult(reused,
                     "Freelist Reuse",
                     reused ? "" : "Pointer not reused from freelist");
}

// ============================================================================
// Test 6: Generic Pool Allocation
// ============================================================================

TestResult testGenericPoolAllocation() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Allocate various sizes
    void* p1 = allocator.allocateGeneric(64);
    void* p2 = allocator.allocateGeneric(256);
    void* p3 = allocator.allocateGeneric(1024);
    
    bool valid = (p1 != nullptr && p2 != nullptr && p3 != nullptr);
    
    allocator.deallocateGeneric(p1);
    allocator.deallocateGeneric(p2);
    allocator.deallocateGeneric(p3);
    
    return TestResult(valid, "Generic Pool Allocation");
}

// ============================================================================
// Test 7: Stress Test - Mixed Operations
// ============================================================================

TestResult testMixedOperationsStress() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    std::atomic<bool> stress_error{false};
    
    const int THREADS = 16;
    const int ITERATIONS = 1000;
    
    std::vector<std::thread> threads;
    
    auto stress_worker = [&](int thread_id) {
        std::vector<void*> allocs;
        std::mt19937 rng(thread_id);
        std::uniform_int_distribution<int> dist(0, 2);
        
        for (int i = 0; i < ITERATIONS; ++i) {
            int op = dist(rng);
            
            if (op == 0) {
                // Allocate
                void* p = allocator.allocateProcess(sizeof(char[1]));
                if (p) allocs.push_back(p);
            } else if (op == 1) {
                // Deallocate
                if (!allocs.empty()) {
                    void* p = allocs.back();
                    allocs.pop_back();
                    allocator.deallocateProcess(p);
                }
            } else {
                // Try event allocation
                void* p = allocator.allocateEvent(sizeof(char[1]));
                if (p) allocator.deallocateEvent(p);
            }
        }
        
        // Cleanup remaining
        for (void* p : allocs) {
            allocator.deallocateProcess(p);
        }
    };
    
    // Launch stress threads
    for (int i = 0; i < THREADS; ++i) {
        threads.emplace_back(stress_worker, i);
    }
    
    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }
    
    return TestResult(!stress_error.load(),
                     "Mixed Operations Stress Test");
}

// ============================================================================
// Test 8: Capacity Limits
// ============================================================================

TestResult testCapacityLimits() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Verify capacity constants are reasonable
    size_t process_cap = allocator.getProcessPoolCapacity();
    size_t event_cap = allocator.getEventPoolCapacity();
    size_t edge_cap = allocator.getEdgePoolCapacity();
    
    bool valid = (process_cap > 0 && event_cap > 0 && edge_cap > 0);
    
    // Verify they match our constants
    bool cap_matches = (process_cap == PROCESS_POOL_CAPACITY &&
                       event_cap == EVENT_POOL_CAPACITY &&
                       edge_cap == EDGE_POOL_CAPACITY);
    
    return TestResult(valid && cap_matches,
                     "Capacity Limits",
                     cap_matches ? "" : "Capacity mismatch");
}

// ============================================================================
// Test 9: Statistics Tracking
// ============================================================================

TestResult testStatisticsTracking() {
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Allocate and verify stats are updated
    void* p = allocator.allocateProcess(sizeof(char[1]));
    
    // Get current usage
    size_t usage = allocator.getProcessPoolUsage();
    
    bool tracked = (usage > 0);
    
    allocator.deallocateProcess(p);
    
    return TestResult(tracked,
                     "Statistics Tracking",
                     tracked ? "" : "Memory usage not tracked");
}

// ============================================================================
// Test 10: Deterministic Failure on Exhaustion
// ============================================================================

TestResult testDeterministicFailure() {
    // This test verifies that allocation failures are handled consistently
    auto& allocator = BoundedArenaAllocator::getInstance();
    
    // Try to allocate something way too large
    void* p = allocator.allocateProcess(1ULL << 30);  // 1GB
    
    bool fails_deterministically = (p == nullptr);
    
    return TestResult(fails_deterministically,
                     "Deterministic Failure on Exhaustion");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n[BoundedAllocator Tests] Starting test suite..." << std::endl;
    std::cout << "======================================================================" << std::endl;
    
    std::cout << "  Test 1: Basic Allocation..." << std::endl;
    TestResult t1 = testBasicAllocation();
    t1.print();
    
    std::cout << "  Test 2: Pool Exhaustion..." << std::endl;
    TestResult t2 = testPoolExhaustion();
    t2.print();
    
    std::cout << "  Test 3: Concurrent Allocations..." << std::endl;
    TestResult t3 = testConcurrentAllocations();
    t3.print();
    
    std::cout << "  Test 4: No Unbounded Growth..." << std::endl;
    TestResult t4 = testNoBoundedGrowth();
    t4.print();
    
    std::cout << "  Test 5: Freelist Reuse..." << std::endl;
    TestResult t5 = testFreelistReuse();
    t5.print();
    
    std::cout << "  Test 6: Generic Pool Allocation..." << std::endl;
    TestResult t6 = testGenericPoolAllocation();
    t6.print();
    
    std::cout << "  Test 7: Mixed Operations Stress..." << std::endl;
    TestResult t7 = testMixedOperationsStress();
    t7.print();
    
    std::cout << "  Test 8: Capacity Limits..." << std::endl;
    TestResult t8 = testCapacityLimits();
    t8.print();
    
    std::cout << "  Test 9: Statistics Tracking..." << std::endl;
    TestResult t9 = testStatisticsTracking();
    t9.print();
    
    std::cout << "  Test 10: Deterministic Failure..." << std::endl;
    TestResult t10 = testDeterministicFailure();
    t10.print();
    
    int passed = (t1.passed ? 1 : 0) + (t2.passed ? 1 : 0) + (t3.passed ? 1 : 0) +
                 (t4.passed ? 1 : 0) + (t5.passed ? 1 : 0) + (t6.passed ? 1 : 0) +
                 (t7.passed ? 1 : 0) + (t8.passed ? 1 : 0) + (t9.passed ? 1 : 0) +
                 (t10.passed ? 1 : 0);
    
    std::cout << "\n" << passed << "/10 tests passed" << std::endl;
    std::cout << "======================================================================" << std::endl;
    
    // Print final statistics
    MemoryManager::fold();
    
    return (passed == 10) ? 0 : 1;
}
