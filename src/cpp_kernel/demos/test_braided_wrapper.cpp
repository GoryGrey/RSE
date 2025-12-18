#include "../braided/BraidedKernel.h"

#include <iostream>

/**
 * Test BraidedKernel wrapper
 */

int main() {
    std::cout << "Testing BraidedKernel wrapper..." << std::endl;
    
    braided::BraidedKernel kernel;
    kernel.setTorusId(0);
    
    std::cout << "Kernel created, ID=" << kernel.getTorusId() << std::endl;
    
    // Try to spawn a process
    bool ok = kernel.spawnProcess(0, 0, 0);
    std::cout << "Spawn process: " << (ok ? "OK" : "FAILED") << std::endl;
    
    // Try to create an edge
    ok = kernel.createEdge(0, 0, 0, 1, 0, 0, 10);
    std::cout << "Create edge: " << (ok ? "OK" : "FAILED") << std::endl;
    
    // Try to inject an event
    ok = kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);
    std::cout << "Inject event: " << (ok ? "OK" : "FAILED") << std::endl;
    
    // Extract projection
    std::cout << "Extracting projection..." << std::endl;
    braided::Projection proj = kernel.extractProjection();
    std::cout << "Projection extracted:" << std::endl;
    std::cout << "  - Torus ID: " << proj.torus_id << std::endl;
    std::cout << "  - Timestamp: " << proj.timestamp << std::endl;
    std::cout << "  - Events processed: " << proj.total_events_processed << std::endl;
    std::cout << "  - Hash: " << proj.state_hash << std::endl;
    std::cout << "  - Verified: " << (proj.verify() ? "YES" : "NO") << std::endl;
    
    std::cout << "Test complete!" << std::endl;
    
    return 0;
}
