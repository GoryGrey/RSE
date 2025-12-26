#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace braided {

/**
 * Projection: Compact representation of torus state for cross-torus communication.
 * 
 * Design constraints:
 * - O(1) size: Does not grow with number of processes or events
 * - Serializable: Can be transmitted over network (future)
 * - Verifiable: Includes integrity check (hash)
 * 
 * Total size: ~4.2 KB (constant regardless of workload)
 */
struct Projection {
    // Identity
    uint32_t torus_id;           // 0=A, 1=B, 2=C
    uint64_t timestamp;          // Logical time when projection was created
    
    // Summary statistics (O(1) size)
    uint64_t total_events_processed;
    uint64_t current_time;
    uint32_t active_processes;
    uint32_t pending_events;
    uint32_t edge_count;
    
    // Boundary state (one face of the 32³ cube)
    // We project the x=0 face (32×32 = 1024 cells)
    static constexpr size_t BOUNDARY_SIZE = 32 * 32;
    std::array<uint32_t, BOUNDARY_SIZE> boundary_states;
    
    // Constraint vector (domain-specific invariants)
    // Examples: conservation laws, load balancing targets, etc.
    static constexpr size_t CONSTRAINT_DIM = 16;
    std::array<int32_t, CONSTRAINT_DIM> constraint_vector;
    
    // Integrity
    uint64_t state_hash;         // Hash of critical state for consistency checking
    
    // Methods
    uint64_t computeHash() const;
    bool verify() const;
    
    // Serialization (for future network transmission)
    size_t serialize(uint8_t* buffer, size_t buffer_size) const;
    static Projection deserialize(const uint8_t* buffer, size_t buffer_size);
};

// Compute hash of projection for integrity checking
inline uint64_t Projection::computeHash() const {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    
    // Hash scalar fields
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
    
    // Hash boundary states (sample every 8th element for speed)
    for (size_t i = 0; i < BOUNDARY_SIZE; i += 8) {
        hash ^= boundary_states[i];
        hash *= 1099511628211ULL;
    }
    
    // Hash constraint vector
    for (size_t i = 0; i < CONSTRAINT_DIM; i++) {
        hash ^= static_cast<uint64_t>(constraint_vector[i]);
        hash *= 1099511628211ULL;
    }
    
    return hash;
}

inline bool Projection::verify() const {
    return computeHash() == state_hash;
}

inline void projection_memcpy(void* dst, const void* src, size_t len) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

inline size_t Projection::serialize(uint8_t* buffer, size_t buffer_size) const {
    size_t required_size = sizeof(Projection);
    if (buffer_size < required_size) {
        return 0;  // Buffer too small
    }
    
    projection_memcpy(buffer, this, required_size);
    return required_size;
}

inline Projection Projection::deserialize(const uint8_t* buffer, size_t buffer_size) {
    Projection proj;
    size_t required_size = sizeof(Projection);
    
    if (buffer_size < required_size) {
        // Return invalid projection
        proj.torus_id = 0xFFFFFFFF;
        return proj;
    }
    
    projection_memcpy(&proj, buffer, required_size);
    return proj;
}

} // namespace braided
