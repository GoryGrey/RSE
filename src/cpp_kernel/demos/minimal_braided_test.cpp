#include "../demos/BettiRDLKernel.h"

#include <iostream>

/**
 * Minimal test: Just create a single BettiRDLKernel and run it
 */

int main() {
    std::cout << "Testing single BettiRDLKernel..." << std::endl;
    
    BettiRDLKernel kernel;
    
    std::cout << "Kernel created successfully" << std::endl;
    
    // Try to spawn a process
    bool ok = kernel.spawnProcess(0, 0, 0);
    std::cout << "Spawn process: " << (ok ? "OK" : "FAILED") << std::endl;
    
    // Try to create an edge
    ok = kernel.createEdge(0, 0, 0, 1, 0, 0, 10);
    std::cout << "Create edge: " << (ok ? "OK" : "FAILED") << std::endl;
    
    // Try to inject an event
    ok = kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);
    std::cout << "Inject event: " << (ok ? "OK" : "FAILED") << std::endl;
    
    // Try to run
    std::cout << "Running kernel..." << std::endl;
    int processed = kernel.run(100);
    std::cout << "Processed " << processed << " events" << std::endl;
    
    std::cout << "Test complete!" << std::endl;
    
    return 0;
}
