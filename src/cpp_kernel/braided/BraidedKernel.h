#pragma once

#include "../demos/BettiRDLKernel.h"
#include "Projection.h"

#include <cstdint>
#ifdef RSE_KERNEL
#include "../os/KernelStubs.h"
#endif

namespace braided {

/**
 * BraidedKernel: Wrapper around BettiRDLKernel that adds braided system support.
 * 
 * This class composes BettiRDLKernel (has-a relationship) rather than inheriting from it,
 * allowing us to add braided functionality without modifying the original class.
 */
class BraidedKernel {
private:
    BettiRDLKernel kernel_;
    uint32_t torus_id_ = 0;
    
public:
    BraidedKernel() = default;
    
    // Forward all BettiRDLKernel methods
    bool spawnProcess(int x, int y, int z) {
        return kernel_.spawnProcess(x, y, z);
    }
    
    bool createEdge(int x1, int y1, int z1, int x2, int y2, int z2, unsigned long long initial_delay) {
        return kernel_.createEdge(x1, y1, z1, x2, y2, z2, initial_delay);
    }
    
    bool injectEvent(int dst_x, int dst_y, int dst_z, int src_x, int src_y, int src_z, int payload) {
        return kernel_.injectEvent(dst_x, dst_y, dst_z, src_x, src_y, src_z, payload);
    }

    void flushPendingEvents() {
        kernel_.flushPendingEvents();
    }
    
    void tick() {
        kernel_.flushPendingEvents();
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
    
    // Braided system support
    void setTorusId(uint32_t id) { torus_id_ = id; }
    uint32_t getTorusId() const { return torus_id_; }
    
    /**
     * Extract projection of current state.
     * This is a compact summary (O(1) size) for cross-torus communication.
     */
    Projection extractProjection() const {
        Projection proj;
        
        // 1. Identity
        proj.torus_id = torus_id_;
        proj.timestamp = kernel_.getCurrentTime();
        
        // 2. Summary statistics
        proj.total_events_processed = kernel_.getEventsProcessed();
        proj.current_time = kernel_.getCurrentTime();
        proj.active_processes = kernel_.getActiveProcessCount();
        proj.pending_events = kernel_.getPendingEventCount();
        proj.edge_count = kernel_.getEdgeCount();
        
        // 3. Boundary state (x=0 face)
        kernel_.fillBoundaryStates(proj.boundary_states.data(),
                                   Projection::BOUNDARY_SIZE);
        
        // 4. Constraint vector (domain-specific)
        proj.constraint_vector = {};
        
        // Constraint[0]: Total event count (for conservation)
        proj.constraint_vector[0] = static_cast<int32_t>(kernel_.getEventsProcessed() % INT32_MAX);
        // Constraint[1]: Active process count
        proj.constraint_vector[1] = static_cast<int32_t>(kernel_.getActiveProcessCount());
        // Constraint[2]: Edge count
        proj.constraint_vector[2] = static_cast<int32_t>(kernel_.getEdgeCount());
        // Constraint[3]: Current time
        proj.constraint_vector[3] = static_cast<int32_t>(kernel_.getCurrentTime() % INT32_MAX);
        // Constraint[4]: Pending event count
        proj.constraint_vector[4] = static_cast<int32_t>(kernel_.getPendingEventCount());
        
        // 5. Compute hash for integrity
        proj.state_hash = proj.computeHash();
        
        return proj;
    }
    
    /**
     * Apply constraint from another torus.
     * Returns true if successful, false if consistency violation detected.
     */
    bool applyConstraint(const Projection& proj) {
        // 1. Verify projection integrity
        if (!proj.verify()) {
            std::cerr << "[Torus " << torus_id_ << "] Invalid projection from Torus " 
                      << proj.torus_id << std::endl;
            return false;
        }
        
        // 2. Check consistency
        bool consistent = verifyConsistency(proj);
        if (!consistent) {
            // For Phase 1, we just log inconsistencies
            // Phase 3 will add corrective events
            std::cerr << "[Torus " << torus_id_ << "] Consistency violation with Torus " 
                      << proj.torus_id << std::endl;
        }
        
        // 3. Apply boundary constraints (Phase 2 feature)
        // Not implemented yet
        
        // 4. Propagate constraint vector (Phase 2 feature)
        // Not implemented yet
        
        return consistent;
    }
    
private:
    /**
     * Verify consistency with projection from another torus.
     */
    bool verifyConsistency(const Projection& proj) const {
        // Check 1: Time consistency (our time should be close to theirs)
        int64_t time_diff = static_cast<int64_t>(kernel_.getCurrentTime()) -
                           static_cast<int64_t>(proj.current_time);
        int64_t abs_diff = time_diff < 0 ? -time_diff : time_diff;
        if (abs_diff > 10000) {
            // Time divergence > 10000 ticks is suspicious
            std::cerr << "[Torus " << torus_id_ << "] Time divergence: " << time_diff << std::endl;
            return false;
        }
        
        // Check 2: Event count should grow monotonically
        // (This is torus-specific, so we can't directly compare)
        
        // For Phase 1, we're lenient with consistency checks
        return true;
    }
};

} // namespace braided
