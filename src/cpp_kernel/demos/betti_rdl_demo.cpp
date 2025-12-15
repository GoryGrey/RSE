#include "../Allocator.h"
#include "BettiRDLKernel.h"
#include <iostream>


// Betti-RDL Demo: Space-Time Native Computation

int main() {
  std::cout << "=================================================" << std::endl;
  std::cout << "   BETTI-RDL // SPACE-TIME COMPUTATION          " << std::endl;
  std::cout << "=================================================" << std::endl;
  std::cout << "\nCombining:" << std::endl;
  std::cout << "  • Betti: O(1) memory in toroidal space" << std::endl;
  std::cout << "  • RDL: Time-native events with adaptive delays" << std::endl;
  std::cout << "\nResult: Space-time unified computational substrate"
            << std::endl;

  {
    BettiRDLKernel kernel;

    // Test 1: Simple ring topology
    std::cout << "\n[TEST 1] Ring Topology (10 nodes)" << std::endl;
    std::cout << "Creating processes in a ring..." << std::endl;

    // Spawn processes in a ring pattern
    for (int i = 0; i < 10; i++) {
      kernel.spawnProcess(i, 0, 0);
    }

    // Create edges with initial delays
    for (int i = 0; i < 10; i++) {
      kernel.createEdge(i, 0, 0, (i + 1) % 10, 0, 0, 5);
    }

    // Inject initial event
    kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);

    // Run simulation
    int events_processed = kernel.run(1000);
    std::cout << "  Events processed: " << events_processed << std::endl;
  }

  // Test 2: Grid topology
  std::cout << "\n[TEST 2] Grid Topology (5x5)" << std::endl;
  std::cout << "Creating processes in a 2D grid..." << std::endl;

  {
    BettiRDLKernel kernel2;

    // Spawn processes in a grid
    for (int x = 0; x < 5; x++) {
      for (int y = 0; y < 5; y++) {
        kernel2.spawnProcess(x, y, 0);
      }
    }

    // Create edges (right and down neighbors)
    for (int x = 0; x < 5; x++) {
      for (int y = 0; y < 5; y++) {
        kernel2.createEdge(x, y, 0, (x + 1) % 5, y, 0, 3);
        kernel2.createEdge(x, y, 0, x, (y + 1) % 5, 0, 3);
      }
    }

    // Inject events at corners
    kernel2.injectEvent(0, 0, 0, 0, 0, 0, 1);
    kernel2.injectEvent(4, 4, 0, 4, 4, 0, 1);

    // Run simulation
    int events_processed = kernel2.run(5000);
    std::cout << "  Events processed: " << events_processed << std::endl;
  }

  // Test 3: 3D Cube topology
  std::cout << "\n[TEST 3] 3D Cube Topology (4x4x4)" << std::endl;
  std::cout << "Creating processes in a 3D cube..." << std::endl;

  {
    BettiRDLKernel kernel3;

    // Spawn processes in a 3D cube
    for (int x = 0; x < 4; x++) {
      for (int y = 0; y < 4; y++) {
        for (int z = 0; z < 4; z++) {
          kernel3.spawnProcess(x, y, z);
        }
      }
    }

    // Create edges in all 3 dimensions
    for (int x = 0; x < 4; x++) {
      for (int y = 0; y < 4; y++) {
        for (int z = 0; z < 4; z++) {
          kernel3.createEdge(x, y, z, (x + 1) % 4, y, z, 2);
          kernel3.createEdge(x, y, z, x, (y + 1) % 4, z, 2);
          kernel3.createEdge(x, y, z, x, y, (z + 1) % 4, 2);
        }
      }
    }

    // Inject event at origin
    kernel3.injectEvent(0, 0, 0, 0, 0, 0, 1);

    // Run simulation
    int events_processed = kernel3.run(10000);
    std::cout << "  Events processed: " << events_processed << std::endl;
  }

  std::cout << "\n================================================="
            << std::endl;
  std::cout << "   KEY INSIGHTS                                 " << std::endl;
  std::cout << "=================================================" << std::endl;
  std::cout << "\n1. SPATIAL (Betti):" << std::endl;
  std::cout << "   • Processes live in fixed toroidal space" << std::endl;
  std::cout << "   • O(1) memory regardless of events" << std::endl;
  std::cout << "\n2. TEMPORAL (RDL):" << std::endl;
  std::cout << "   • Events carry timestamps" << std::endl;
  std::cout << "   • Delays encode state and adapt" << std::endl;
  std::cout << "\n3. UNIFIED:" << std::endl;
  std::cout << "   • Space-time as single substrate" << std::endl;
  std::cout << "   • Deterministic yet adaptive" << std::endl;
  std::cout << "   • Brain-inspired, machine-verifiable" << std::endl;
  std::cout << "\n================================================="
            << std::endl;

  return 0;
}
