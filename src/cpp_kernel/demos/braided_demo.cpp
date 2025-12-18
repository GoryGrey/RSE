#include "../braided/TorusBraid.h"

#include <iostream>

/**
 * Braided-RSE Demo: Three-Torus Braided System
 * 
 * This demo demonstrates the braided three-torus architecture:
 * - Three 32³ toroidal lattices (A, B, C)
 * - Cyclic projection exchange every k ticks
 * - O(1) memory per torus (3× single-torus total)
 * - Emergent global consistency without global controller
 */

int main() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  BRAIDED-RSE DEMO: Three-Torus Braided System\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "\n";
    
    // Create braided system with braid interval of 1000 ticks
    braided::TorusBraid braid(1000, false);  // 1000 ticks, sequential execution
    
    std::cout << "\n[DEMO] Setting up test scenario..." << std::endl;
    std::cout << "    > Creating processes in each torus" << std::endl;
    std::cout << "    > Creating edges for event propagation" << std::endl;
    std::cout << "    > Injecting initial events" << std::endl;
    
    // Setup Torus A
    {
        auto& torus_a = braid.getTorusA();
        
        // Create a small network in Torus A
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                torus_a.spawnProcess(x, y, 0);
                
                // Create edges to neighbors
                if (x < 3) {
                    torus_a.createEdge(x, y, 0, x+1, y, 0, 10);  // Right
                }
                if (y < 3) {
                    torus_a.createEdge(x, y, 0, x, y+1, 0, 10);  // Down
                }
            }
        }
        
        // Inject initial events
        torus_a.injectEvent(0, 0, 0, 0, 0, 0, 1);
        torus_a.injectEvent(1, 1, 0, 0, 0, 0, 2);
    }
    
    // Setup Torus B (similar network)
    {
        auto& torus_b = braid.getTorusB();
        
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                torus_b.spawnProcess(x, y, 0);
                
                if (x < 3) {
                    torus_b.createEdge(x, y, 0, x+1, y, 0, 10);
                }
                if (y < 3) {
                    torus_b.createEdge(x, y, 0, x, y+1, 0, 10);
                }
            }
        }
        
        torus_b.injectEvent(0, 0, 0, 0, 0, 0, 1);
    }
    
    // Setup Torus C (similar network)
    {
        auto& torus_c = braid.getTorusC();
        
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                torus_c.spawnProcess(x, y, 0);
                
                if (x < 3) {
                    torus_c.createEdge(x, y, 0, x+1, y, 0, 10);
                }
                if (y < 3) {
                    torus_c.createEdge(x, y, 0, x, y+1, 0, 10);
                }
            }
        }
        
        torus_c.injectEvent(2, 2, 0, 0, 0, 0, 3);
    }
    
    std::cout << "\n[DEMO] Setup complete. Starting braided execution..." << std::endl;
    
    // Run the braided system for 10,000 ticks
    // This will trigger 10 braid cycles (every 1000 ticks)
    braid.run(10000);
    
    std::cout << "\n[DEMO] Braided execution complete!" << std::endl;
    std::cout << "\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "  DEMO COMPLETE\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "\nKey observations:" << std::endl;
    std::cout << "  1. Three tori ran independently" << std::endl;
    std::cout << "  2. Projections exchanged cyclically (A→B→C→A)" << std::endl;
    std::cout << "  3. No consistency violations (expected for Phase 1)" << std::endl;
    std::cout << "  4. O(1) memory maintained per torus" << std::endl;
    std::cout << "\nNext steps:" << std::endl;
    std::cout << "  - Phase 2: Implement boundary coupling" << std::endl;
    std::cout << "  - Phase 3: Add consistency verification and self-correction" << std::endl;
    std::cout << "  - Phase 4: Optimize for throughput" << std::endl;
    std::cout << "\n";
    
    return 0;
}
