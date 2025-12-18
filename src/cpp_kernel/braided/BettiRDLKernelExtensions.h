#pragma once

#include "../demos/BettiRDLKernel.h"
#include "Projection.h"

#include <cmath>

/**
 * Extensions to BettiRDLKernel for braided system support.
 * 
 * This file adds methods to BettiRDLKernel without modifying the original class.
 * We use inheritance to add braided functionality while maintaining backward compatibility.
 */

namespace braided {

// Forward declaration
class BraidedKernel;

} // namespace braided

// Add braided methods to BettiRDLKernel via inheritance
class BraidedKernel : public BettiRDLKernel {
private:
    uint32_t torus_id_ = 0;
    
public:
    // Set torus ID (0=A, 1=B, 2=C)
    void setTorusId(uint32_t id) { torus_id_ = id; }
    uint32_t getTorusId() const { return torus_id_; }
    
    /**
     * Extract projection of current state.
     * This is a compact summary (O(1) size) for cross-torus communication.
     */
    braided::Projection extractProjection() const {
        braided::Projection proj;
        
        // 1. Identity
        proj.torus_id = torus_id_;
        proj.timestamp = current_time;
        
        // 2. Summary statistics
        proj.total_events_processed = events_processed;
        proj.current_time = current_time;
        proj.active_processes = 0;  // TODO: Get from space
        proj.pending_events = event_queue.size();
        proj.edge_count = edge_count_;
        
        // 3. Boundary state (x=0 face)
        for (int y = 0; y < 32; y++) {
            for (int z = 0; z < 32; z++) {
                proj.boundary_states[y * 32 + z] = extractBoundaryState(0, y, z);
            }
        }
        
        // 4. Constraint vector (domain-specific)
        proj.constraint_vector = extractConstraints();
        
        // 5. Compute hash for integrity
        proj.state_hash = proj.computeHash();
        
        return proj;
    }
    
    /**
     * Apply constraint from another torus.
     * Returns true if successful, false if consistency violation detected.
     */
    bool applyConstraint(const braided::Projection& proj) {
        // 1. Verify projection integrity
        if (!proj.verify()) {
            std::cerr << "[Torus " << torus_id_ << "] Invalid projection from Torus " 
                      << proj.torus_id << std::endl;
            return false;
        }
        
        // 2. Check consistency
        bool consistent = verifyConsistency(proj);
        if (!consistent) {
            // Generate corrective events (future work)
            // For now, just log the inconsistency
            std::cerr << "[Torus " << torus_id_ << "] Consistency violation with Torus " 
                      << proj.torus_id << std::endl;
        }
        
        // 3. Apply boundary constraints (Phase 2 feature)
        // applyBoundaryConstraints(proj.boundary_states);
        
        // 4. Propagate constraint vector (Phase 2 feature)
        // propagateConstraints(proj.constraint_vector);
        
        return consistent;
    }
    
private:
    /**
     * Extract state at boundary cell.
     * For now, we use edge count as a proxy for activity.
     */
    uint32_t extractBoundaryState(int x, int y, int z) const {
        const std::uint32_t node = nodeId(x, y, z);
        
        // Count outgoing edges (proxy for activity)
        uint32_t edge_count = 0;
        for (std::uint32_t idx = out_head_[node]; idx != kInvalidEdge; idx = edges_[idx].next_out) {
            edge_count++;
            if (edge_count >= 255) break;  // Cap at 255 to fit in uint8_t if needed
        }
        
        return edge_count;
    }
    
    /**
     * Extract constraint vector (domain-specific invariants).
     */
    std::array<int32_t, braided::Projection::CONSTRAINT_DIM> extractConstraints() const {
        std::array<int32_t, braided::Projection::CONSTRAINT_DIM> constraints = {};
        
        // Constraint[0]: Total event count (for conservation)
        constraints[0] = static_cast<int32_t>(events_processed % INT32_MAX);
        
        // Constraint[1]: Process count (for load balancing)
        // constraints[1] = static_cast<int32_t>(space.getProcessCount());
        constraints[1] = 0;  // TODO: Get from space
        
        // Constraint[2]: Edge count
        constraints[2] = static_cast<int32_t>(edge_count_);
        
        // Constraint[3]: Current time
        constraints[3] = static_cast<int32_t>(current_time % INT32_MAX);
        
        // [4-15]: Reserved for future use
        
        return constraints;
    }
    
    /**
     * Verify consistency with projection from another torus.
     */
    bool verifyConsistency(const braided::Projection& proj) const {
        // Check 1: Time consistency (our time should be close to theirs)
        int64_t time_diff = static_cast<int64_t>(current_time) - static_cast<int64_t>(proj.current_time);
        if (std::abs(time_diff) > 10000) {
            // Time divergence > 10000 ticks is suspicious
            std::cerr << "[Torus " << torus_id_ << "] Time divergence: " << time_diff << std::endl;
            return false;
        }
        
        // Check 2: Constraint satisfaction
        auto our_constraints = extractConstraints();
        
        // Example: constraint[0] (event count) should grow monotonically
        // (This is torus-specific, so we can't directly compare)
        
        // Check 3: Edge count should be reasonable
        int32_t edge_diff = std::abs(our_constraints[2] - proj.constraint_vector[2]);
        if (edge_diff > 1000) {
            std::cerr << "[Torus " << torus_id_ << "] Edge count divergence: " << edge_diff << std::endl;
            // This is not necessarily an error, just log it
        }
        
        return true;
    }
};

// For backward compatibility, we'll use BraidedKernel in the braided system
// but BettiRDLKernel remains unchanged for single-torus use
