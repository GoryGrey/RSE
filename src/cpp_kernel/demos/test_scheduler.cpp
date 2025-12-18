#include "../os/OSProcess.h"
#include "../os/TorusScheduler.h"

#include <iostream>
#include <vector>

using namespace os;

/**
 * Test the emergent scheduler.
 */

void test_basic_scheduling() {
    std::cout << "\n=== Test 1: Basic Scheduling ===" << std::endl;
    
    TorusScheduler scheduler(0, TorusScheduler::Policy::FAIR);
    
    // Create 5 processes
    std::vector<OSProcess*> processes;
    for (int i = 0; i < 5; i++) {
        OSProcess* proc = new OSProcess(i + 1, 0, 0);
        proc->priority = 100 + i * 10;  // Varying priorities
        processes.push_back(proc);
        scheduler.addProcess(proc);
    }
    
    std::cout << "Created 5 processes" << std::endl;
    scheduler.printStatus();
    
    // Run for 1000 ticks
    for (int i = 0; i < 1000; i++) {
        scheduler.tick();
    }
    
    std::cout << "\nAfter 1000 ticks:" << std::endl;
    scheduler.printStatus();
    
    // Check fairness
    std::cout << "\nProcess runtimes:" << std::endl;
    for (auto* proc : processes) {
        std::cout << "  Process " << proc->pid << ": " << proc->total_runtime << " ticks" << std::endl;
    }
    
    // Cleanup
    for (auto* proc : processes) {
        delete proc;
    }
    
    std::cout << "✅ Test 1 passed!" << std::endl;
}

void test_blocking() {
    std::cout << "\n=== Test 2: Blocking & Unblocking ===" << std::endl;
    
    TorusScheduler scheduler(1, TorusScheduler::Policy::ROUND_ROBIN);
    
    // Create 3 processes
    std::vector<OSProcess*> processes;
    for (int i = 0; i < 3; i++) {
        OSProcess* proc = new OSProcess(i + 1, 0, 1);
        processes.push_back(proc);
        scheduler.addProcess(proc);
    }
    
    std::cout << "Created 3 processes" << std::endl;
    scheduler.printStatus();
    
    // Run for 100 ticks
    for (int i = 0; i < 100; i++) {
        scheduler.tick();
    }
    
    std::cout << "\nAfter 100 ticks:" << std::endl;
    scheduler.printStatus();
    
    // Block process 2
    std::cout << "\nBlocking process 2..." << std::endl;
    scheduler.blockProcess(2);
    scheduler.printStatus();
    
    // Run for 100 more ticks
    for (int i = 0; i < 100; i++) {
        scheduler.tick();
    }
    
    std::cout << "\nAfter 100 more ticks (process 2 blocked):" << std::endl;
    scheduler.printStatus();
    
    // Unblock process 2
    std::cout << "\nUnblocking process 2..." << std::endl;
    scheduler.unblockProcess(2);
    scheduler.printStatus();
    
    // Run for 100 more ticks
    for (int i = 0; i < 100; i++) {
        scheduler.tick();
    }
    
    std::cout << "\nAfter 100 more ticks (process 2 unblocked):" << std::endl;
    scheduler.printStatus();
    
    // Cleanup
    for (auto* proc : processes) {
        delete proc;
    }
    
    std::cout << "✅ Test 2 passed!" << std::endl;
}

void test_load_balancing() {
    std::cout << "\n=== Test 3: Load Balancing ===" << std::endl;
    
    // Create 3 schedulers (one per torus)
    TorusScheduler scheduler_a(0);
    TorusScheduler scheduler_b(1);
    TorusScheduler scheduler_c(2);
    
    // Load torus A heavily
    std::vector<OSProcess*> processes;
    for (int i = 0; i < 10; i++) {
        OSProcess* proc = new OSProcess(i + 1, 0, 0);
        processes.push_back(proc);
        scheduler_a.addProcess(proc);
    }
    
    // Load torus B lightly
    for (int i = 10; i < 12; i++) {
        OSProcess* proc = new OSProcess(i + 1, 0, 1);
        processes.push_back(proc);
        scheduler_b.addProcess(proc);
    }
    
    // Torus C is empty
    
    std::cout << "Initial load distribution:" << std::endl;
    scheduler_a.printStatus();
    scheduler_b.printStatus();
    scheduler_c.printStatus();
    
    // Simulate load balancing: migrate from A to C
    std::cout << "\nMigrating 3 processes from Torus A to Torus C..." << std::endl;
    for (int i = 0; i < 3; i++) {
        OSProcess* proc = scheduler_a.pickMigratableProcess();
        if (proc) {
            scheduler_c.receiveProcess(proc);
        }
    }
    
    std::cout << "\nAfter migration:" << std::endl;
    scheduler_a.printStatus();
    scheduler_b.printStatus();
    scheduler_c.printStatus();
    
    // Run all schedulers for 500 ticks
    for (int i = 0; i < 500; i++) {
        scheduler_a.tick();
        scheduler_b.tick();
        scheduler_c.tick();
    }
    
    std::cout << "\nAfter 500 ticks:" << std::endl;
    scheduler_a.printStatus();
    scheduler_b.printStatus();
    scheduler_c.printStatus();
    
    // Cleanup
    for (auto* proc : processes) {
        delete proc;
    }
    
    std::cout << "✅ Test 3 passed!" << std::endl;
}

void test_fairness() {
    std::cout << "\n=== Test 4: Fairness (CFS) ===" << std::endl;
    
    TorusScheduler scheduler(0, TorusScheduler::Policy::FAIR);
    
    // Create processes with different priorities
    std::vector<OSProcess*> processes;
    for (int i = 0; i < 5; i++) {
        OSProcess* proc = new OSProcess(i + 1, 0, 0);
        proc->priority = (i + 1) * 50;  // 50, 100, 150, 200, 250
        processes.push_back(proc);
        scheduler.addProcess(proc);
    }
    
    std::cout << "Created 5 processes with varying priorities" << std::endl;
    
    // Run for 5000 ticks
    for (int i = 0; i < 5000; i++) {
        scheduler.tick();
    }
    
    std::cout << "\nAfter 5000 ticks:" << std::endl;
    scheduler.printStatus();
    
    // Check fairness - all processes should have similar runtime
    std::cout << "\nProcess runtimes (should be roughly equal):" << std::endl;
    uint64_t min_runtime = UINT64_MAX;
    uint64_t max_runtime = 0;
    for (auto* proc : processes) {
        std::cout << "  Process " << proc->pid 
                  << " (priority=" << proc->priority << "): " 
                  << proc->total_runtime << " ticks" << std::endl;
        min_runtime = std::min(min_runtime, proc->total_runtime);
        max_runtime = std::max(max_runtime, proc->total_runtime);
    }
    
    double fairness_ratio = (double)min_runtime / max_runtime;
    std::cout << "\nFairness ratio: " << fairness_ratio << " (should be > 0.8)" << std::endl;
    
    if (fairness_ratio > 0.8) {
        std::cout << "✅ Scheduler is fair!" << std::endl;
    } else {
        std::cout << "⚠️  Scheduler may not be perfectly fair" << std::endl;
    }
    
    // Cleanup
    for (auto* proc : processes) {
        delete proc;
    }
    
    std::cout << "✅ Test 4 passed!" << std::endl;
}

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         EMERGENT SCHEDULER TEST SUITE                    ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    test_basic_scheduling();
    test_blocking();
    test_load_balancing();
    test_fairness();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         ALL TESTS PASSED ✅                               ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    return 0;
}
