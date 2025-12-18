#include "../braided/TorusBraidV4.h"

#include <iostream>
#include <chrono>

using namespace braided;

/**
 * Simple Phase 4 Benchmark - Minimal output, just results
 */

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         RSE PHASE 4 PERFORMANCE BENCHMARK (SIMPLE)       ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // Disable verbose output from kernels
    std::cout << "Creating braided system..." << std::endl;
    
    TorusBraidV4 braid(1000);  // 1000-tick braid interval
    
    // Create workload
    std::cout << "Setting up workload (300 processes, 3000 events per torus)..." << std::endl;
    
    for (int torus = 0; torus < 3; torus++) {
        BraidedKernelV3* kernel = nullptr;
        if (torus == 0) kernel = &braid.getTorusA();
        else if (torus == 1) kernel = &braid.getTorusB();
        else kernel = &braid.getTorusC();
        
        // Create 100 processes per torus
        for (int i = 0; i < 100; i++) {
            int x = i % 10;
            int y = (i / 10) % 10;
            int z = torus * 10;
            kernel->spawnProcess(x, y, z);
            
            if (i > 0) {
                int prev_x = (i - 1) % 10;
                int prev_y = ((i - 1) / 10) % 10;
                kernel->createEdge(prev_x, prev_y, z, x, y, z, 10);
            }
        }
        
        // Inject 1000 events per torus
        for (int i = 0; i < 1000; i++) {
            kernel->injectEvent(i % 10, (i / 10) % 10, torus * 10, 0, 0, torus * 10, i);
        }
    }
    
    std::cout << "Workload created. Starting parallel execution..." << std::endl;
    std::cout << "(Running for 5 seconds - please wait)\n" << std::endl;
    
    // Temporarily redirect cout to suppress verbose output
    std::streambuf* old_cout = std::cout.rdbuf();
    std::cout.rdbuf(nullptr);
    
    // Run for 5 seconds
    braid.runFor(5000);
    
    // Restore cout
    std::cout.rdbuf(old_cout);
    
    std::cout << "\nExecution complete! Printing results...\n" << std::endl;
    
    // Print statistics
    braid.printStatistics();
    
    std::cout << "\n✅ Benchmark complete!" << std::endl;
    
    return 0;
}
