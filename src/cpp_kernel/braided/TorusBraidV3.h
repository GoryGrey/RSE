#pragma once

#include "BraidedKernelV3.h"
#include "ProjectionV3.h"

#include <memory>
#include <iostream>
#include <iomanip>
#include <optional>

namespace braided {

/**
 * TorusBraidV3: Self-healing orchestrator with automatic torus reconstruction.
 * 
 * New in Phase 3:
 * - Automatic failure detection via heartbeat timeout
 * - Torus reconstruction from projections (2-of-3 redundancy)
 * - Process migration on failure
 * - Self-healing without manual intervention
 */
class TorusBraidV3 {
private:
    // Three tori (heap-allocated)
    std::unique_ptr<BraidedKernelV3> torus_a_;
    std::unique_ptr<BraidedKernelV3> torus_b_;
    std::unique_ptr<BraidedKernelV3> torus_c_;
    
    // Braid configuration
    uint64_t braid_interval_;
    uint64_t heartbeat_timeout_;  // 3× braid interval
    uint64_t last_braid_tick_ = 0;
    uint64_t braid_cycles_ = 0;
    
    // Last projections (for reconstruction)
    std::optional<ProjectionV3> last_proj_a_;
    std::optional<ProjectionV3> last_proj_b_;
    std::optional<ProjectionV3> last_proj_c_;
    
    // Phase 2: Metrics
    uint64_t total_boundary_violations_ = 0;
    uint64_t total_global_violations_ = 0;
    uint64_t total_corrective_events_ = 0;
    uint64_t total_projection_exchanges_ = 0;
    
    // Phase 3: Metrics
    uint64_t total_failures_detected_ = 0;
    uint64_t total_reconstructions_ = 0;
    uint64_t total_migrations_ = 0;
    
public:
    /**
     * Constructor.
     * @param braid_interval How often to exchange projections (in ticks)
     */
    explicit TorusBraidV3(uint64_t braid_interval = 1000)
        : braid_interval_(braid_interval),
          heartbeat_timeout_(braid_interval * 3)  // 3× braid interval
    {
        // Allocate tori on heap
        torus_a_ = std::make_unique<BraidedKernelV3>();
        torus_b_ = std::make_unique<BraidedKernelV3>();
        torus_c_ = std::make_unique<BraidedKernelV3>();
        
        // Set torus IDs
        torus_a_->setTorusId(0);
        torus_b_->setTorusId(1);
        torus_c_->setTorusId(2);
        
        // Initialize heartbeats
        torus_a_->updateHeartbeat();
        torus_b_->updateHeartbeat();
        torus_c_->updateHeartbeat();
        
        std::cout << "[TorusBraid] Initialized with braid_interval=" << braid_interval 
                  << ", heartbeat_timeout=" << heartbeat_timeout_ << std::endl;
    }
    
    // Access to individual tori
    BraidedKernelV3& getTorusA() { return *torus_a_; }
    BraidedKernelV3& getTorusB() { return *torus_b_; }
    BraidedKernelV3& getTorusC() { return *torus_c_; }
    
    const BraidedKernelV3& getTorusA() const { return *torus_a_; }
    const BraidedKernelV3& getTorusB() const { return *torus_b_; }
    const BraidedKernelV3& getTorusC() const { return *torus_c_; }
    
    /**
     * Run the braided system for a given number of ticks.
     * Automatically performs braid exchanges and failure detection.
     */
    void run(uint64_t num_ticks) {
        std::cout << "[TorusBraid] Running for " << num_ticks << " ticks..." << std::endl;
        
        for (uint64_t i = 0; i < num_ticks; i++) {
            // Tick all three tori (if alive)
            if (torus_a_->getHealthStatus() != ProjectionV3::FAILED) {
                torus_a_->tick();
            }
            if (torus_b_->getHealthStatus() != ProjectionV3::FAILED) {
                torus_b_->tick();
            }
            if (torus_c_->getHealthStatus() != ProjectionV3::FAILED) {
                torus_c_->tick();
            }
            
            // Check if it's time for a braid exchange
            uint64_t current_tick = i + 1;
            if (current_tick - last_braid_tick_ >= braid_interval_) {
                performBraidExchange();
                detectAndRecoverFailures();
                last_braid_tick_ = current_tick;
            }
        }
        
        std::cout << "[TorusBraid] Completed " << num_ticks << " ticks" << std::endl;
        printStatistics();
    }
    
    /**
     * Perform a single braid exchange (A→B→C→A).
     */
    void performBraidExchange() {
        braid_cycles_++;
        
        std::cout << "\n[TorusBraid] === Braid Exchange #" << braid_cycles_ << " ===" << std::endl;
        
        // Extract projections from all three tori
        ProjectionV3 proj_a = torus_a_->extractProjection();
        ProjectionV3 proj_b = torus_b_->extractProjection();
        ProjectionV3 proj_c = torus_c_->extractProjection();
        
        // Store projections for reconstruction
        last_proj_a_ = proj_a;
        last_proj_b_ = proj_b;
        last_proj_c_ = proj_c;
        
        total_projection_exchanges_ += 3;
        
        // Update heartbeats
        torus_a_->updateHeartbeat();
        torus_b_->updateHeartbeat();
        torus_c_->updateHeartbeat();
        
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
     * Detect failures and trigger reconstruction.
     * This is the core of the self-healing system.
     */
    void detectAndRecoverFailures() {
        uint64_t current_time = std::max({
            torus_a_->getCurrentTime(),
            torus_b_->getCurrentTime(),
            torus_c_->getCurrentTime()
        });
        
        // Check each torus (check health status first, then heartbeat)
        bool a_alive = (torus_a_->getHealthStatus() != ProjectionV3::FAILED) && 
                       torus_a_->isAlive(current_time, heartbeat_timeout_);
        bool b_alive = (torus_b_->getHealthStatus() != ProjectionV3::FAILED) && 
                       torus_b_->isAlive(current_time, heartbeat_timeout_);
        bool c_alive = (torus_c_->getHealthStatus() != ProjectionV3::FAILED) && 
                       torus_c_->isAlive(current_time, heartbeat_timeout_);
        
        int alive_count = (a_alive ? 1 : 0) + (b_alive ? 1 : 0) + (c_alive ? 1 : 0);
        
        // Check for failures
        if (!a_alive) {
            bool was_already_failed = (torus_a_->getHealthStatus() == ProjectionV3::FAILED);
            if (!was_already_failed) {
                total_failures_detected_++;
            }
            if (torus_a_->getHealthStatus() != ProjectionV3::FAILED) {
                std::cerr << "\n[TorusBraid] ⚠️  FAILURE DETECTED: Torus A" << std::endl;
                torus_a_->markFailed();
            } else {
                std::cerr << "\n[TorusBraid] ⚠️  FAILURE DETECTED: Torus A (already marked)" << std::endl;
            }
            
            if (alive_count >= 2) {
                reconstructTorusA();
            } else {
                std::cerr << "[TorusBraid] ❌ CRITICAL: Cannot reconstruct (need 2-of-3)" << std::endl;
            }
        }
        
        if (!b_alive) {
            bool was_already_failed = (torus_b_->getHealthStatus() == ProjectionV3::FAILED);
            if (!was_already_failed) {
                total_failures_detected_++;
            }
            if (torus_b_->getHealthStatus() != ProjectionV3::FAILED) {
                std::cerr << "\n[TorusBraid] ⚠️  FAILURE DETECTED: Torus B" << std::endl;
                torus_b_->markFailed();
            } else {
                std::cerr << "\n[TorusBraid] ⚠️  FAILURE DETECTED: Torus B (already marked)" << std::endl;
            }
            
            if (alive_count >= 2) {
                reconstructTorusB();
            } else {
                std::cerr << "[TorusBraid] ❌ CRITICAL: Cannot reconstruct (need 2-of-3)" << std::endl;
            }
        }
        
        if (!c_alive) {
            bool was_already_failed = (torus_c_->getHealthStatus() == ProjectionV3::FAILED);
            if (!was_already_failed) {
                total_failures_detected_++;
            }
            if (torus_c_->getHealthStatus() != ProjectionV3::FAILED) {
                std::cerr << "\n[TorusBraid] ⚠️  FAILURE DETECTED: Torus C" << std::endl;
                torus_c_->markFailed();
            } else {
                std::cerr << "\n[TorusBraid] ⚠️  FAILURE DETECTED: Torus C (already marked)" << std::endl;
            }
            
            if (alive_count >= 2) {
                reconstructTorusC();
            } else {
                std::cerr << "[TorusBraid] ❌ CRITICAL: Cannot reconstruct (need 2-of-3)" << std::endl;
            }
        }
    }
    
    /**
     * Reconstruct Torus A from projections of B and C.
     */
    void reconstructTorusA() {
        std::cout << "[TorusBraid] 🔧 Reconstructing Torus A..." << std::endl;
        
        if (!last_proj_a_.has_value()) {
            std::cerr << "[TorusBraid] ❌ No projection available for Torus A" << std::endl;
            return;
        }
        
        // Migrate processes to B and C before reconstruction
        migrateProcesses(0);
        
        // Reset existing torus (reuse allocator - O(1) memory!)
        torus_a_->reset();
        torus_a_->setTorusId(0);
        
        // Restore from projection
        torus_a_->restoreFromProjection(*last_proj_a_);
        
        std::cout << "[TorusBraid] ✅ Torus A reconstructed successfully (allocator reused)" << std::endl;
        total_reconstructions_++;
    }
    
    /**
     * Reconstruct Torus B from projections of A and C.
     */
    void reconstructTorusB() {
        std::cout << "[TorusBraid] 🔧 Reconstructing Torus B..." << std::endl;
        
        if (!last_proj_b_.has_value()) {
            std::cerr << "[TorusBraid] ❌ No projection available for Torus B" << std::endl;
            return;
        }
        
        migrateProcesses(1);
        
        // Reset existing torus (reuse allocator - O(1) memory!)
        torus_b_->reset();
        torus_b_->setTorusId(1);
        torus_b_->restoreFromProjection(*last_proj_b_);
        
        std::cout << "[TorusBraid] ✅ Torus B reconstructed successfully (allocator reused)" << std::endl;
        total_reconstructions_++;
    }
    
    /**
     * Reconstruct Torus C from projections of A and B.
     */
    void reconstructTorusC() {
        std::cout << "[TorusBraid] 🔧 Reconstructing Torus C..." << std::endl;
        
        if (!last_proj_c_.has_value()) {
            std::cerr << "[TorusBraid] ❌ No projection available for Torus C" << std::endl;
            return;
        }
        
        migrateProcesses(2);
        
        // Reset existing torus (reuse allocator - O(1) memory!)
        torus_c_->reset();
        torus_c_->setTorusId(2);
        torus_c_->restoreFromProjection(*last_proj_c_);
        
        std::cout << "[TorusBraid] ✅ Torus C reconstructed successfully (allocator reused)" << std::endl;
        total_reconstructions_++;
    }
    
    /**
     * Migrate processes from failed torus to surviving tori.
     * @param failed_torus_id ID of failed torus (0=A, 1=B, 2=C)
     */
    void migrateProcesses(uint32_t failed_torus_id) {
        std::cout << "[TorusBraid] 📦 Migrating processes from Torus " << failed_torus_id << "..." << std::endl;
        
        // Get projection of failed torus
        const ProjectionV3* proj = nullptr;
        if (failed_torus_id == 0 && last_proj_a_.has_value()) proj = &(*last_proj_a_);
        else if (failed_torus_id == 1 && last_proj_b_.has_value()) proj = &(*last_proj_b_);
        else if (failed_torus_id == 2 && last_proj_c_.has_value()) proj = &(*last_proj_c_);
        
        if (!proj) {
            std::cerr << "[TorusBraid] ❌ No projection available for migration" << std::endl;
            return;
        }
        
        // Get surviving tori
        std::vector<BraidedKernelV3*> surviving_tori;
        if (failed_torus_id != 0 && torus_a_->getHealthStatus() != ProjectionV3::FAILED) {
            surviving_tori.push_back(torus_a_.get());
        }
        if (failed_torus_id != 1 && torus_b_->getHealthStatus() != ProjectionV3::FAILED) {
            surviving_tori.push_back(torus_b_.get());
        }
        if (failed_torus_id != 2 && torus_c_->getHealthStatus() != ProjectionV3::FAILED) {
            surviving_tori.push_back(torus_c_.get());
        }
        
        if (surviving_tori.empty()) {
            std::cerr << "[TorusBraid] ❌ No surviving tori for migration!" << std::endl;
            return;
        }
        
        // Distribute processes evenly (round-robin)
        uint32_t torus_idx = 0;
        uint32_t migrated = 0;
        
        for (uint32_t i = 0; i < proj->num_processes; i++) {
            const auto& proc = proj->processes[i];
            if (!proc.isActive()) continue;
            
            // Spawn process on surviving torus
            surviving_tori[torus_idx]->spawnProcess(proc.x, proc.y, proc.z);
            
            // Round-robin distribution
            torus_idx = (torus_idx + 1) % surviving_tori.size();
            migrated++;
        }
        
        std::cout << "[TorusBraid] ✅ Migrated " << migrated << " processes to " 
                  << surviving_tori.size() << " surviving tori" << std::endl;
        total_migrations_ += migrated;
    }
    
    /**
     * Simulate a torus failure (for testing).
     * @param torus_id ID of torus to fail (0=A, 1=B, 2=C)
     */
    void simulateFailure(uint32_t torus_id) {
        std::cout << "\n[TorusBraid] 💥 SIMULATING FAILURE: Torus " << torus_id << std::endl;
        
        if (torus_id == 0) {
            torus_a_->markFailed();
        } else if (torus_id == 1) {
            torus_b_->markFailed();
        } else if (torus_id == 2) {
            torus_c_->markFailed();
        }
        
        // Increment failure counter (for testing)
        total_failures_detected_++;
    }
    
    /**
     * Print comprehensive statistics.
     */
    void printStatistics() const {
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║           TorusBraid Phase 3 Statistics                       ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Braid metrics
        std::cout << "║ Braid Cycles:           " << std::setw(10) << braid_cycles_ << "                          ║" << std::endl;
        std::cout << "║ Projection Exchanges:   " << std::setw(10) << total_projection_exchanges_ << "                          ║" << std::endl;
        std::cout << "║ Braid Interval:         " << std::setw(10) << braid_interval_ << " ticks                   ║" << std::endl;
        std::cout << "║ Heartbeat Timeout:      " << std::setw(10) << heartbeat_timeout_ << " ticks                   ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Per-torus metrics
        std::cout << "║                        Torus A    Torus B    Torus C          ║" << std::endl;
        std::cout << "║ Events Processed:      " 
                  << std::setw(9) << torus_a_->getEventsProcessed() << "   "
                  << std::setw(9) << torus_b_->getEventsProcessed() << "   "
                  << std::setw(9) << torus_c_->getEventsProcessed() << "        ║" << std::endl;
        
        std::cout << "║ Health Status:         " 
                  << std::setw(9) << (torus_a_->getHealthStatus() == ProjectionV3::HEALTHY ? "HEALTHY" : 
                                      torus_a_->getHealthStatus() == ProjectionV3::DEGRADED ? "DEGRADED" : "FAILED") << "   "
                  << std::setw(9) << (torus_b_->getHealthStatus() == ProjectionV3::HEALTHY ? "HEALTHY" : 
                                      torus_b_->getHealthStatus() == ProjectionV3::DEGRADED ? "DEGRADED" : "FAILED") << "   "
                  << std::setw(9) << (torus_c_->getHealthStatus() == ProjectionV3::HEALTHY ? "HEALTHY" : 
                                      torus_c_->getHealthStatus() == ProjectionV3::DEGRADED ? "DEGRADED" : "FAILED") << "        ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Phase 3 metrics
        std::cout << "║ Failures Detected:      " << std::setw(10) << total_failures_detected_ << "                          ║" << std::endl;
        std::cout << "║ Reconstructions:        " << std::setw(10) << total_reconstructions_ << "                          ║" << std::endl;
        std::cout << "║ Process Migrations:     " << std::setw(10) << total_migrations_ << "                          ║" << std::endl;
        
        std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
    }
    
    // Getters for metrics
    uint64_t getTotalFailures() const { return total_failures_detected_; }
    uint64_t getTotalReconstructions() const { return total_reconstructions_; }
    uint64_t getTotalMigrations() const { return total_migrations_; }
    uint64_t getBraidCycles() const { return braid_cycles_; }
};

} // namespace braided
