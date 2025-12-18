#pragma once

#include "BraidCoordinator.h"
#include "BraidedKernel.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace braided {

/**
 * TorusBraid: Main orchestrator for the braided three-torus system.
 * 
 * Manages three BettiRDLKernel instances (tori A, B, C) and coordinates
 * their execution with periodic projection exchange.
 * 
 * Key features:
 * - Runs three tori in parallel (optionally)
 * - Exchanges projections every k ticks (braid interval)
 * - Maintains O(1) memory guarantee (3× single torus)
 * - Provides aggregate statistics
 */
class TorusBraid {
private:
    // The three tori (heap-allocated to avoid stack overflow)
    std::unique_ptr<BraidedKernel> torus_a_;
    std::unique_ptr<BraidedKernel> torus_b_;
    std::unique_ptr<BraidedKernel> torus_c_;
    
    // Braid coordination
    BraidCoordinator coordinator_;
    
    // Configuration
    uint64_t braid_interval_;    // Exchange projections every N ticks
    uint64_t current_tick_;
    
    // Parallelization
    bool parallel_ticks_;        // Run torus ticks in parallel?
    
public:
    /**
     * Constructor
     * @param braid_interval Number of ticks between projection exchanges
     * @param parallel Enable parallel execution of torus ticks
     */
    TorusBraid(uint64_t braid_interval = 1000, bool parallel = false)
        : braid_interval_(braid_interval)
        , current_tick_(0)
        , parallel_ticks_(parallel)
    {
        // Create tori on heap
        torus_a_ = std::make_unique<BraidedKernel>();
        torus_b_ = std::make_unique<BraidedKernel>();
        torus_c_ = std::make_unique<BraidedKernel>();
        
        // Assign unique IDs to tori
        torus_a_->setTorusId(0);
        torus_b_->setTorusId(1);
        torus_c_->setTorusId(2);
        
        std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║   BRAIDED-RSE: Three-Torus Braided System                ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << "[BRAIDED-RSE] Initializing..." << std::endl;
        std::cout << "    > Topology: Three 32³ toroidal lattices (A, B, C)" << std::endl;
        std::cout << "    > Braid interval: " << braid_interval_ << " ticks" << std::endl;
        std::cout << "    > Parallel execution: " << (parallel_ticks_ ? "enabled" : "disabled") << std::endl;
        std::cout << "    > Memory model: O(1) per torus (3× single-torus)" << std::endl;
        std::cout << "    > Coordination: Cyclic projection exchange (A→B→C→A)" << std::endl;
    }
    
    // Access individual tori (for setup)
    BraidedKernel& getTorusA() { return *torus_a_; }
    BraidedKernel& getTorusB() { return *torus_b_; }
    BraidedKernel& getTorusC() { return *torus_c_; }
    
    /**
     * Execute one tick across all three tori.
     * Every braid_interval ticks, performs projection exchange.
     */
    void tick();
    
    /**
     * Execute multiple ticks.
     * @param max_ticks Maximum number of ticks to execute
     */
    void run(int max_ticks);
    
    // Configuration
    void setBraidInterval(uint64_t interval) { braid_interval_ = interval; }
    void setParallelExecution(bool enable) { parallel_ticks_ = enable; }
    
    // Query
    uint64_t getCurrentTick() const { return current_tick_; }
    uint64_t getBraidCycles() const { return coordinator_.getExchangeCount(); }
    
    // Statistics
    void printStatistics() const;
};

inline void TorusBraid::tick() {
    // Run one tick on each torus
    if (parallel_ticks_) {
        // Parallel execution (future optimization)
        std::thread t_a([this]() { torus_a_->tick(); });
        std::thread t_b([this]() { torus_b_->tick(); });
        std::thread t_c([this]() { torus_c_->tick(); });
        
        t_a.join();
        t_b.join();
        t_c.join();
    } else {
        // Sequential execution (simpler, easier to debug)
        torus_a_->tick();
        torus_b_->tick();
        torus_c_->tick();
    }
    
    current_tick_++;
    
    // Every k ticks: braid coordination
    if (current_tick_ % braid_interval_ == 0) {
        coordinator_.exchange(*torus_a_, *torus_b_, *torus_c_);
        
        // Debug output
        if (current_tick_ % (braid_interval_ * 10) == 0) {
            std::cout << "[BRAID] Tick " << current_tick_ 
                      << ", Cycle " << coordinator_.getExchangeCount()
                      << ", Phase: " << coordinator_.getPhaseName()
                      << std::endl;
        }
    }
}

inline void TorusBraid::run(int max_ticks) {
    std::cout << "\n[BRAIDED-RSE] Starting braided execution..." << std::endl;
    std::cout << "    > Max ticks: " << max_ticks << std::endl;
    std::cout << "    > Expected braid cycles: " << (max_ticks / braid_interval_) << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < max_ticks; i++) {
        tick();
        
        // Progress reporting
        if ((i + 1) % 10000 == 0) {
            std::cout << "    > Progress: " << (i + 1) << "/" << max_ticks 
                      << " ticks (" << ((i + 1) * 100 / max_ticks) << "%)"
                      << ", Braid cycles: " << coordinator_.getExchangeCount()
                      << std::endl;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   BRAIDED-RSE: EXECUTION COMPLETE                        ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "[BRAIDED-RSE] Summary:" << std::endl;
    std::cout << "    > Total ticks: " << current_tick_ << std::endl;
    std::cout << "    > Braid cycles: " << coordinator_.getExchangeCount() << std::endl;
    std::cout << "    > Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "    > Ticks/sec: " << (current_tick_ * 1000.0 / duration.count()) << std::endl;
    
    printStatistics();
}

inline void TorusBraid::printStatistics() const {
    std::cout << "\n[BRAIDED-RSE] Detailed Statistics:" << std::endl;
    
    std::cout << "\n  Torus A (ID=0):" << std::endl;
    std::cout << "    > Events processed: " << torus_a_->getEventsProcessed() << std::endl;
    std::cout << "    > Current time: " << torus_a_->getCurrentTime() << std::endl;
    
    std::cout << "\n  Torus B (ID=1):" << std::endl;
    std::cout << "    > Events processed: " << torus_b_->getEventsProcessed() << std::endl;
    std::cout << "    > Current time: " << torus_b_->getCurrentTime() << std::endl;
    
    std::cout << "\n  Torus C (ID=2):" << std::endl;
    std::cout << "    > Events processed: " << torus_c_->getEventsProcessed() << std::endl;
    std::cout << "    > Current time: " << torus_c_->getCurrentTime() << std::endl;
    
    std::cout << "\n  Braid Coordination:" << std::endl;
    std::cout << "    > Total exchanges: " << coordinator_.getTotalExchanges() << std::endl;
    std::cout << "    > Consistency violations: " << coordinator_.getConsistencyViolations() << std::endl;
    std::cout << "    > Current phase: " << coordinator_.getPhaseName() << std::endl;
    
    // Aggregate statistics
    uint64_t total_events = torus_a_->getEventsProcessed() 
                          + torus_b_->getEventsProcessed() 
                          + torus_c_->getEventsProcessed();
    
    std::cout << "\n  Aggregate:" << std::endl;
    std::cout << "    > Total events (all tori): " << total_events << std::endl;
    std::cout << "    > Average per torus: " << (total_events / 3) << std::endl;
    
    // Consistency check
    if (coordinator_.getConsistencyViolations() == 0) {
        std::cout << "\n  ✓ No consistency violations detected" << std::endl;
    } else {
        std::cout << "\n  ⚠ " << coordinator_.getConsistencyViolations() 
                  << " consistency violations detected" << std::endl;
    }
}

} // namespace braided
