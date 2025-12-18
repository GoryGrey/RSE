#pragma once

#include "BraidedKernelV2.h"
#include "ProjectionV3.h"

#include <vector>
#include <tuple>

namespace braided {

/**
 * BraidedKernelV3: Enhanced wrapper with Phase 3 failure detection and reconstruction support.
 * 
 * New in Phase 3:
 * - Heartbeat mechanism for failure detection
 * - Health status tracking
 * - Process information extraction for reconstruction
 * - Support for state restoration from projections
 */
class BraidedKernelV3 {
private:
    BettiRDLKernel kernel_;
    uint32_t torus_id_ = 0;
    
    // Phase 2: Metrics
    uint64_t total_boundary_violations_ = 0;
    uint64_t total_global_violations_ = 0;
    uint64_t total_corrective_events_ = 0;
    
    // Phase 3: Heartbeat and health
    uint64_t last_heartbeat_ = 0;
    ProjectionV3::HealthStatus health_status_ = ProjectionV3::HEALTHY;
    
    // Phase 3: Process tracking (for reconstruction)
    std::vector<std::tuple<int, int, int, uint32_t>> active_processes_;
    
public:
    BraidedKernelV3() = default;
    
    // ========== Forward BettiRDLKernel methods ==========
    
    bool spawnProcess(int x, int y, int z) {
        bool success = kernel_.spawnProcess(x, y, z);
        if (success) {
            // Track process for reconstruction
            active_processes_.push_back({x, y, z, 0});
        }
        return success;
    }
    
    bool createEdge(int x1, int y1, int z1, int x2, int y2, int z2, unsigned long long initial_delay) {
        return kernel_.createEdge(x1, y1, z1, x2, y2, z2, initial_delay);
    }
    
    bool injectEvent(int dst_x, int dst_y, int dst_z, int src_x, int src_y, int src_z, int payload) {
        return kernel_.injectEvent(dst_x, dst_y, dst_z, src_x, src_y, src_z, payload);
    }
    
    void tick() {
        kernel_.tick();
    }
    
    int run(int max_events) {
        return kernel_.run(max_events);
    }
    
    unsigned long long getCurrentTime() const {
        return kernel_.getCurrentTime();
    }
    
    unsigned long long getEventsProcessed() const {
        return kernel_.getEventsProcessed();
    }
    
    // ========== Braided system support ==========
    
    void setTorusId(uint32_t id) { torus_id_ = id; }
    uint32_t getTorusId() const { return torus_id_; }
    
    // Phase 2: Metrics
    uint64_t getBoundaryViolations() const { return total_boundary_violations_; }
    uint64_t getGlobalViolations() const { return total_global_violations_; }
    uint64_t getCorrectiveEvents() const { return total_corrective_events_; }
    
    // ========== Phase 3: Heartbeat and Health ==========
    
    /**
     * Update heartbeat timestamp.
     * Should be called on every braid exchange.
     */
    void updateHeartbeat() {
        last_heartbeat_ = kernel_.getCurrentTime();
        if (health_status_ != ProjectionV3::FAILED) {
            health_status_ = ProjectionV3::HEALTHY;
        }
    }
    
    /**
     * Check if this torus is alive.
     * @param current_time Current system time
     * @param timeout Heartbeat timeout (e.g., 3× braid interval)
     * @return true if alive, false if failed
     */
    bool isAlive(uint64_t current_time, uint64_t timeout) const {
        if (health_status_ == ProjectionV3::FAILED) return false;
        return (current_time - last_heartbeat_) < timeout;
    }
    
    /**
     * Mark this torus as failed.
     * Used when external failure is detected.
     */
    void markFailed() {
        health_status_ = ProjectionV3::FAILED;
        std::cerr << "[Torus " << torus_id_ << "] Marked as FAILED" << std::endl;
    }
    
    /**
     * Mark this torus as degraded.
     * Used when performance issues detected.
     */
    void markDegraded() {
        health_status_ = ProjectionV3::DEGRADED;
        std::cout << "[Torus " << torus_id_ << "] Marked as DEGRADED" << std::endl;
    }
    
    /**
     * Get health status.
     */
    ProjectionV3::HealthStatus getHealthStatus() const {
        return health_status_;
    }
    
    /**
     * Get time since last heartbeat.
     */
    uint64_t getTimeSinceHeartbeat(uint64_t current_time) const {
        return current_time - last_heartbeat_;
    }
    
    // ========== Phase 3: Projection with Heartbeat ==========
    
    /**
     * Extract projection of current state (Phase 3 version).
     * Includes heartbeat, health status, and process information.
     */
    ProjectionV3 extractProjection() const {
        ProjectionV3 proj;
        
        // 1. Identity
        proj.torus_id = torus_id_;
        proj.timestamp = kernel_.getCurrentTime();
        
        // 2. Summary statistics
        proj.total_events_processed = kernel_.getEventsProcessed();
        proj.current_time = kernel_.getCurrentTime();
        proj.active_processes = static_cast<uint32_t>(active_processes_.size());
        proj.pending_events = 0;    // Not exposed by BettiRDLKernel
        proj.edge_count = 0;        // Not exposed by BettiRDLKernel
        
        // 3. Boundary state (x=0 face)
        extractBoundaryState(proj.boundary_states);
        
        // 4. Legacy constraint vector
        proj.constraint_vector = {};
        proj.constraint_vector[0] = static_cast<int32_t>(kernel_.getEventsProcessed() % INT32_MAX);
        proj.constraint_vector[3] = static_cast<int32_t>(kernel_.getCurrentTime() % INT32_MAX);
        
        // 5. Phase 2: Initialize boundary and global constraints
        proj.initializeBoundaryConstraints(10);
        proj.initializeGlobalConstraints();
        
        // 6. Phase 3: Heartbeat and health
        proj.heartbeat_timestamp = last_heartbeat_;
        proj.health_status = health_status_;
        
        // 7. Phase 3: Process information
        proj.initializeProcessInfo(active_processes_);
        
        // 8. Compute hash for integrity
        proj.state_hash = proj.computeHash();
        
        return proj;
    }
    
    /**
     * Apply constraint from another torus (Phase 3 version).
     * Includes heartbeat checking.
     */
    bool applyConstraint(const ProjectionV3& proj) {
        // 1. Verify projection integrity
        if (!proj.verify()) {
            std::cerr << "[Torus " << torus_id_ << "] Invalid projection from Torus " 
                      << proj.torus_id << " (hash mismatch)" << std::endl;
            return false;
        }
        
        // 2. Check if source torus is alive
        if (proj.health_status == ProjectionV3::FAILED) {
            std::cerr << "[Torus " << torus_id_ << "] Received projection from FAILED Torus " 
                      << proj.torus_id << std::endl;
            return false;
        }
        
        // 3. Apply boundary constraints
        int boundary_violations = applyBoundaryConstraints(proj);
        total_boundary_violations_ += boundary_violations;
        
        // 4. Check global constraints
        int global_violations = checkGlobalConstraints(proj);
        total_global_violations_ += global_violations;
        
        // 5. Log if violations detected
        if (boundary_violations > 0 || global_violations > 0) {
            std::cout << "[Torus " << torus_id_ << "] Constraints from Torus " << proj.torus_id 
                      << ": " << boundary_violations << " boundary violations, "
                      << global_violations << " global violations" << std::endl;
        }
        
        // Critical violation: too many violations indicates system instability
        if (boundary_violations > 10 || global_violations > 2) {
            std::cerr << "[Torus " << torus_id_ << "] CRITICAL: Too many constraint violations!" << std::endl;
            markDegraded();
            return false;
        }
        
        return true;
    }
    
    // ========== Phase 3: State Restoration ==========
    
    /**
     * Restore state from a projection.
     * Used during torus reconstruction.
     */
    void restoreFromProjection(const ProjectionV3& proj) {
        std::cout << "[Torus " << torus_id_ << "] Restoring from projection..." << std::endl;
        
        // Clear existing state
        active_processes_.clear();
        
        // Restore processes
        for (uint32_t i = 0; i < proj.num_processes; i++) {
            const auto& proc = proj.processes[i];
            if (!proc.isActive()) continue;
            
            spawnProcess(proc.x, proc.y, proc.z);
        }
        
        // Update heartbeat
        last_heartbeat_ = proj.heartbeat_timestamp;
        health_status_ = ProjectionV3::HEALTHY;
        
        std::cout << "[Torus " << torus_id_ << "] Restored " << proj.num_processes << " processes" << std::endl;
    }
    
    /**
     * Get number of active processes.
     */
    size_t getNumActiveProcesses() const {
        return active_processes_.size();
    }
    
    /**
     * Reset kernel to initial state while preserving allocators.
     * Critical for Phase 3 reconstruction - maintains O(1) memory usage.
     */
    void reset() {
        // Reset underlying kernel (preserves allocators)
        kernel_.reset();
        
        // Clear Phase 3 tracking
        active_processes_.clear();
        
        // Reset metrics
        total_boundary_violations_ = 0;
        total_global_violations_ = 0;
        total_corrective_events_ = 0;
        
        // Reset health
        last_heartbeat_ = 0;
        health_status_ = ProjectionV3::HEALTHY;
    }
    
private:
    /**
     * Extract boundary state from kernel (x=0 face).
     */
    void extractBoundaryState(std::array<uint32_t, ProjectionV3::BOUNDARY_SIZE>& boundary_states) const {
        uint64_t time = kernel_.getCurrentTime();
        
        for (int y = 0; y < 32; y++) {
            for (int z = 0; z < 32; z++) {
                int idx = y * 32 + z;
                
                // Heuristic: hash of coordinates and time
                uint64_t hash = (0ULL * 1000000 + y * 1000 + z) ^ time;
                hash = hash * 2654435761ULL;
                
                boundary_states[idx] = static_cast<uint32_t>(hash % 256);
            }
        }
    }
    
    /**
     * Apply boundary constraints from projection.
     */
    int applyBoundaryConstraints(const ProjectionV3& proj) {
        int violations = 0;
        
        std::array<uint32_t, ProjectionV3::BOUNDARY_SIZE> our_boundary;
        extractBoundaryState(our_boundary);
        
        for (const auto& bc : proj.boundary_constraints) {
            if (!bc.isActive()) continue;
            
            uint32_t our_cell_idx = bc.cell_index;
            int32_t our_state = static_cast<int32_t>(our_boundary[our_cell_idx]);
            
            if (bc.isViolated(our_state)) {
                violations++;
                int32_t correction = bc.computeCorrection(our_state);
                generateCorrectiveEvent(our_cell_idx, correction);
            }
        }
        
        return violations;
    }
    
    /**
     * Check global constraints from projection.
     */
    int checkGlobalConstraints(const ProjectionV3& proj) {
        int violations = 0;
        
        for (const auto& gc : proj.global_constraints) {
            if (!gc.isActive()) continue;
            
            int64_t our_value = 0;
            switch (gc.type) {
                case ProjectionV3::GlobalConstraint::EVENT_CONSERVATION:
                    our_value = static_cast<int64_t>(kernel_.getEventsProcessed());
                    break;
                case ProjectionV3::GlobalConstraint::TIME_SYNC:
                    our_value = static_cast<int64_t>(kernel_.getCurrentTime());
                    break;
                default:
                    continue;
            }
            
            if (gc.isViolated(our_value)) {
                violations++;
            }
        }
        
        return violations;
    }
    
    /**
     * Generate corrective event.
     */
    void generateCorrectiveEvent(uint32_t cell_idx, int32_t correction) {
        int y = cell_idx / 32;
        int z = cell_idx % 32;
        int x = 31;
        
        kernel_.injectEvent(x, y, z, x, y, z, correction);
        total_corrective_events_++;
    }
};

} // namespace braided
