
#pragma once
#include <cstdlib>
#include <iostream>

// BettiOS Memory Manager (Folding Engine Stub)
// Intercepts all 'new' calls to track entropy.

static size_t g_memory_used = 0;

void* operator new(size_t size) {
    // In a real BettiOS, we would verify entropy here before allocating
    if (size == 0) size = 1;
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    
    g_memory_used += size;
    return p;
}

void operator delete(void* p) noexcept {
    // In a real BettiOS, we would 'fold' the freed memory into a singularity (recycle bin)
    std::free(p);
}

void operator delete(void* p, size_t size) noexcept {
    g_memory_used -= size;
    std::free(p);
}

class MemoryManager {
public:
    static size_t getUsedMemory() {
        return g_memory_used;
    }

    static void fold() {
        // Simulation of memory compression
        std::cout << "[Metal] Memory Manager: Folding Entropy..." << std::endl;
        // In prototype, just logging.
    }
};
