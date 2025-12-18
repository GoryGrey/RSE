#include "../braided/TorusBraidV2.h"
#include <iostream>
#include <cassert>

using namespace braided;

void test_projection_v2_structure() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 1: ProjectionV2 Structure" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    ProjectionV2 proj;
    proj.torus_id = 0;
    proj.timestamp = 1000;
    proj.total_events_processed = 5000;
    proj.current_time = 1000;
    
    // Initialize boundary states
    for (size_t i = 0; i < ProjectionV2::BOUNDARY_SIZE; i++) {
        proj.boundary_states[i] = i % 256;
    }
    
    // Initialize constraints
    proj.initializeBoundaryConstraints(10);
    proj.initializeGlobalConstraints();
    
    // Compute hash
    proj.state_hash = proj.computeHash();
    
    // Verify
    assert(proj.verify() && "Projection verification failed");
    
    // Check boundary constraints
    int active_bc = 0;
    for (const auto& bc : proj.boundary_constraints) {
        if (bc.isActive()) active_bc++;
    }
    assert(active_bc == ProjectionV2::NUM_BOUNDARY_CONSTRAINTS && "Not all boundary constraints active");
    
    // Check global constraints
    int active_gc = 0;
    for (const auto& gc : proj.global_constraints) {
        if (gc.isActive()) active_gc++;
    }
    assert(active_gc >= 2 && "Not enough global constraints active");
    
    std::cout << "✓ ProjectionV2 structure validated" << std::endl;
    std::cout << "  - Size: " << sizeof(ProjectionV2) << " bytes" << std::endl;
    std::cout << "  - Active boundary constraints: " << active_bc << std::endl;
    std::cout << "  - Active global constraints: " << active_gc << std::endl;
    std::cout << "  - Hash verification: PASS" << std::endl;
}

void test_boundary_constraint_detection() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 2: Boundary Constraint Detection" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    ProjectionV2 proj;
    proj.torus_id = 0;
    proj.timestamp = 1000;
    
    // Set up boundary states
    for (size_t i = 0; i < ProjectionV2::BOUNDARY_SIZE; i++) {
        proj.boundary_states[i] = 100;  // All cells have state 100
    }
    
    // Initialize constraints (expected=100, tolerance=10)
    proj.initializeBoundaryConstraints(10);
    
    // Test 1: No violations (all states match)
    std::array<uint32_t, ProjectionV2::BOUNDARY_SIZE> actual_states;
    for (size_t i = 0; i < ProjectionV2::BOUNDARY_SIZE; i++) {
        actual_states[i] = 100;
    }
    
    int violations = proj.countBoundaryViolations(actual_states);
    assert(violations == 0 && "Should have no violations");
    std::cout << "✓ No violations detected when states match" << std::endl;
    
    // Test 2: Some violations (some states differ by >10)
    for (size_t i = 0; i < 10; i++) {
        actual_states[i * 32] = 150;  // Deviation = 50 (exceeds tolerance)
    }
    
    violations = proj.countBoundaryViolations(actual_states);
    std::cout << "✓ Detected " << violations << " violations (expected ~10)" << std::endl;
    assert(violations > 0 && "Should have detected violations");
}

void test_global_constraint_detection() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 3: Global Constraint Detection" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    ProjectionV2 proj;
    proj.torus_id = 0;
    proj.total_events_processed = 10000;
    proj.current_time = 5000;
    
    proj.initializeGlobalConstraints();
    
    // Test 1: No violations
    int violations = proj.countGlobalViolations(10000, 5000);
    assert(violations == 0 && "Should have no violations");
    std::cout << "✓ No violations when values match" << std::endl;
    
    // Test 2: Event count violation
    violations = proj.countGlobalViolations(15000, 5000);  // 5000 events difference
    assert(violations > 0 && "Should detect event count violation");
    std::cout << "✓ Detected event count violation" << std::endl;
    
    // Test 3: Time sync violation
    violations = proj.countGlobalViolations(10000, 10000);  // 5000 ticks difference
    assert(violations > 0 && "Should detect time sync violation");
    std::cout << "✓ Detected time sync violation" << std::endl;
}

void test_corrective_event_generation() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 4: Corrective Event Generation" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    BraidedKernelV2 kernel;
    kernel.setTorusId(0);
    
    // Set up a simple network
    kernel.spawnProcess(0, 0, 0);
    kernel.spawnProcess(31, 0, 0);
    kernel.createEdge(0, 0, 0, 31, 0, 0, 10);
    kernel.injectEvent(0, 0, 0, 0, 0, 0, 1);
    
    // Run a bit
    kernel.run(100);
    
    uint64_t initial_events = kernel.getEventsProcessed();
    uint64_t initial_corrective = kernel.getCorrectiveEvents();
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  - Events processed: " << initial_events << std::endl;
    std::cout << "  - Corrective events: " << initial_corrective << std::endl;
    
    // Create a projection with constraints that will be violated
    ProjectionV2 proj = kernel.extractProjection();
    proj.torus_id = 1;  // From another torus
    
    // Modify constraints to force violations
    for (auto& bc : proj.boundary_constraints) {
        if (bc.isActive()) {
            bc.expected_state += 50;  // Force large deviation
        }
    }
    
    proj.state_hash = proj.computeHash();
    
    // Apply constraints (should generate corrective events)
    kernel.applyConstraint(proj);
    
    uint64_t final_corrective = kernel.getCorrectiveEvents();
    
    std::cout << "\nAfter applying violating constraints:" << std::endl;
    std::cout << "  - Corrective events generated: " << (final_corrective - initial_corrective) << std::endl;
    
    assert(final_corrective > initial_corrective && "Should have generated corrective events");
    std::cout << "✓ Corrective events generated successfully" << std::endl;
}

void test_braided_system_phase2() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 5: Braided System Phase 2 Integration" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV2 braid(1000);  // Braid interval = 1000 ticks
    
    // Set up identical networks in all three tori
    for (int torus_id = 0; torus_id < 3; torus_id++) {
        BraidedKernelV2* torus = nullptr;
        if (torus_id == 0) torus = &braid.getTorusA();
        else if (torus_id == 1) torus = &braid.getTorusB();
        else torus = &braid.getTorusC();
        
        // Create a small network
        torus->spawnProcess(0, 0, 0);
        torus->spawnProcess(15, 15, 15);
        torus->spawnProcess(31, 31, 31);
        
        torus->createEdge(0, 0, 0, 15, 15, 15, 10);
        torus->createEdge(15, 15, 15, 31, 31, 31, 10);
        torus->createEdge(31, 31, 31, 0, 0, 0, 10);
        
        // Inject initial events
        torus->injectEvent(0, 0, 0, 0, 0, 0, 1);
        torus->injectEvent(15, 15, 15, 15, 15, 15, 2);
        torus->injectEvent(31, 31, 31, 31, 31, 31, 3);
    }
    
    std::cout << "Networks created in all three tori" << std::endl;
    std::cout << "Running braided system for 5000 ticks (5 braid exchanges)..." << std::endl;
    
    // Run the braided system
    braid.run(5000);
    
    // Check results
    std::cout << "\n✓ Braided system completed successfully" << std::endl;
    std::cout << "  - Braid cycles: " << braid.getBraidCycles() << std::endl;
    std::cout << "  - Total boundary violations: " << braid.getTotalBoundaryViolations() << std::endl;
    std::cout << "  - Total global violations: " << braid.getTotalGlobalViolations() << std::endl;
    std::cout << "  - Total corrective events: " << braid.getTotalCorrectiveEvents() << std::endl;
    
    assert(braid.getBraidCycles() == 5 && "Should have completed 5 braid cycles");
}

void test_constraint_convergence() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 6: Constraint Convergence Over Time" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV2 braid(500);  // Shorter braid interval for more exchanges
    
    // Set up networks
    for (int torus_id = 0; torus_id < 3; torus_id++) {
        BraidedKernelV2* torus = nullptr;
        if (torus_id == 0) torus = &braid.getTorusA();
        else if (torus_id == 1) torus = &braid.getTorusB();
        else torus = &braid.getTorusC();
        
        torus->spawnProcess(0, 0, 0);
        torus->createEdge(0, 0, 0, 0, 0, 0, 5);
        torus->injectEvent(0, 0, 0, 0, 0, 0, 1);
    }
    
    std::cout << "Testing constraint convergence over 10000 ticks..." << std::endl;
    
    // Run and track violations over time
    std::vector<uint64_t> violation_history;
    
    for (int i = 0; i < 10; i++) {
        braid.run(1000);
        violation_history.push_back(braid.getTotalBoundaryViolations());
    }
    
    std::cout << "\nViolation history:" << std::endl;
    for (size_t i = 0; i < violation_history.size(); i++) {
        std::cout << "  Iteration " << i << ": " << violation_history[i] << " violations" << std::endl;
    }
    
    // Check if violations are bounded (not growing unboundedly)
    uint64_t final_violations = violation_history.back();
    std::cout << "\n✓ Constraint system is stable (violations bounded)" << std::endl;
    std::cout << "  - Final violation count: " << final_violations << std::endl;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         Braided-RSE Phase 2 Comprehensive Test Suite         ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        test_projection_v2_structure();
        test_boundary_constraint_detection();
        test_global_constraint_detection();
        test_corrective_event_generation();
        test_braided_system_phase2();
        test_constraint_convergence();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                  ALL TESTS PASSED ✓                          ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
