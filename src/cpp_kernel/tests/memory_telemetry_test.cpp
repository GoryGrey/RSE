#include "../Allocator.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

// Simple test to verify MemoryManager telemetry

int main() {
    std::cout << "Running Memory Telemetry Test..." << std::endl;

    // 1. Initial State
    size_t initial_rss = MemoryManager::getSystemRSS();
    size_t initial_peak = MemoryManager::getSystemPeakRSS();

    std::cout << "Initial RSS: " << initial_rss << " bytes" << std::endl;
    std::cout << "Initial Peak: " << initial_peak << " bytes" << std::endl;

    if (initial_rss == 0) {
        std::cerr << "WARNING: RSS is 0. This might be expected if /proc/self/statm is not available or readable." << std::endl;
        // On some restricted environments, this might happen. But we should try to assert if possible.
        // For the purpose of this task, we implemented Linux support.
        #ifdef __linux__
        std::cerr << "FAILURE: RSS should not be 0 on Linux." << std::endl;
        return 1;
        #endif
    }

    if (initial_peak < initial_rss) {
        std::cerr << "FAILURE: Peak RSS (" << initial_peak << ") < Current RSS (" << initial_rss << ")" << std::endl;
        return 1;
    }

    // 2. Allocate memory using Generic Pool (to simulate usage)
    std::cout << "Allocating 10MB..." << std::endl;
    size_t alloc_size = 10 * 1024 * 1024;
    void* ptr = BoundedArenaAllocator::getInstance().allocateGeneric(alloc_size);
    if (!ptr) {
        std::cerr << "FAILURE: Allocation failed." << std::endl;
        return 1;
    }
    // Touch memory to ensure it's resident
    std::memset(ptr, 1, alloc_size);

    size_t rss_after_alloc = MemoryManager::getSystemRSS();
    size_t peak_after_alloc = MemoryManager::getSystemPeakRSS();

    std::cout << "RSS after alloc: " << rss_after_alloc << " bytes" << std::endl;
    std::cout << "Peak after alloc: " << peak_after_alloc << " bytes" << std::endl;

    if (rss_after_alloc <= initial_rss) {
        std::cout << "WARNING: RSS did not increase (OS might handle pages lazily or allocator reused pages). Diff: " << (long long)rss_after_alloc - (long long)initial_rss << std::endl;
    }

    if (peak_after_alloc < rss_after_alloc) {
        std::cerr << "FAILURE: Peak < RSS after alloc." << std::endl;
        return 1;
    }

    // 3. Reset Peak
    std::cout << "Resetting Peak..." << std::endl;
    MemoryManager::resetSystemPeak();
    size_t peak_after_reset = MemoryManager::getSystemPeakRSS();
    size_t rss_after_reset = MemoryManager::getSystemRSS();

    std::cout << "Peak after reset: " << peak_after_reset << " bytes" << std::endl;
    std::cout << "RSS after reset: " << rss_after_reset << " bytes" << std::endl;

    // Peak should be approximately equal to RSS now (might be slightly higher if RSS increased in between)
    // But it should definitely be >= RSS
    if (peak_after_reset < rss_after_reset) {
        std::cerr << "FAILURE: Peak < RSS after reset." << std::endl;
        return 1;
    }
    
    // It should also be <= peak_after_alloc (unless RSS grew)
    // Actually, we reset it, so it should be close to current RSS.
    // If we free memory, RSS might drop, but we didn't free yet.
    
    // 4. Cleanup
    // Generic pool doesn't support individual deallocation in this codebase (arena style), 
    // but let's try calling deallocate just to be correct with API, though it does nothing.
    BoundedArenaAllocator::getInstance().deallocateGeneric(ptr);

    std::cout << "Memory Telemetry Test PASSED" << std::endl;
    return 0;
}
