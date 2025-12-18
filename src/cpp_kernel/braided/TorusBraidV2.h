#pragma once

#include "BraidedKernelV2.h"
#include "ProjectionV2.h"

#include <memory>
#include <iostream>
#include <iomanip>

namespace braided {

/**
 * TorusBraidV2: Enhanced orchestrator for three-torus braided system with Phase 2 features.
 * 
 * New in Phase 2:
 * - Boundary constraint propagation
 * - Corrective event generation
 * - Enhanced consistency metrics
 * - Detailed violation tracking
 */
class TorusBraidV2 {
private:
    // Three tori (heap-allocated to avoid stack overflow)
    std::unique_ptr<BraidedKernelV2> torus_a_;
    std::unique_ptr<BraidedKernelV2> torus_b_;
    std::unique_ptr<BraidedKernelV2> torus_c_;
    
    // Braid configuration
    uint64_t braid_interval_;      // How often to exchange projections
    uint64_t last_braid_tick_ = 0;
    uint64_t braid_cycles_ = 0;
    
    // Phase 2: Metrics
    uint64_t total_boundary_violations_ = 0;
    uint64_t total_global_violations_ = 0;
    uint64_t total_corrective_events_ = 0;
    uint64_t total_projection_exchanges_ = 0;
    
public:
    /**
     * Constructor.
     * @param braid_interval How often to exchange projections (in ticks)
     */
    explicit TorusBraidV2(uint64_t braid_interval = 1000)
        : braid_interval_(braid_interval)
    {
        // Allocate tori on heap
        torus_a_ = std::make_unique<BraidedKernelV2>();
        torus_b_ = std::make_unique<BraidedKernelV2>();
        torus_c_ = std::make_unique<BraidedKernelV2>();
        
        // Set torus IDs
        torus_a_->setTorusId(0);
        torus_b_->setTorusId(1);
        torus_c_->setTorusId(2);
        
        std::cout << "[TorusBraid] Initialized with braid_interval=" << braid_interval << std::endl;
    }
    
    // Access to individual tori
    BraidedKernelV2& getTorusA() { return *torus_a_; }
    BraidedKernelV2& getTorusB() { return *torus_b_; }
    BraidedKernelV2& getTorusC() { return *torus_c_; }
    
    const BraidedKernelV2& getTorusA() const { return *torus_a_; }
    const BraidedKernelV2& getTorusB() const { return *torus_b_; }
    const BraidedKernelV2& getTorusC() const { return *torus_c_; }
    
    /**
     * Run the braided system for a given number of ticks.
     * Automatically performs braid exchanges at configured intervals.
     */
    void run(uint64_t num_ticks) {
        std::cout << "[TorusBraid] Running for " << num_ticks << " ticks..." << std::endl;
        
        for (uint64_t i = 0; i < num_ticks; i++) {
            // Tick all three tori
            torus_a_->tick();
            torus_b_->tick();
            torus_c_->tick();
            
            // Check if it's time for a braid exchange
            uint64_t current_tick = i + 1;
            if (current_tick - last_braid_tick_ >= braid_interval_) {
                performBraidExchange();
                last_braid_tick_ = current_tick;
            }
        }
        
        std::cout << "[TorusBraid] Completed " << num_ticks << " ticks" << std::endl;
        printStatistics();
    }
    
    /**
     * Perform a single braid exchange (A→B→C→A).
     * This is the core of the braided system.
     */
    void performBraidExchange() {
        braid_cycles_++;
        
        std::cout << "\n[TorusBraid] === Braid Exchange #" << braid_cycles_ << " ===" << std::endl;
        
        // Extract projections from all three tori
        ProjectionV2 proj_a = torus_a_->extractProjection();
        ProjectionV2 proj_b = torus_b_->extractProjection();
        ProjectionV2 proj_c = torus_c_->extractProjection();
        
        total_projection_exchanges_ += 3;
        
        // Apply projections cyclically: A→B, B→C, C→A
        std::cout << "[TorusBraid] Applying constraints: A→B, B→C, C→A" << std::endl;
        
        bool success_b = torus_b_->applyConstraint(proj_a);
        bool success_c = torus_c_->applyConstraint(proj_b);
        bool success_a = torus_a_->applyConstraint(proj_c);
        
        // Update metrics
        total_boundary_violations_ += torus_a_->getBoundaryViolations() + 
                                      torus_b_->getBoundaryViolations() + 
                                      torus_c_->getBoundaryViolations();
        
        total_global_violations_ += torus_a_->getGlobalViolations() + 
                                    torus_b_->getGlobalViolations() + 
                                    torus_c_->getGlobalViolations();
        
        total_corrective_events_ += torus_a_->getCorrectiveEvents() + 
                                    torus_b_->getCorrectiveEvents() + 
                                    torus_c_->getCorrectiveEvents();
        
        // Check for critical failures
        if (!success_a || !success_b || !success_c) {
            std::cerr << "[TorusBraid] WARNING: Constraint application failed!" << std::endl;
        }
        
        std::cout << "[TorusBraid] Braid exchange complete" << std::endl;
    }
    
    /**
     * Print comprehensive statistics.
     */
    void printStatistics() const {
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║           TorusBraid Phase 2 Statistics                       ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Braid metrics
        std::cout << "║ Braid Cycles:           " << std::setw(10) << braid_cycles_ << "                          ║" << std::endl;
        std::cout << "║ Projection Exchanges:   " << std::setw(10) << total_projection_exchanges_ << "                          ║" << std::endl;
        std::cout << "║ Braid Interval:         " << std::setw(10) << braid_interval_ << " ticks                   ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Per-torus metrics
        std::cout << "║                        Torus A    Torus B    Torus C          ║" << std::endl;
        std::cout << "║ Events Processed:      " 
                  << std::setw(9) << torus_a_->getEventsProcessed() << "   "
                  << std::setw(9) << torus_b_->getEventsProcessed() << "   "
                  << std::setw(9) << torus_c_->getEventsProcessed() << "        ║" << std::endl;
        
        std::cout << "║ Current Time:          " 
                  << std::setw(9) << torus_a_->getCurrentTime() << "   "
                  << std::setw(9) << torus_b_->getCurrentTime() << "   "
                  << std::setw(9) << torus_c_->getCurrentTime() << "        ║" << std::endl;
        
        std::cout << "║ Boundary Violations:   " 
                  << std::setw(9) << torus_a_->getBoundaryViolations() << "   "
                  << std::setw(9) << torus_b_->getBoundaryViolations() << "   "
                  << std::setw(9) << torus_c_->getBoundaryViolations() << "        ║" << std::endl;
        
        std::cout << "║ Global Violations:     " 
                  << std::setw(9) << torus_a_->getGlobalViolations() << "   "
                  << std::setw(9) << torus_b_->getGlobalViolations() << "   "
                  << std::setw(9) << torus_c_->getGlobalViolations() << "        ║" << std::endl;
        
        std::cout << "║ Corrective Events:     " 
                  << std::setw(9) << torus_a_->getCorrectiveEvents() << "   "
                  << std::setw(9) << torus_b_->getCorrectiveEvents() << "   "
                  << std::setw(9) << torus_c_->getCorrectiveEvents() << "        ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Aggregate Phase 2 metrics
        std::cout << "║ Total Boundary Violations:  " << std::setw(10) << total_boundary_violations_ << "                      ║" << std::endl;
        std::cout << "║ Total Global Violations:    " << std::setw(10) << total_global_violations_ << "                      ║" << std::endl;
        std::cout << "║ Total Corrective Events:    " << std::setw(10) << total_corrective_events_ << "                      ║" << std::endl;
        
        // Calculate violation rate
        if (total_projection_exchanges_ > 0) {
            double boundary_rate = static_cast<double>(total_boundary_violations_) / total_projection_exchanges_;
            double global_rate = static_cast<double>(total_global_violations_) / total_projection_exchanges_;
            
            std::cout << "║ Boundary Violation Rate:    " << std::fixed << std::setprecision(2) 
                      << std::setw(6) << boundary_rate << " per exchange             ║" << std::endl;
            std::cout << "║ Global Violation Rate:      " << std::fixed << std::setprecision(2) 
                      << std::setw(6) << global_rate << " per exchange             ║" << std::endl;
        }
        
        std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
    }
    
    /**
     * Get aggregate metrics.
     */
    uint64_t getTotalBoundaryViolations() const { return total_boundary_violations_; }
    uint64_t getTotalGlobalViolations() const { return total_global_violations_; }
    uint64_t getTotalCorrectiveEvents() const { return total_corrective_events_; }
    uint64_t getBraidCycles() const { return braid_cycles_; }
};

} // namespace braided
