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
        proj.timestamp = getCurrentTime();
        
        // 2. Summary statistics
        proj.total_events_processed = getEventsProcessed();
        proj.current_time = getCurrentTime();
        proj.active_processes = getActiveProcessCount();
        proj.pending_events = getPendingEventCount();
        proj.edge_count = getEdgeCount();
        
        // 3. Boundary state (x=0 face)
        fillBoundaryStates(proj.boundary_states.data(), proj.boundary_states.size());
        
        // 4. Constraint vector (domain-specific)
        proj.constraint_vector = buildConstraintVector();
        
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
            std::cerr << "[Torus " << torus_id_ << "] Consistency violation with Torus " 
                      << proj.torus_id << std::endl;
        }
        
        return consistent;
    }
    
private:
    /**
     * Extract constraint vector (domain-specific invariants).
     */
    std::array<int32_t, braided::Projection::CONSTRAINT_DIM> buildConstraintVector() const {
        std::array<int32_t, braided::Projection::CONSTRAINT_DIM> constraints = {};
        
        // Constraint[0]: Total event count (for conservation)
        constraints[0] = static_cast<int32_t>(getEventsProcessed() % INT32_MAX);
        // Constraint[1]: Process count (for load balancing)
        constraints[1] = static_cast<int32_t>(getActiveProcessCount());
        // Constraint[2]: Edge count
        constraints[2] = static_cast<int32_t>(getEdgeCount());
        // Constraint[3]: Current time
        constraints[3] = static_cast<int32_t>(getCurrentTime() % INT32_MAX);
        // Constraint[4]: Pending event count
        constraints[4] = static_cast<int32_t>(getPendingEventCount());
        
        // [4-15]: Reserved for future use
        
        return constraints;
    }
    
    /**
     * Verify consistency with projection from another torus.
     */
    bool verifyConsistency(const braided::Projection& proj) const {
        // Check 1: Time consistency (our time should be close to theirs)
        int64_t time_diff = static_cast<int64_t>(getCurrentTime()) - static_cast<int64_t>(proj.current_time);
        if (std::abs(time_diff) > 10000) {
            // Time divergence > 10000 ticks is suspicious
            std::cerr << "[Torus " << torus_id_ << "] Time divergence: " << time_diff << std::endl;
            return false;
        }
        
        // Check 2: Constraint satisfaction
        auto our_constraints = buildConstraintVector();
        
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
