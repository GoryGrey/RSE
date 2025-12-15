#include "../Allocator.h"
#include "../demos/BettiRDLKernel.h"
#include <iostream>
#include <vector>
#include <cassert>

// Killer Demo Memory Test
// verifies O(1) memory usage during intense simulation

int main() {
    std::cout << "[KillerDemo] Starting Memory Regression Test..." << std::endl;

    // 1. Setup Dense Grid
    BettiRDLKernel kernel;
    const int GRID_SIZE = 10;
    
    std::cout << "[KillerDemo] Spawning " << GRID_SIZE * GRID_SIZE << " processes..." << std::endl;
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            kernel.spawnProcess(x, y, 0);
        }
    }

    std::cout << "[KillerDemo] Creating edges..." << std::endl;
    for (int x = 0; x < GRID_SIZE; x++) {
        for (int y = 0; y < GRID_SIZE; y++) {
            // Connect to neighbors
            kernel.createEdge(x, y, 0, (x + 1) % GRID_SIZE, y, 0, 1);
            kernel.createEdge(x, y, 0, x, (y + 1) % GRID_SIZE, 0, 1);
        }
    }

    // Inject initial events to start the chain reaction
    std::cout << "[KillerDemo] Injecting initial events..." << std::endl;
    for (int i = 0; i < GRID_SIZE; i++) {
        kernel.injectEvent(i, i, 0, i, i, 0, 1);
    }

    // 2. Warmup
    std::cout << "[KillerDemo] Warming up (1000 steps)..." << std::endl;
    kernel.run(1000);

    // 3. Measure Baseline
    size_t memory_after_warmup = MemoryManager::getTotalUsedMemory();
    size_t rss_after_warmup = MemoryManager::getSystemRSS();
    
    std::cout << "[KillerDemo] Baseline Memory Usage:" << std::endl;
    std::cout << "  Internal Used: " << memory_after_warmup << " bytes" << std::endl;
    std::cout << "  System RSS:    " << rss_after_warmup << " bytes" << std::endl;

    // 4. Heavy Load
    std::cout << "[KillerDemo] Running heavy load (50000 steps)..." << std::endl;
    int events = kernel.run(50000);
    std::cout << "[KillerDemo] Processed " << events << " events." << std::endl;

    // 5. Measure After Load
    size_t memory_after_load = MemoryManager::getTotalUsedMemory();
    size_t rss_after_load = MemoryManager::getSystemRSS();

    std::cout << "[KillerDemo] Post-Load Memory Usage:" << std::endl;
    std::cout << "  Internal Used: " << memory_after_load << " bytes" << std::endl;
    std::cout << "  System RSS:    " << rss_after_load << " bytes" << std::endl;

    // 6. Assertions
    // Internal memory usage should be stable (bounded).
    // It might fluctuate slightly depending on the exact number of pending events at the moment we stopped.
    // But it should not have grown proportionally to the 50,000 steps.
    
    // We allow some fluctuation, but if it doubled, that's likely a leak.
    // Actually, since we use object pools, if we leaked events, the pool would eventually fill up and crash/reject.
    // But we want to ensure we are not leaking generic memory or anything else.

    long diff = (long)memory_after_load - (long)memory_after_warmup;
    std::cout << "[KillerDemo] Internal Memory Delta: " << diff << " bytes" << std::endl;

    // If diff is massive (e.g. > 1MB for just events), fail.
    // An event is 32 bytes. 10,000 leaked events would be 320KB.
    if (diff > 500 * 1024) { 
        std::cerr << "FAILURE: Internal memory grew significantly (" << diff << " bytes)." << std::endl;
        return 1;
    }

    // RSS Check
    long rss_diff = (long)rss_after_load - (long)rss_after_warmup;
    std::cout << "[KillerDemo] RSS Delta: " << rss_diff << " bytes" << std::endl;
    
    // RSS is trickier because of allocator fragmentation/OS paging.
    // But with the BoundedArenaAllocator, we expect the arena to be touched and resident.
    // If it grows significantly, it means we are allocating outside the arena (e.g. malloc) and leaking.
    
    if (rss_diff > 2 * 1024 * 1024) { // 2MB tolerance
        std::cerr << "FAILURE: RSS grew significantly (" << rss_diff << " bytes)." << std::endl;
        return 1;
    }

    std::cout << "[KillerDemo] SUCCESS: Memory usage is stable (O(1))." << std::endl;
    
    // Print stats for debug
    BoundedArenaAllocator::getInstance().printAllStats();

    return 0;
}
