# Phase 2: Boundary Coupling Design

**Status**: In Development  
**Date**: December 18, 2025

---

## Overview

Phase 2 implements **actual constraint propagation** between tori. In Phase 1, tori exchanged projections but didn't actually affect each other's behavior. In Phase 2, projections become **constraints** that influence execution.

---

## Key Concepts

### 1. Boundary Coupling

Each torus has **6 faces** (x=0, x=31, y=0, y=31, z=0, z=31). The **x=0 face** of one torus is **coupled** to the **x=31 face** of another torus through projection exchange.

**Coupling Mechanism**:
```
Torus A (x=0 face) → Projection → Torus B (applies as constraint to x=31 face)
Torus B (x=0 face) → Projection → Torus C (applies as constraint to x=31 face)
Torus C (x=0 face) → Projection → Torus A (applies as constraint to x=31 face)
```

**Why x=0 and x=31?**
- Toroidal topology means x=0 and x=31 are **adjacent** (wraparound)
- Coupling at boundaries creates a **continuous braided structure**
- No arbitrary choice of "which face" - it's topologically natural

### 2. Constraint Propagation

A **constraint** is a rule that must be satisfied. When a torus receives a projection, it extracts constraints and **propagates** them into its own execution.

**Types of Constraints**:

1. **Boundary State Constraints**
   - Expected state at boundary cells
   - Example: "Cell (31, y, z) should have activity level X"

2. **Global Invariant Constraints**
   - Conservation laws (e.g., total event count)
   - Example: "Sum of events across all tori should be constant"

3. **Temporal Constraints**
   - Time synchronization
   - Example: "Current time should be within ±1000 ticks of other tori"

### 3. Corrective Events

When a constraint is **violated**, the system generates **corrective events** to restore consistency.

**Example**:
```
Torus A expects boundary cell (31, 5, 10) to have activity level 50
Actual activity level: 30
→ Generate event to increase activity by 20
```

**Corrective Event Properties**:
- **Priority**: Higher than normal events (processed first)
- **Source**: System-generated (not user-injected)
- **Payload**: Correction magnitude

---

## Design

### 1. Enhanced Projection Structure

Extend `Projection` to include **constraint information**:

```cpp
struct Projection {
    // ... existing fields ...
    
    // NEW: Constraint information
    struct BoundaryConstraint {
        uint32_t cell_index;      // Which boundary cell (0-1023)
        int32_t expected_state;   // Expected state value
        int32_t tolerance;        // Acceptable deviation
    };
    
    std::array<BoundaryConstraint, 32> boundary_constraints;  // Sample of boundary cells
    
    struct GlobalConstraint {
        enum Type { EVENT_CONSERVATION, TIME_SYNC, LOAD_BALANCE };
        Type type;
        int64_t expected_value;
        int64_t tolerance;
    };
    
    std::array<GlobalConstraint, 4> global_constraints;
};
```

**Size Impact**:
- Boundary constraints: 32 × 12 bytes = 384 bytes
- Global constraints: 4 × 20 bytes = 80 bytes
- **Total new size**: 4.2KB + 0.5KB = **4.7KB** (still O(1))

### 2. Constraint Application

When a torus receives a projection, it applies constraints:

```cpp
bool BraidedKernel::applyConstraint(const Projection& proj) {
    // 1. Verify projection integrity
    if (!proj.verify()) return false;
    
    // 2. Apply boundary constraints
    for (const auto& bc : proj.boundary_constraints) {
        if (bc.cell_index == 0) continue;  // Skip empty slots
        
        int actual_state = getBoundaryState(bc.cell_index);
        int deviation = actual_state - bc.expected_state;
        
        if (std::abs(deviation) > bc.tolerance) {
            // Generate corrective event
            generateCorrectiveEvent(bc.cell_index, -deviation);
        }
    }
    
    // 3. Apply global constraints
    for (const auto& gc : proj.global_constraints) {
        if (gc.type == GlobalConstraint::EVENT_CONSERVATION) {
            // Check if our event count is consistent
            int64_t our_events = getEventsProcessed();
            int64_t expected_events = gc.expected_value;
            
            if (std::abs(our_events - expected_events) > gc.tolerance) {
                // Log inconsistency (can't easily correct this)
                logInconsistency("EVENT_CONSERVATION", our_events, expected_events);
            }
        }
        // ... handle other constraint types ...
    }
    
    return true;
}
```

### 3. Corrective Event Generation

Corrective events are **high-priority** events that restore consistency:

```cpp
void BraidedKernel::generateCorrectiveEvent(uint32_t cell_index, int32_t correction) {
    // Decode cell index to (x, y, z)
    int x = cell_index / 1024;
    int y = (cell_index % 1024) / 32;
    int z = cell_index % 32;
    
    // Create corrective event
    RDLEvent evt;
    evt.timestamp = getCurrentTime() + 1;  // Process ASAP
    evt.dst_node = nodeId(x, y, z);
    evt.src_node = nodeId(x, y, z);  // Self-event
    evt.payload = correction;
    
    // Mark as high-priority (use a special flag or separate queue)
    injectEvent(x, y, z, x, y, z, correction);
}
```

### 4. Boundary State Extraction

Extract state from boundary cells for projection:

```cpp
Projection BraidedKernel::extractProjection() const {
    Projection proj;
    
    // ... existing projection fields ...
    
    // NEW: Extract boundary constraints
    // Sample 32 cells from x=0 face (out of 1024 total)
    for (int i = 0; i < 32; i++) {
        int y = (i * 32) / 32;  // Sample evenly
        int z = i % 32;
        
        uint32_t cell_index = nodeId(0, y, z);
        int32_t state = extractBoundaryState(0, y, z);
        
        proj.boundary_constraints[i] = {
            .cell_index = cell_index,
            .expected_state = state,
            .tolerance = 10  // Allow ±10 deviation
        };
    }
    
    // NEW: Extract global constraints
    proj.global_constraints[0] = {
        .type = Projection::GlobalConstraint::EVENT_CONSERVATION,
        .expected_value = static_cast<int64_t>(getEventsProcessed()),
        .tolerance = 1000  // Allow ±1000 events difference
    };
    
    proj.global_constraints[1] = {
        .type = Projection::GlobalConstraint::TIME_SYNC,
        .expected_value = static_cast<int64_t>(getCurrentTime()),
        .tolerance = 1000  // Allow ±1000 ticks difference
    };
    
    return proj;
}
```

---

## Implementation Plan

### Step 1: Extend Projection Structure (1-2 days)
- Add `BoundaryConstraint` and `GlobalConstraint` structs
- Update `computeHash()` to include new fields
- Update `verify()` to validate constraints

### Step 2: Implement Constraint Extraction (2-3 days)
- Implement `extractBoundaryState()` for all boundary cells
- Sample boundary cells evenly (32 out of 1024)
- Extract global constraints (event count, time)

### Step 3: Implement Constraint Application (3-4 days)
- Implement `applyConstraint()` with boundary checking
- Implement `generateCorrectiveEvent()` for violations
- Add logging for inconsistencies

### Step 4: Enhance BraidCoordinator (2-3 days)
- Update `exchange()` to apply constraints after projection
- Add metrics for constraint violations
- Add debugging output for constraint application

### Step 5: Testing (3-4 days)
- Test boundary coupling with simple workloads
- Test corrective event generation
- Test global constraint enforcement
- Measure overhead of constraint checking

**Total Estimated Time**: 11-16 days (2-3 weeks)

---

## Expected Behavior

### Scenario 1: Consistent Execution

```
Torus A: 1000 events processed, time=5000
Torus B: 1020 events processed, time=5010
Torus C: 980 events processed, time=4990

→ All within tolerance (±1000)
→ No corrective events generated
→ System continues normally
```

### Scenario 2: Boundary Inconsistency

```
Torus A: Boundary cell (0, 5, 10) has activity=50
Torus B: Receives projection, expects cell (31, 5, 10) to have activity=50
Torus B: Actual activity at (31, 5, 10) is 30

→ Deviation = -20 (exceeds tolerance of ±10)
→ Generate corrective event with payload=+20
→ Cell (31, 5, 10) activity increases to 50
→ Consistency restored
```

### Scenario 3: Time Divergence

```
Torus A: time=10000
Torus B: time=11500
Torus C: time=9800

→ B is 1500 ticks ahead (exceeds tolerance of ±1000)
→ Log inconsistency
→ Adjust braid interval to force more frequent synchronization
```

---

## Performance Impact

### Overhead Estimates

| Operation | Phase 1 | Phase 2 | Overhead |
|-----------|---------|---------|----------|
| **Projection Size** | 4.2KB | 4.7KB | +12% |
| **Projection Extraction** | ~1μs | ~2μs | +100% |
| **Constraint Application** | N/A | ~5μs | New |
| **Corrective Events** | 0 | 0-10/cycle | Variable |

**Total Overhead**: ~7μs per braid cycle (every 1000 ticks)  
**Impact on Throughput**: < 1% (negligible)

---

## Success Criteria

Phase 2 is complete when:

1. ✅ Boundary constraints extracted and applied
2. ✅ Corrective events generated for violations
3. ✅ Global constraints enforced (event count, time sync)
4. ✅ Comprehensive tests passing (boundary coupling, corrective events)
5. ✅ Overhead < 5% of total execution time
6. ✅ Consistency violations reduced by >90% compared to Phase 1

---

## Risks and Mitigations

### Risk 1: Corrective Events Cause Oscillation
**Problem**: Corrective events might overcorrect, causing oscillation.

**Mitigation**:
- Use **damping** (correct only 50% of deviation)
- Add **hysteresis** (don't correct if deviation is small)
- Limit **correction rate** (max 1 corrective event per cell per cycle)

### Risk 2: Constraint Checking Too Slow
**Problem**: Checking 32 boundary cells might be too slow.

**Mitigation**:
- Sample fewer cells (e.g., 16 instead of 32)
- Use **lazy evaluation** (only check when deviation suspected)
- **Parallelize** constraint checking

### Risk 3: False Positives
**Problem**: Legitimate state differences flagged as inconsistencies.

**Mitigation**:
- Use **wider tolerances** initially
- **Tune tolerances** based on workload
- Add **grace period** (ignore violations in first few cycles)

---

## Next Steps

After Phase 2 is complete:

**Phase 3: Self-Correction** (4-6 weeks)
- Automatic torus reconstruction on failure
- Enhanced fault tolerance
- Self-healing system

**Phase 4: Optimization** (4-6 weeks)
- Parallel torus execution
- Adaptive braid interval
- 50M+ events/sec target

---

**"Constraints are not restrictions. They are the forces that create order from chaos."**
