#pragma once

#include "ProjectionV2.h"
#include <vector>

namespace braided {

/**
 * ProjectionV3: Enhanced projection with heartbeat and process information for Phase 3.
 * 
 * New in Phase 3:
 * - Heartbeat timestamp for failure detection
 * - Health status (healthy, degraded, failed)
 * - Process information for reconstruction (sample of active processes)
 * 
 * Total size: ~5.7 KB (still O(1), independent of workload size)
 */
struct ProjectionV3 {
    // ========== Phase 1 & 2 Fields (from ProjectionV2) ==========
    
    uint32_t torus_id;
    uint64_t timestamp;
    uint64_t total_events_processed;
    uint64_t current_time;
    uint32_t active_processes;
    uint32_t pending_events;
    uint32_t edge_count;
    
    static constexpr size_t BOUNDARY_SIZE = 32 * 32;
    std::array<uint32_t, BOUNDARY_SIZE> boundary_states;
    
    static constexpr size_t CONSTRAINT_DIM = 16;
    std::array<int32_t, CONSTRAINT_DIM> constraint_vector;
    
    // Phase 2 fields
    struct BoundaryConstraint {
        uint32_t cell_index;
        int32_t expected_state;
        int32_t tolerance;
        
        bool isActive() const { return cell_index != 0xFFFFFFFF; }
        bool isViolated(int32_t actual_state) const {
            return std::abs(actual_state - expected_state) > tolerance;
        }
        int32_t computeCorrection(int32_t actual_state) const {
            return expected_state - actual_state;
        }
    };
    
    struct GlobalConstraint {
        enum Type : uint32_t {
            NONE = 0,
            EVENT_CONSERVATION = 1,
            TIME_SYNC = 2,
            LOAD_BALANCE = 3,
            CUSTOM = 255
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
    
    static constexpr size_t NUM_BOUNDARY_CONSTRAINTS = 32;
    std::array<BoundaryConstraint, NUM_BOUNDARY_CONSTRAINTS> boundary_constraints;
    
    static constexpr size_t NUM_GLOBAL_CONSTRAINTS = 4;
    std::array<GlobalConstraint, NUM_GLOBAL_CONSTRAINTS> global_constraints;
    
    uint64_t state_hash;
    
    // ========== Phase 3 New Fields ==========
    
    /**
     * Heartbeat: Timestamp of last successful operation.
     * Used for failure detection via timeout.
     */
    uint64_t heartbeat_timestamp = 0;
    
    /**
     * Health Status: Current health of the torus.
     * 0 = Healthy, 1 = Degraded, 2 = Failed
     */
    enum HealthStatus : uint32_t {
        HEALTHY = 0,
        DEGRADED = 1,
        FAILED = 2
    };
    HealthStatus health_status = HEALTHY;
    
    /**
     * Process Information: Sample of active processes for reconstruction.
     * Stores coordinates and basic state of processes.
     */
    struct ProcessInfo {
        uint32_t process_id;  // Unique process ID
        int16_t x, y, z;      // Coordinates in toroidal space
        uint32_t state;       // Process-specific state (activity level, etc.)
        
        bool isActive() const { return process_id != 0xFFFFFFFF; }
    };
    
    // Sample up to 64 processes (out of potentially thousands)
    static constexpr size_t MAX_PROCESSES_IN_PROJECTION = 64;
    std::array<ProcessInfo, MAX_PROCESSES_IN_PROJECTION> processes;
    uint32_t num_processes = 0;
    
    // ========== Methods ==========
    
    // Phase 2 methods
    void initializeBoundaryConstraints(int32_t default_tolerance = 10) {
        for (size_t i = 0; i < NUM_BOUNDARY_CONSTRAINTS; i++) {
            uint32_t cell_idx = i * 32;
            boundary_constraints[i] = {
                .cell_index = cell_idx,
                .expected_state = static_cast<int32_t>(boundary_states[cell_idx]),
                .tolerance = default_tolerance
            };
        }
    }
    
    void initializeGlobalConstraints() {
        global_constraints[0] = {
            .type = GlobalConstraint::EVENT_CONSERVATION,
            .expected_value = static_cast<int64_t>(total_events_processed),
            .tolerance = 1000
        };
        global_constraints[1] = {
            .type = GlobalConstraint::TIME_SYNC,
            .expected_value = static_cast<int64_t>(current_time),
            .tolerance = 1000
        };
        global_constraints[2] = {
            .type = GlobalConstraint::LOAD_BALANCE,
            .expected_value = static_cast<int64_t>(active_processes),
            .tolerance = 100
        };
        global_constraints[3] = {
            .type = GlobalConstraint::NONE,
            .expected_value = 0,
            .tolerance = 0
        };
    }
    
    bool verify() const {
        return computeHash() == state_hash;
    }
    
    // Override computeHash to include Phase 3 fields
    uint64_t computeHash() const {
        // Start with Phase 2 hash (without state_hash field)
        uint64_t hash = 14695981039346656037ULL;
        
        // Hash all Phase 1 & 2 fields
        hash ^= torus_id;
        hash *= 1099511628211ULL;
        hash ^= timestamp;
        hash *= 1099511628211ULL;
        hash ^= total_events_processed;
        hash *= 1099511628211ULL;
        hash ^= current_time;
        hash *= 1099511628211ULL;
        
        // Hash Phase 3 fields: heartbeat
        hash ^= heartbeat_timestamp;
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(health_status);
        hash *= 1099511628211ULL;
        
        // Hash Phase 3 fields: process information (sample)
        for (size_t i = 0; i < std::min(num_processes, 16u); i++) {
            const auto& proc = processes[i];
            hash ^= proc.process_id;
            hash *= 1099511628211ULL;
            hash ^= static_cast<uint64_t>(proc.x) << 32 | 
                    static_cast<uint64_t>(proc.y) << 16 | 
                    static_cast<uint64_t>(proc.z);
            hash *= 1099511628211ULL;
        }
        
        return hash;
    }
    
    // Initialize process information from a list of processes
    void initializeProcessInfo(const std::vector<std::tuple<int, int, int, uint32_t>>& process_list) {
        num_processes = std::min(static_cast<size_t>(process_list.size()), 
                                 static_cast<size_t>(MAX_PROCESSES_IN_PROJECTION));
        
        for (uint32_t i = 0; i < num_processes; i++) {
            const auto& [x, y, z, state] = process_list[i];
            processes[i] = {
                .process_id = i,  // Simple ID for now
                .x = static_cast<int16_t>(x),
                .y = static_cast<int16_t>(y),
                .z = static_cast<int16_t>(z),
                .state = state
            };
        }
        
        // Mark remaining slots as inactive
        for (uint32_t i = num_processes; i < MAX_PROCESSES_IN_PROJECTION; i++) {
            processes[i].process_id = 0xFFFFFFFF;
        }
    }
    
    // Check if torus is alive based on heartbeat
    bool isAlive(uint64_t current_time, uint64_t timeout) const {
        if (health_status == FAILED) return false;
        return (current_time - heartbeat_timestamp) < timeout;
    }
    
    // Get time since last heartbeat
    uint64_t getTimeSinceHeartbeat(uint64_t current_time) const {
        return current_time - heartbeat_timestamp;
    }
};

} // namespace braided
