#include "../braided/TorusBraidV4.h"

#include <iostream>
#include <chrono>

using namespace braided;

/**
 * Phase 4 Benchmark: Parallel Execution Performance
 * 
 * Tests:
 * 1. Single-torus baseline (for comparison)
 * 2. Braided-torus V3 (sequential execution)
 * 3. Braided-torus V4 (parallel execution)
 * 4. Adaptive braid interval
 * 5. Scalability analysis
 */

void benchmark_single_torus() {
    std::cout << "\n═══════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  BENCHMARK 1: Single-Torus Baseline" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════\n" << std::endl;
    
    BettiRDLKernel kernel;
    
    // Create a simple network
    for (int i = 0; i < 100; i++) {
        int x = i % 10;
        int y = (i / 10) % 10;
        int z = 0;
        kernel.spawnProcess(x, y, z);
        
        if (i > 0) {
            int prev_x = (i - 1) % 10;
            int prev_y = ((i - 1) / 10) % 10;
            kernel.createEdge(prev_x, prev_y, 0, x, y, z, 10);
        }
    }
    
    // Inject events
    for (int i = 0; i < 1000; i++) {
        kernel.injectEvent(i % 10, (i / 10) % 10, 0, 0, 0, 0, i);
    }
    
    // Run benchmark
    auto start = std::chrono::high_resolution_clock::now();
    int events_processed = kernel.run(100000);
    auto end = std::chrono::high_resolution_clock::now();
    
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double throughput = events_processed / elapsed_sec;
    
    std::cout << "Results:" << std::endl;
    std::cout << "  Events processed: " << events_processed << std::endl;
    std::cout << "  Elapsed time: " << elapsed_sec << " sec" << std::endl;
    std::cout << "  Throughput: " << (throughput / 1e6) << " M events/sec" << std::endl;
    std::cout << "  ✅ Baseline established" << std::endl;
}

void benchmark_braided_v4_parallel() {
    std::cout << "\n═══════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  BENCHMARK 2: Braided-Torus V4 (Parallel Execution)" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV4 braid(1000);  // 1000-tick braid interval
    
    // Create networks in all three tori
    for (int torus = 0; torus < 3; torus++) {
        BraidedKernelV3* kernel = nullptr;
        if (torus == 0) kernel = &braid.getTorusA();
        else if (torus == 1) kernel = &braid.getTorusB();
        else kernel = &braid.getTorusC();
        
        for (int i = 0; i < 100; i++) {
            int x = i % 10;
            int y = (i / 10) % 10;
            int z = torus * 10;  // Spread across z-axis
            kernel->spawnProcess(x, y, z);
            
            if (i > 0) {
                int prev_x = (i - 1) % 10;
                int prev_y = ((i - 1) / 10) % 10;
                int prev_z = z;
                kernel->createEdge(prev_x, prev_y, prev_z, x, y, z, 10);
            }
        }
        
        // Inject events
        for (int i = 0; i < 1000; i++) {
            kernel->injectEvent(i % 10, (i / 10) % 10, torus * 10, 0, 0, torus * 10, i);
        }
    }
    
    // Run parallel benchmark
    std::cout << "Running parallel execution for 10 seconds..." << std::endl;
    braid.runFor(10000);  // 10 seconds
    
    // Print statistics
    braid.printStatistics();
    
    std::cout << "✅ Parallel execution benchmark complete" << std::endl;
}

void benchmark_adaptive_interval() {
    std::cout << "\n═══════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  BENCHMARK 3: Adaptive Braid Interval" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV4 braid(500);  // Start with 500-tick interval
    
    // Create workload with varying violation rates
    for (int torus = 0; torus < 3; torus++) {
        BraidedKernelV3* kernel = nullptr;
        if (torus == 0) kernel = &braid.getTorusA();
        else if (torus == 1) kernel = &braid.getTorusB();
        else kernel = &braid.getTorusC();
        
        // Create a denser network (more potential for violations)
        for (int i = 0; i < 200; i++) {
            int x = i % 20;
            int y = (i / 20) % 10;
            int z = torus * 10;
            kernel->spawnProcess(x, y, z);
            
            // Create multiple edges per process
            if (i > 0) {
                kernel->createEdge((i - 1) % 20, ((i - 1) / 20) % 10, z, x, y, z, 5);
            }
            if (i > 1) {
                kernel->createEdge((i - 2) % 20, ((i - 2) / 20) % 10, z, x, y, z, 8);
            }
        }
        
        // Inject many events
        for (int i = 0; i < 2000; i++) {
            kernel->injectEvent(i % 20, (i / 20) % 10, torus * 10, 0, 0, torus * 10, i);
        }
    }
    
    // Run and observe adaptation
    std::cout << "Running with adaptive braid interval for 15 seconds..." << std::endl;
    std::cout << "Watch the interval adjust based on violation rate!" << std::endl;
    braid.runFor(15000);  // 15 seconds
    
    // Print statistics
    braid.printStatistics();
    
    std::cout << "✅ Adaptive interval benchmark complete" << std::endl;
}

void benchmark_scalability() {
    std::cout << "\n═══════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  BENCHMARK 4: Scalability Analysis" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════\n" << std::endl;
    
    std::cout << "Testing with increasing workload..." << std::endl;
    
    for (int workload_multiplier = 1; workload_multiplier <= 3; workload_multiplier++) {
        std::cout << "\n--- Workload " << workload_multiplier << "× ---" << std::endl;
        
        TorusBraidV4 braid(1000);
        
        for (int torus = 0; torus < 3; torus++) {
            BraidedKernelV3* kernel = nullptr;
            if (torus == 0) kernel = &braid.getTorusA();
            else if (torus == 1) kernel = &braid.getTorusB();
            else kernel = &braid.getTorusC();
            
            int num_processes = 50 * workload_multiplier;
            int num_events = 500 * workload_multiplier;
            
            for (int i = 0; i < num_processes; i++) {
                int x = i % 10;
                int y = (i / 10) % 10;
                int z = torus * 10;
                kernel->spawnProcess(x, y, z);
                
                if (i > 0) {
                    kernel->createEdge((i - 1) % 10, ((i - 1) / 10) % 10, z, x, y, z, 10);
                }
            }
            
            for (int i = 0; i < num_events; i++) {
                kernel->injectEvent(i % 10, (i / 10) % 10, torus * 10, 0, 0, torus * 10, i);
            }
        }
        
        braid.runFor(5000);  // 5 seconds each
        braid.printStatistics();
    }
    
    std::cout << "\n✅ Scalability analysis complete" << std::endl;
}

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                           ║" << std::endl;
    std::cout << "║         RSE PHASE 4 PERFORMANCE BENCHMARK                 ║" << std::endl;
    std::cout << "║                                                           ║" << std::endl;
    std::cout << "║  Goal: Achieve 50M+ events/sec with parallel execution   ║" << std::endl;
    std::cout << "║                                                           ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    try {
        // Benchmark 1: Single-torus baseline
        benchmark_single_torus();
        
        // Benchmark 2: Parallel execution
        benchmark_braided_v4_parallel();
        
        // Benchmark 3: Adaptive interval
        benchmark_adaptive_interval();
        
        // Benchmark 4: Scalability
        benchmark_scalability();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                                                           ║" << std::endl;
        std::cout << "║           ALL BENCHMARKS COMPLETED SUCCESSFULLY           ║" << std::endl;
        std::cout << "║                                                           ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}
