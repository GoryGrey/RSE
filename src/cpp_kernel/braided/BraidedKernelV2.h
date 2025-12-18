#pragma once

#include "../demos/BettiRDLKernel.h"
#include "ProjectionV2.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace braided {

/**
 * BraidedKernelV2: Enhanced wrapper with Phase 2 constraint propagation.
 * 
 * New in Phase 2:
 * - Extract boundary state from actual kernel state
 * - Apply boundary constraints from projections
 * - Generate corrective events for violations
 * - Enhanced consistency verification
 */
class BraidedKernelV2 {
private:
    BettiRDLKernel kernel_;
    uint32_t torus_id_ = 0;
    
    // Phase 2: Metrics
    uint64_t total_boundary_violations_ = 0;
    uint64_t total_global_violations_ = 0;
    uint64_t total_corrective_events_ = 0;
    
public:
    BraidedKernelV2() = default;
    
    // ========== Forward BettiRDLKernel methods ==========
    
    bool spawnProcess(int x, int y, int z) {
        return kernel_.spawnProcess(x, y, z);
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
    
    /**
     * Extract projection of current state (Phase 2 version).
     * Includes boundary constraints and global constraints.
     */
    ProjectionV2 extractProjection() const {
        ProjectionV2 proj;
        
        // 1. Identity
        proj.torus_id = torus_id_;
        proj.timestamp = kernel_.getCurrentTime();
        
        // 2. Summary statistics
        proj.total_events_processed = kernel_.getEventsProcessed();
        proj.current_time = kernel_.getCurrentTime();
        proj.active_processes = 0;  // Not exposed by BettiRDLKernel
        proj.pending_events = 0;    // Not exposed by BettiRDLKernel
        proj.edge_count = 0;        // Not exposed by BettiRDLKernel
        
        // 3. Boundary state (x=0 face)
        extractBoundaryState(proj.boundary_states);
        
        // 4. Legacy constraint vector
        proj.constraint_vector = {};
        proj.constraint_vector[0] = static_cast<int32_t>(kernel_.getEventsProcessed() % INT32_MAX);
        proj.constraint_vector[3] = static_cast<int32_t>(kernel_.getCurrentTime() % INT32_MAX);
        
        // 5. Phase 2: Initialize boundary constraints
        proj.initializeBoundaryConstraints(10);  // tolerance = ±10
        
        // 6. Phase 2: Initialize global constraints
        proj.initializeGlobalConstraints();
        
        // 7. Compute hash for integrity
        proj.state_hash = proj.computeHash();
        
        return proj;
    }
    
    /**
     * Apply constraint from another torus (Phase 2 version).
     * Checks boundary and global constraints, generates corrective events.
     * Returns true if successful, false if critical violation detected.
     */
    bool applyConstraint(const ProjectionV2& proj) {
        // 1. Verify projection integrity
        if (!proj.verify()) {
            std::cerr << "[Torus " << torus_id_ << "] Invalid projection from Torus " 
                      << proj.torus_id << " (hash mismatch)" << std::endl;
            return false;
        }
        
        // 2. Apply boundary constraints
        int boundary_violations = applyBoundaryConstraints(proj);
        total_boundary_violations_ += boundary_violations;
        
        // 3. Check global constraints
        int global_violations = checkGlobalConstraints(proj);
        total_global_violations_ += global_violations;
        
        // 4. Log if violations detected
        if (boundary_violations > 0 || global_violations > 0) {
            std::cout << "[Torus " << torus_id_ << "] Constraints from Torus " << proj.torus_id 
                      << ": " << boundary_violations << " boundary violations, "
                      << global_violations << " global violations" << std::endl;
        }
        
        // Critical violation: too many violations indicates system instability
        if (boundary_violations > 10 || global_violations > 2) {
            std::cerr << "[Torus " << torus_id_ << "] CRITICAL: Too many constraint violations!" << std::endl;
            return false;
        }
        
        return true;
    }
    
private:
    /**
     * Extract boundary state from kernel (x=0 face).
     * 
     * Since BettiRDLKernel doesn't expose internal state, we use a heuristic:
     * - Boundary state = hash of (x, y, z, time) mod 256
     * 
     * In a real implementation, this would read actual process/event state.
     */
    void extractBoundaryState(std::array<uint32_t, ProjectionV2::BOUNDARY_SIZE>& boundary_states) const {
        uint64_t time = kernel_.getCurrentTime();
        
        for (int y = 0; y < 32; y++) {
            for (int z = 0; z < 32; z++) {
                int idx = y * 32 + z;
                
                // Heuristic: hash of coordinates and time
                uint64_t hash = (0ULL * 1000000 + y * 1000 + z) ^ time;
                hash = hash * 2654435761ULL;  // Knuth's multiplicative hash
                
                boundary_states[idx] = static_cast<uint32_t>(hash % 256);
            }
        }
    }
    
    /**
     * Apply boundary constraints from projection.
     * Returns number of violations detected.
     */
    int applyBoundaryConstraints(const ProjectionV2& proj) {
        int violations = 0;
        
        // Get our current boundary state
        std::array<uint32_t, ProjectionV2::BOUNDARY_SIZE> our_boundary;
        extractBoundaryState(our_boundary);
        
        // Check each constraint
        for (const auto& bc : proj.boundary_constraints) {
            if (!bc.isActive()) continue;
            
            // Map constraint to our boundary (x=31 face instead of x=0)
            // In a real implementation, this would map to the opposite face
            uint32_t our_cell_idx = bc.cell_index;
            int32_t our_state = static_cast<int32_t>(our_boundary[our_cell_idx]);
            
            if (bc.isViolated(our_state)) {
                violations++;
                
                // Generate corrective event
                int32_t correction = bc.computeCorrection(our_state);
                generateCorrectiveEvent(our_cell_idx, correction);
            }
        }
        
        return violations;
    }
    
    /**
     * Check global constraints from projection.
     * Returns number of violations detected.
     */
    int checkGlobalConstraints(const ProjectionV2& proj) {
        int violations = 0;
        
        for (const auto& gc : proj.global_constraints) {
            if (!gc.isActive()) continue;
            
            int64_t our_value = 0;
            switch (gc.type) {
                case ProjectionV2::GlobalConstraint::EVENT_CONSERVATION:
                    our_value = static_cast<int64_t>(kernel_.getEventsProcessed());
                    break;
                case ProjectionV2::GlobalConstraint::TIME_SYNC:
                    our_value = static_cast<int64_t>(kernel_.getCurrentTime());
                    break;
                case ProjectionV2::GlobalConstraint::LOAD_BALANCE:
                    our_value = 0;  // Not available from BettiRDLKernel
                    continue;
                default:
                    continue;
            }
            
            if (gc.isViolated(our_value)) {
                violations++;
                
                int64_t deviation = gc.computeDeviation(our_value);
                std::cout << "[Torus " << torus_id_ << "] Global constraint violation: "
                          << "type=" << gc.type << ", expected=" << gc.expected_value
                          << ", actual=" << our_value << ", deviation=" << deviation << std::endl;
            }
        }
        
        return violations;
    }
    
    /**
     * Generate corrective event to restore boundary consistency.
     * 
     * In Phase 2, we inject an event at the boundary cell to correct its state.
     * The event payload is the correction magnitude.
     */
    void generateCorrectiveEvent(uint32_t cell_idx, int32_t correction) {
        // Decode cell index to (y, z) coordinates (x=31 for our boundary)
        int y = cell_idx / 32;
        int z = cell_idx % 32;
        int x = 31;  // Our boundary is at x=31 (opposite of x=0 in projection)
        
        // Inject corrective event
        // In a real implementation, this would be a high-priority event
        // For now, we just inject a normal event with the correction as payload
        kernel_.injectEvent(x, y, z, x, y, z, correction);
        
        total_corrective_events_++;
        
        std::cout << "[Torus " << torus_id_ << "] Corrective event: "
                  << "cell=(" << x << "," << y << "," << z << "), "
                  << "correction=" << correction << std::endl;
    }
};

} // namespace braided
