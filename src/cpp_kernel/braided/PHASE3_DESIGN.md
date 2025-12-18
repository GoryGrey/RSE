# Phase 3: Self-Correction Design

**Status**: In Development  
**Date**: December 18, 2025

---

## Overview

Phase 3 implements **automatic torus reconstruction** and **self-healing** capabilities. When a torus fails, the system detects the failure and reconstructs the torus from the projections of the other two, achieving **true fault tolerance**.

**Key Insight**: With three tori in a braided system, any two can reconstruct the third. This is the **2-of-3 redundancy** that makes the system resilient.

---

## Core Concepts

### 1. Failure Detection

**Problem**: How do we know when a torus has failed?

**Solution**: Heartbeat mechanism with timeout.

Each torus maintains a **heartbeat timestamp** that's updated on every braid exchange. If a torus doesn't update its heartbeat within a timeout period, it's considered failed.

**Heartbeat Properties**:
- **Frequency**: Updated every braid exchange
- **Timeout**: 3× braid interval (e.g., 3000 ticks for 1000-tick interval)
- **Detection Latency**: Maximum 1 braid interval after failure

**Example**:
```
Braid interval: 1000 ticks
Timeout: 3000 ticks

Torus A: Last heartbeat at tick 5000
Torus B: Last heartbeat at tick 5000
Torus C: Last heartbeat at tick 2000  ← FAILED (3000 ticks ago)

→ Torus C is detected as failed at tick 5000
→ Reconstruction begins immediately
```

### 2. Torus Reconstruction

**Problem**: How do we rebuild a failed torus?

**Solution**: Merge projections from the other two tori.

When a torus fails, the other two tori have **recent projections** of its state. We can reconstruct the failed torus by:
1. Taking the most recent projection from each surviving torus
2. Merging their information (average, interpolate, or use most recent)
3. Initializing a new torus with the merged state

**Reconstruction Process**:
```
Torus C fails

Step 1: Get last projections of C from A and B
  - Projection from A: C's state at tick 4900
  - Projection from B: C's state at tick 4950

Step 2: Merge projections (use most recent)
  - Use B's projection (more recent)

Step 3: Initialize new Torus C
  - Set time = 4950
  - Set events_processed = projection.total_events_processed
  - Restore boundary state
  - Spawn processes (if possible)

Step 4: Resume execution
  - New Torus C joins braid at tick 5000
  - Continues from reconstructed state
```

**Limitations**:
- Can only reconstruct **state visible in projections**
- Cannot reconstruct **internal kernel state** (event queue, process details)
- **Best-effort reconstruction**: May lose some events/processes

### 3. Process Migration

**Problem**: What happens to processes on the failed torus?

**Solution**: Migrate them to surviving tori.

When a torus fails, its processes need to be moved to other tori. We can:
1. **Redistribute processes** evenly across surviving tori
2. **Preserve process IDs** (if possible)
3. **Re-create edges** between processes

**Migration Strategy**:
```
Torus C has 100 processes

Option 1: Even distribution
  - 50 processes → Torus A
  - 50 processes → Torus B

Option 2: Load-based distribution
  - If A has 50 processes, B has 150 processes
  - 75 processes → Torus A (total 125)
  - 25 processes → Torus B (total 175)

Option 3: Spatial distribution
  - Processes at x<16 → Torus A
  - Processes at x≥16 → Torus B
```

**For Phase 3, we'll use Option 1 (even distribution)** for simplicity.

### 4. Self-Healing

**Problem**: How does the system recover automatically?

**Solution**: Orchestrator detects failure and triggers reconstruction.

The `TorusBraidV3` orchestrator monitors all tori and:
1. Detects failures via heartbeat timeout
2. Triggers reconstruction automatically
3. Migrates processes to surviving tori
4. Resumes execution with 2 tori (or reconstructed 3rd)

**Self-Healing Properties**:
- **Automatic**: No manual intervention required
- **Fast**: Detection + reconstruction < 1 second
- **Transparent**: Applications continue running (with possible hiccup)

---

## Design

### 1. Enhanced Projection with Heartbeat

Extend `ProjectionV2` to include heartbeat information:

```cpp
struct ProjectionV3 {
    // ... all ProjectionV2 fields ...
    
    // NEW: Heartbeat information
    uint64_t heartbeat_timestamp;  // Last time this torus was alive
    uint32_t health_status;        // 0=healthy, 1=degraded, 2=failed
    
    // NEW: Process information (for reconstruction)
    struct ProcessInfo {
        uint32_t process_id;
        int16_t x, y, z;
        uint32_t state;  // Process-specific state
    };
    
    static constexpr size_t MAX_PROCESSES_IN_PROJECTION = 64;
    std::array<ProcessInfo, MAX_PROCESSES_IN_PROJECTION> processes;
    uint32_t num_processes;
};
```

**Size Impact**:
- Heartbeat: 8 + 4 = 12 bytes
- Process info: 64 × 16 bytes = 1024 bytes
- **Total new size**: 4.7KB + 1.0KB = **5.7KB** (still O(1))

### 2. Failure Detection in BraidedKernelV3

```cpp
class BraidedKernelV3 {
private:
    uint64_t last_heartbeat_ = 0;
    uint32_t health_status_ = 0;  // 0=healthy
    
public:
    // Update heartbeat on every braid exchange
    void updateHeartbeat() {
        last_heartbeat_ = getCurrentTime();
        health_status_ = 0;  // Healthy
    }
    
    // Check if this torus is alive
    bool isAlive(uint64_t current_time, uint64_t timeout) const {
        return (current_time - last_heartbeat_) < timeout;
    }
    
    // Mark as failed
    void markFailed() {
        health_status_ = 2;  // Failed
    }
    
    // Extract projection with heartbeat
    ProjectionV3 extractProjection() const {
        ProjectionV3 proj;
        // ... existing projection fields ...
        
        proj.heartbeat_timestamp = last_heartbeat_;
        proj.health_status = health_status_;
        
        // Extract process information (sample up to 64 processes)
        proj.num_processes = extractProcessInfo(proj.processes);
        
        return proj;
    }
};
```

### 3. Torus Reconstruction in TorusBraidV3

```cpp
class TorusBraidV3 {
private:
    uint64_t heartbeat_timeout_;  // 3× braid interval
    
    std::unique_ptr<ProjectionV3> last_proj_a_;
    std::unique_ptr<ProjectionV3> last_proj_b_;
    std::unique_ptr<ProjectionV3> last_proj_c_;
    
public:
    // Detect failures
    bool detectFailures() {
        uint64_t current_time = std::max({
            torus_a_->getCurrentTime(),
            torus_b_->getCurrentTime(),
            torus_c_->getCurrentTime()
        });
        
        bool a_alive = torus_a_->isAlive(current_time, heartbeat_timeout_);
        bool b_alive = torus_b_->isAlive(current_time, heartbeat_timeout_);
        bool c_alive = torus_c_->isAlive(current_time, heartbeat_timeout_);
        
        if (!a_alive) {
            std::cerr << "[TorusBraid] Torus A FAILED!" << std::endl;
            reconstructTorusA();
            return true;
        }
        if (!b_alive) {
            std::cerr << "[TorusBraid] Torus B FAILED!" << std::endl;
            reconstructTorusB();
            return true;
        }
        if (!c_alive) {
            std::cerr << "[TorusBraid] Torus C FAILED!" << std::endl;
            reconstructTorusC();
            return true;
        }
        
        return false;  // No failures
    }
    
    // Reconstruct Torus C from projections of A and B
    void reconstructTorusC() {
        std::cout << "[TorusBraid] Reconstructing Torus C..." << std::endl;
        
        // Step 1: Get last projections of C from A and B
        const ProjectionV3* proj_from_a = last_proj_c_.get();  // A's view of C
        const ProjectionV3* proj_from_b = last_proj_c_.get();  // B's view of C
        
        if (!proj_from_a || !proj_from_b) {
            std::cerr << "[TorusBraid] Cannot reconstruct: no projections available" << std::endl;
            return;
        }
        
        // Step 2: Choose most recent projection
        const ProjectionV3* proj = (proj_from_a->timestamp > proj_from_b->timestamp) 
                                   ? proj_from_a : proj_from_b;
        
        // Step 3: Create new torus
        torus_c_ = std::make_unique<BraidedKernelV3>();
        torus_c_->setTorusId(2);
        
        // Step 4: Restore state from projection
        restoreFromProjection(*torus_c_, *proj);
        
        // Step 5: Update heartbeat
        torus_c_->updateHeartbeat();
        
        std::cout << "[TorusBraid] Torus C reconstructed successfully" << std::endl;
        total_reconstructions_++;
    }
    
    // Restore torus state from projection
    void restoreFromProjection(BraidedKernelV3& torus, const ProjectionV3& proj) {
        // Restore processes
        for (uint32_t i = 0; i < proj.num_processes; i++) {
            const auto& proc = proj.processes[i];
            torus.spawnProcess(proc.x, proc.y, proc.z);
        }
        
        // Note: We cannot restore edges or events (not in projection)
        // This is a limitation of best-effort reconstruction
        
        std::cout << "[TorusBraid] Restored " << proj.num_processes << " processes" << std::endl;
    }
};
```

### 4. Process Migration

```cpp
class TorusBraidV3 {
public:
    // Migrate processes from failed torus to surviving tori
    void migrateProcesses(uint32_t failed_torus_id) {
        std::cout << "[TorusBraid] Migrating processes from Torus " << failed_torus_id << std::endl;
        
        // Get last projection of failed torus
        const ProjectionV3* proj = nullptr;
        if (failed_torus_id == 0) proj = last_proj_a_.get();
        else if (failed_torus_id == 1) proj = last_proj_b_.get();
        else proj = last_proj_c_.get();
        
        if (!proj) {
            std::cerr << "[TorusBraid] No projection available for migration" << std::endl;
            return;
        }
        
        // Distribute processes evenly across surviving tori
        std::vector<BraidedKernelV3*> surviving_tori;
        if (failed_torus_id != 0) surviving_tori.push_back(torus_a_.get());
        if (failed_torus_id != 1) surviving_tori.push_back(torus_b_.get());
        if (failed_torus_id != 2) surviving_tori.push_back(torus_c_.get());
        
        uint32_t torus_idx = 0;
        for (uint32_t i = 0; i < proj->num_processes; i++) {
            const auto& proc = proj->processes[i];
            
            // Spawn process on surviving torus
            surviving_tori[torus_idx]->spawnProcess(proc.x, proc.y, proc.z);
            
            // Round-robin distribution
            torus_idx = (torus_idx + 1) % surviving_tori.size();
        }
        
        std::cout << "[TorusBraid] Migrated " << proj->num_processes << " processes" << std::endl;
        total_migrations_ += proj->num_processes;
    }
};
```

---

## Implementation Plan

### Step 1: Extend Projection with Heartbeat (1-2 days)
- Add heartbeat_timestamp and health_status
- Add process information array
- Update hash computation and verification

### Step 2: Implement Failure Detection (2-3 days)
- Add heartbeat update in BraidedKernelV3
- Implement isAlive() check with timeout
- Add failure detection in TorusBraidV3

### Step 3: Implement Torus Reconstruction (3-4 days)
- Implement reconstructTorusX() methods
- Implement restoreFromProjection()
- Test reconstruction with simulated failures

### Step 4: Implement Process Migration (2-3 days)
- Implement migrateProcesses()
- Test migration with various workloads
- Verify processes continue executing after migration

### Step 5: Testing (3-4 days)
- Test failure detection (heartbeat timeout)
- Test torus reconstruction (kill one torus)
- Test process migration (verify processes survive)
- Test self-healing (automatic recovery)

**Total Estimated Time**: 11-16 days (2-3 weeks)

---

## Expected Behavior

### Scenario 1: Torus C Fails

```
Tick 0-4000: Normal operation (all 3 tori healthy)
Tick 4500: Torus C crashes (stops updating heartbeat)
Tick 5000: Braid exchange detects C is unresponsive
Tick 5000: Reconstruction begins
  - Get last projections of C from A and B
  - Create new Torus C
  - Restore processes from projection
  - Migrate remaining processes to A and B
Tick 5100: Reconstruction complete
Tick 5100-∞: Continue with reconstructed C (or 2 tori)
```

**Recovery Time**: ~100 ticks (< 1 second at 16M events/sec)

### Scenario 2: Two Tori Fail

```
Tick 0-4000: Normal operation
Tick 4500: Torus B crashes
Tick 4600: Torus C crashes
Tick 5000: Detect both failures
→ CRITICAL: Cannot reconstruct (need 2-of-3)
→ System enters degraded mode with only Torus A
→ Log error and alert operator
```

**Limitation**: System can only tolerate **1 failure at a time**.

### Scenario 3: Rapid Failure and Recovery

```
Tick 0-4000: Normal operation
Tick 4500: Torus C fails
Tick 5000: Reconstruct C
Tick 5100: C back online
Tick 6000: Torus A fails
Tick 6500: Reconstruct A
Tick 6600: A back online
→ System survives multiple failures
```

**Resilience**: As long as failures are spaced out, system can survive indefinitely.

---

## Performance Impact

### Overhead Estimates

| Operation | Phase 2 | Phase 3 | Overhead |
|-----------|---------|---------|----------|
| **Projection Size** | 4.7KB | 5.7KB | +21% |
| **Heartbeat Update** | N/A | ~0.1μs | Negligible |
| **Failure Detection** | N/A | ~1μs | Per braid cycle |
| **Reconstruction** | N/A | ~100ms | One-time (on failure) |

**Normal Operation Overhead**: < 2% (just heartbeat + failure check)  
**Failure Recovery Time**: ~100ms (acceptable for fault tolerance)

---

## Success Criteria

Phase 3 is complete when:

1. ✅ Heartbeat mechanism implemented and tested
2. ✅ Failure detection working (timeout-based)
3. ✅ Torus reconstruction working (2-of-3)
4. ✅ Process migration working (even distribution)
5. ✅ Self-healing demonstrated (automatic recovery)
6. ✅ Recovery time < 1 second
7. ✅ System survives at least 10 consecutive failures

---

## Risks and Mitigations

### Risk 1: Incomplete State Reconstruction
**Problem**: Projections don't capture full kernel state (event queue, edges).

**Mitigation**:
- **Accept limitation**: Best-effort reconstruction
- **Enhance projections**: Add more state (edges, pending events)
- **Graceful degradation**: System continues with reduced state

### Risk 2: Reconstruction Too Slow
**Problem**: Reconstruction takes too long, causing system stall.

**Mitigation**:
- **Parallel reconstruction**: Reconstruct while other tori continue
- **Incremental reconstruction**: Restore critical state first, rest later
- **Fallback**: Continue with 2 tori if reconstruction fails

### Risk 3: Cascading Failures
**Problem**: Reconstruction triggers more failures (overload).

**Mitigation**:
- **Rate limiting**: Don't reconstruct too frequently
- **Load shedding**: Drop non-critical work during reconstruction
- **Backpressure**: Slow down event injection during recovery

---

## Next Steps

After Phase 3 is complete:

**Phase 4: Optimization** (4-6 weeks)
- Parallel torus execution
- Adaptive braid interval
- Lock-free projection exchange
- 50M+ events/sec target

**Phase 5: Distributed Mode** (8-12 weeks)
- Network-based projection exchange
- Cross-machine torus deployment
- Distributed failure detection

---

**"The system that can survive failure is the system that will dominate."**

*Phase 3 will make the braided-torus system production-ready for mission-critical applications.*
