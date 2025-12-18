#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace braided {

/**
 * ProjectionV2: Enhanced projection with constraint information for Phase 2.
 * 
 * New in Phase 2:
 * - Boundary constraints: Expected state at boundary cells
 * - Global constraints: System-wide invariants (event conservation, time sync)
 * - Corrective event support: Information needed to generate corrections
 * 
 * Total size: ~4.7 KB (still O(1), independent of workload size)
 */
struct ProjectionV2 {
    // ========== Phase 1 Fields (unchanged) ==========
    
    // Identity
    uint32_t torus_id;           // 0=A, 1=B, 2=C
    uint64_t timestamp;          // Logical time when projection was created
    
    // Summary statistics
    uint64_t total_events_processed;
    uint64_t current_time;
    uint32_t active_processes;
    uint32_t pending_events;
    uint32_t edge_count;
    
    // Boundary state (x=0 face, 32×32 = 1024 cells)
    static constexpr size_t BOUNDARY_SIZE = 32 * 32;
    std::array<uint32_t, BOUNDARY_SIZE> boundary_states;
    
    // Legacy constraint vector (kept for compatibility)
    static constexpr size_t CONSTRAINT_DIM = 16;
    std::array<int32_t, CONSTRAINT_DIM> constraint_vector;
    
    // ========== Phase 2 New Fields ==========
    
    /**
     * Boundary Constraint: Expected state at a specific boundary cell.
     * Used to enforce consistency between adjacent tori.
     */
    struct BoundaryConstraint {
        uint32_t cell_index;      // Index into boundary_states (0-1023)
        int32_t expected_state;   // Expected state value
        int32_t tolerance;        // Acceptable deviation (±)
        
        bool isActive() const { return cell_index != 0xFFFFFFFF; }
        bool isViolated(int32_t actual_state) const {
            return std::abs(actual_state - expected_state) > tolerance;
        }
        int32_t computeCorrection(int32_t actual_state) const {
            return expected_state - actual_state;
        }
    };
    
    /**
     * Global Constraint: System-wide invariant that must be maintained.
     */
    struct GlobalConstraint {
        enum Type : uint32_t {
            NONE = 0,
            EVENT_CONSERVATION = 1,  // Total events should be conserved
            TIME_SYNC = 2,           // Clocks should be synchronized
            LOAD_BALANCE = 3,        // Load should be balanced
            CUSTOM = 255             // User-defined constraint
        };
        
        Type type;
        int64_t expected_value;
        int64_t tolerance;
        
        bool isActive() const { return type != NONE; }
        bool isViolated(int64_t actual_value) const {
            return std::abs(actual_value - expected_value) > tolerance;
        }
        int64_t computeDeviation(int64_t actual_value) const {
            return actual_value - expected_value;
        }
    };
    
    // Sample boundary constraints (32 out of 1024 cells)
    static constexpr size_t NUM_BOUNDARY_CONSTRAINTS = 32;
    std::array<BoundaryConstraint, NUM_BOUNDARY_CONSTRAINTS> boundary_constraints;
    
    // Global constraints
    static constexpr size_t NUM_GLOBAL_CONSTRAINTS = 4;
    std::array<GlobalConstraint, NUM_GLOBAL_CONSTRAINTS> global_constraints;
    
    // Integrity
    uint64_t state_hash;         // Hash of all fields for consistency checking
    
    // ========== Methods ==========
    
    uint64_t computeHash() const;
    bool verify() const;
    
    // Initialize constraints from boundary states
    void initializeBoundaryConstraints(int32_t default_tolerance = 10);
    void initializeGlobalConstraints();
    
    // Check for violations
    int countBoundaryViolations(const std::array<uint32_t, BOUNDARY_SIZE>& actual_states) const;
    int countGlobalViolations(uint64_t actual_events, uint64_t actual_time) const;
    
    // Serialization (for future network transmission)
    size_t serialize(uint8_t* buffer, size_t buffer_size) const;
    static ProjectionV2 deserialize(const uint8_t* buffer, size_t buffer_size);
};

// ========== Implementation ==========

inline uint64_t ProjectionV2::computeHash() const {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    
    // Hash Phase 1 fields
    hash ^= torus_id;
    hash *= 1099511628211ULL;
    hash ^= timestamp;
    hash *= 1099511628211ULL;
    hash ^= total_events_processed;
    hash *= 1099511628211ULL;
    hash ^= current_time;
    hash *= 1099511628211ULL;
    hash ^= active_processes;
    hash *= 1099511628211ULL;
    hash ^= pending_events;
    hash *= 1099511628211ULL;
    hash ^= edge_count;
    hash *= 1099511628211ULL;
    
    // Hash boundary states (sample every 8th element)
    for (size_t i = 0; i < BOUNDARY_SIZE; i += 8) {
        hash ^= boundary_states[i];
        hash *= 1099511628211ULL;
    }
    
    // Hash legacy constraint vector
    for (size_t i = 0; i < CONSTRAINT_DIM; i++) {
        hash ^= static_cast<uint64_t>(constraint_vector[i]);
        hash *= 1099511628211ULL;
    }
    
    // Hash Phase 2 fields: boundary constraints
    for (size_t i = 0; i < NUM_BOUNDARY_CONSTRAINTS; i++) {
        const auto& bc = boundary_constraints[i];
        hash ^= bc.cell_index;
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(bc.expected_state);
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(bc.tolerance);
        hash *= 1099511628211ULL;
    }
    
    // Hash Phase 2 fields: global constraints
    for (size_t i = 0; i < NUM_GLOBAL_CONSTRAINTS; i++) {
        const auto& gc = global_constraints[i];
        hash ^= static_cast<uint64_t>(gc.type);
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(gc.expected_value);
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(gc.tolerance);
        hash *= 1099511628211ULL;
    }
    
    return hash;
}

inline bool ProjectionV2::verify() const {
    return computeHash() == state_hash;
}

inline void ProjectionV2::initializeBoundaryConstraints(int32_t default_tolerance) {
    // Sample 32 cells evenly from the 1024 boundary cells
    for (size_t i = 0; i < NUM_BOUNDARY_CONSTRAINTS; i++) {
        // Sample every 32nd cell (1024 / 32 = 32)
        uint32_t cell_idx = i * 32;
        
        boundary_constraints[i] = {
            .cell_index = cell_idx,
            .expected_state = static_cast<int32_t>(boundary_states[cell_idx]),
            .tolerance = default_tolerance
        };
    }
}

inline void ProjectionV2::initializeGlobalConstraints() {
    // Constraint 0: Event conservation
    global_constraints[0] = {
        .type = GlobalConstraint::EVENT_CONSERVATION,
        .expected_value = static_cast<int64_t>(total_events_processed),
        .tolerance = 1000  // Allow ±1000 events difference
    };
    
    // Constraint 1: Time synchronization
    global_constraints[1] = {
        .type = GlobalConstraint::TIME_SYNC,
        .expected_value = static_cast<int64_t>(current_time),
        .tolerance = 1000  // Allow ±1000 ticks difference
    };
    
    // Constraint 2: Load balance (active processes)
    global_constraints[2] = {
        .type = GlobalConstraint::LOAD_BALANCE,
        .expected_value = static_cast<int64_t>(active_processes),
        .tolerance = 100  // Allow ±100 processes difference
    };
    
    // Constraint 3: Unused (for future extensions)
    global_constraints[3] = {
        .type = GlobalConstraint::NONE,
        .expected_value = 0,
        .tolerance = 0
    };
}

inline int ProjectionV2::countBoundaryViolations(
    const std::array<uint32_t, BOUNDARY_SIZE>& actual_states) const 
{
    int violations = 0;
    for (const auto& bc : boundary_constraints) {
        if (!bc.isActive()) continue;
        
        int32_t actual = static_cast<int32_t>(actual_states[bc.cell_index]);
        if (bc.isViolated(actual)) {
            violations++;
        }
    }
    return violations;
}

inline int ProjectionV2::countGlobalViolations(
    uint64_t actual_events, uint64_t actual_time) const 
{
    int violations = 0;
    for (const auto& gc : global_constraints) {
        if (!gc.isActive()) continue;
        
        int64_t actual_value = 0;
        switch (gc.type) {
            case GlobalConstraint::EVENT_CONSERVATION:
                actual_value = static_cast<int64_t>(actual_events);
                break;
            case GlobalConstraint::TIME_SYNC:
                actual_value = static_cast<int64_t>(actual_time);
                break;
            default:
                continue;
        }
        
        if (gc.isViolated(actual_value)) {
            violations++;
        }
    }
    return violations;
}

inline size_t ProjectionV2::serialize(uint8_t* buffer, size_t buffer_size) const {
    size_t required_size = sizeof(ProjectionV2);
    if (buffer_size < required_size) {
        return 0;  // Buffer too small
    }
    
    std::memcpy(buffer, this, required_size);
    return required_size;
}

inline ProjectionV2 ProjectionV2::deserialize(const uint8_t* buffer, size_t buffer_size) {
    ProjectionV2 proj;
    size_t required_size = sizeof(ProjectionV2);
    
    if (buffer_size < required_size) {
        // Return invalid projection
        proj.torus_id = 0xFFFFFFFF;
        return proj;
    }
    
    std::memcpy(&proj, buffer, required_size);
    return proj;
}

} // namespace braided
