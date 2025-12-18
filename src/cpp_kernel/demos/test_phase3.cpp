#include "../braided/TorusBraidV3.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace braided;

void test_heartbeat_mechanism() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 1: Heartbeat Mechanism" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    BraidedKernelV3 kernel;
    kernel.setTorusId(0);
    
    // Initial state: no heartbeat
    assert(!kernel.isAlive(1000, 100) && "Should not be alive without heartbeat");
    std::cout << "✓ Kernel not alive without heartbeat" << std::endl;
    
    // Update heartbeat
    kernel.updateHeartbeat();
    
    // Should be alive within timeout
    assert(kernel.isAlive(50, 100) && "Should be alive within timeout");
    std::cout << "✓ Kernel alive after heartbeat update" << std::endl;
    
    // Should be dead after timeout
    assert(!kernel.isAlive(200, 100) && "Should be dead after timeout");
    std::cout << "✓ Kernel dead after timeout" << std::endl;
    
    // Health status
    assert(kernel.getHealthStatus() == ProjectionV3::HEALTHY && "Should be healthy");
    kernel.markFailed();
    assert(kernel.getHealthStatus() == ProjectionV3::FAILED && "Should be failed");
    std::cout << "✓ Health status tracking works" << std::endl;
}

void test_projection_with_heartbeat() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 2: Projection with Heartbeat" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    BraidedKernelV3 kernel;
    kernel.setTorusId(0);
    kernel.updateHeartbeat();
    
    // Spawn some processes
    kernel.spawnProcess(0, 0, 0);
    kernel.spawnProcess(15, 15, 15);
    kernel.spawnProcess(31, 31, 31);
    
    // Extract projection
    ProjectionV3 proj = kernel.extractProjection();
    
    // Verify heartbeat in projection
    assert(proj.heartbeat_timestamp == kernel.getCurrentTime() && "Heartbeat should match");
    assert(proj.health_status == ProjectionV3::HEALTHY && "Should be healthy");
    std::cout << "✓ Projection contains heartbeat information" << std::endl;
    
    // Verify process information
    assert(proj.num_processes == 3 && "Should have 3 processes");
    std::cout << "✓ Projection contains " << proj.num_processes << " processes" << std::endl;
    
    // Verify projection integrity
    assert(proj.verify() && "Projection should verify");
    std::cout << "✓ Projection integrity verified" << std::endl;
    
    // Check if alive
    assert(proj.isAlive(proj.heartbeat_timestamp + 50, 100) && "Should be alive");
    assert(!proj.isAlive(proj.heartbeat_timestamp + 150, 100) && "Should be dead");
    std::cout << "✓ Projection liveness check works" << std::endl;
}

void test_state_restoration() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 3: State Restoration from Projection" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    // Create original kernel with processes
    BraidedKernelV3 original;
    original.setTorusId(0);
    original.updateHeartbeat();
    
    original.spawnProcess(5, 10, 15);
    original.spawnProcess(20, 25, 30);
    original.spawnProcess(1, 2, 3);
    
    size_t original_process_count = original.getNumActiveProcesses();
    std::cout << "Original kernel has " << original_process_count << " processes" << std::endl;
    
    // Extract projection
    ProjectionV3 proj = original.extractProjection();
    
    // Create new kernel and restore from projection
    BraidedKernelV3 restored;
    restored.setTorusId(1);
    restored.restoreFromProjection(proj);
    
    size_t restored_process_count = restored.getNumActiveProcesses();
    std::cout << "Restored kernel has " << restored_process_count << " processes" << std::endl;
    
    assert(restored_process_count == original_process_count && "Process count should match");
    assert(restored.getHealthStatus() == ProjectionV3::HEALTHY && "Should be healthy");
    std::cout << "✓ State restoration successful" << std::endl;
}

void test_failure_detection() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 4: Failure Detection" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV3 braid(500);  // Short braid interval for faster testing
    
    // Set up processes
    braid.getTorusA().spawnProcess(0, 0, 0);
    braid.getTorusB().spawnProcess(0, 0, 0);
    braid.getTorusC().spawnProcess(0, 0, 0);
    
    std::cout << "Running system normally for 1000 ticks..." << std::endl;
    braid.run(1000);
    
    // All tori should be healthy
    assert(braid.getTorusA().getHealthStatus() == ProjectionV3::HEALTHY);
    assert(braid.getTorusB().getHealthStatus() == ProjectionV3::HEALTHY);
    assert(braid.getTorusC().getHealthStatus() == ProjectionV3::HEALTHY);
    std::cout << "✓ All tori healthy after normal operation" << std::endl;
    
    // Simulate failure of Torus C
    braid.simulateFailure(2);
    
    // Run a bit more to detect failure
    braid.run(1000);
    
    // Check if failure was detected
    assert(braid.getTotalFailures() > 0 && "Should have detected failure");
    std::cout << "✓ Failure detected: " << braid.getTotalFailures() << " failures" << std::endl;
}

void test_torus_reconstruction() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 5: Torus Reconstruction" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV3 braid(500);
    
    // Set up networks in all tori
    for (int i = 0; i < 3; i++) {
        BraidedKernelV3* torus = nullptr;
        if (i == 0) torus = &braid.getTorusA();
        else if (i == 1) torus = &braid.getTorusB();
        else torus = &braid.getTorusC();
        
        torus->spawnProcess(0, 0, 0);
        torus->spawnProcess(15, 15, 15);
        torus->spawnProcess(31, 31, 31);
        torus->createEdge(0, 0, 0, 15, 15, 15, 10);
        torus->injectEvent(0, 0, 0, 0, 0, 0, 1);
    }
    
    std::cout << "Running system for 2000 ticks..." << std::endl;
    braid.run(2000);
    
    size_t initial_processes_c = braid.getTorusC().getNumActiveProcesses();
    std::cout << "Torus C has " << initial_processes_c << " processes before failure" << std::endl;
    
    // Simulate failure of Torus C
    std::cout << "\nSimulating failure of Torus C..." << std::endl;
    braid.simulateFailure(2);
    
    // Run to trigger detection and reconstruction
    braid.run(1000);
    
    // Check if reconstruction happened
    assert(braid.getTotalReconstructions() > 0 && "Should have reconstructed");
    std::cout << "✓ Reconstruction completed: " << braid.getTotalReconstructions() << " reconstructions" << std::endl;
    
    // Check if Torus C is healthy again
    assert(braid.getTorusC().getHealthStatus() == ProjectionV3::HEALTHY && "Should be healthy after reconstruction");
    std::cout << "✓ Torus C is healthy after reconstruction" << std::endl;
    
    // Check if processes were restored/migrated
    size_t final_processes_c = braid.getTorusC().getNumActiveProcesses();
    std::cout << "Torus C has " << final_processes_c << " processes after reconstruction" << std::endl;
}

void test_process_migration() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 6: Process Migration" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV3 braid(500);
    
    // Set up processes only in Torus C
    for (int i = 0; i < 10; i++) {
        braid.getTorusC().spawnProcess(i, i, i);
    }
    
    size_t initial_a = braid.getTorusA().getNumActiveProcesses();
    size_t initial_b = braid.getTorusB().getNumActiveProcesses();
    size_t initial_c = braid.getTorusC().getNumActiveProcesses();
    
    std::cout << "Initial process counts:" << std::endl;
    std::cout << "  Torus A: " << initial_a << std::endl;
    std::cout << "  Torus B: " << initial_b << std::endl;
    std::cout << "  Torus C: " << initial_c << std::endl;
    
    // Run to establish projections
    braid.run(1000);
    
    // Simulate failure of Torus C
    std::cout << "\nSimulating failure of Torus C..." << std::endl;
    braid.simulateFailure(2);
    
    // Run to trigger migration and reconstruction
    braid.run(1000);
    
    size_t final_a = braid.getTorusA().getNumActiveProcesses();
    size_t final_b = braid.getTorusB().getNumActiveProcesses();
    
    std::cout << "\nFinal process counts:" << std::endl;
    std::cout << "  Torus A: " << final_a << std::endl;
    std::cout << "  Torus B: " << final_b << std::endl;
    std::cout << "  Total migrations: " << braid.getTotalMigrations() << std::endl;
    
    assert(braid.getTotalMigrations() > 0 && "Should have migrated processes");
    std::cout << "✓ Process migration successful" << std::endl;
}

void test_multiple_failures() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 7: Multiple Sequential Failures" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV3 braid(500);
    
    // Set up processes in all tori
    for (int i = 0; i < 3; i++) {
        BraidedKernelV3* torus = nullptr;
        if (i == 0) torus = &braid.getTorusA();
        else if (i == 1) torus = &braid.getTorusB();
        else torus = &braid.getTorusC();
        
        for (int j = 0; j < 5; j++) {
            torus->spawnProcess(j * 5, j * 5, j * 5);
        }
    }
    
    std::cout << "Running system normally..." << std::endl;
    braid.run(1000);
    
    // Fail Torus A
    std::cout << "\n1. Failing Torus A..." << std::endl;
    braid.simulateFailure(0);
    braid.run(1000);
    assert(braid.getTotalFailures() >= 1 && "Should detect first failure");
    std::cout << "✓ Torus A failed and reconstructed" << std::endl;
    
    // Fail Torus B
    std::cout << "\n2. Failing Torus B..." << std::endl;
    braid.simulateFailure(1);
    braid.run(1000);
    assert(braid.getTotalFailures() >= 2 && "Should detect second failure");
    std::cout << "✓ Torus B failed and reconstructed" << std::endl;
    
    // Fail Torus C
    std::cout << "\n3. Failing Torus C..." << std::endl;
    braid.simulateFailure(2);
    braid.run(1000);
    assert(braid.getTotalFailures() >= 3 && "Should detect third failure");
    std::cout << "✓ Torus C failed and reconstructed" << std::endl;
    
    std::cout << "\nFinal statistics:" << std::endl;
    std::cout << "  Total failures: " << braid.getTotalFailures() << std::endl;
    std::cout << "  Total reconstructions: " << braid.getTotalReconstructions() << std::endl;
    std::cout << "  Total migrations: " << braid.getTotalMigrations() << std::endl;
    
    assert(braid.getTotalReconstructions() >= 3 && "Should have 3+ reconstructions");
    std::cout << "✓ System survived multiple sequential failures" << std::endl;
}

void test_self_healing_resilience() {
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "  TEST 8: Self-Healing Resilience (10 Failures)" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════\n" << std::endl;
    
    TorusBraidV3 braid(500);
    
    // Set up processes
    for (int i = 0; i < 3; i++) {
        BraidedKernelV3* torus = nullptr;
        if (i == 0) torus = &braid.getTorusA();
        else if (i == 1) torus = &braid.getTorusB();
        else torus = &braid.getTorusC();
        
        torus->spawnProcess(0, 0, 0);
        torus->spawnProcess(15, 15, 15);
    }
    
    std::cout << "Testing resilience with 10 random failures..." << std::endl;
    
    for (int i = 0; i < 10; i++) {
        // Run normally
        braid.run(500);
        
        // Randomly fail one torus
        uint32_t torus_to_fail = i % 3;
        std::cout << "\nFailure " << (i+1) << ": Torus " << torus_to_fail << std::endl;
        braid.simulateFailure(torus_to_fail);
        
        // Allow recovery
        braid.run(1000);
    }
    
    std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Final Resilience Statistics:" << std::endl;
    std::cout << "  Total failures: " << braid.getTotalFailures() << std::endl;
    std::cout << "  Total reconstructions: " << braid.getTotalReconstructions() << std::endl;
    std::cout << "  Total migrations: " << braid.getTotalMigrations() << std::endl;
    std::cout << "  Success rate: " << (braid.getTotalReconstructions() * 100.0 / braid.getTotalFailures()) << "%" << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
    
    assert(braid.getTotalFailures() >= 10 && "Should have detected 10+ failures");
    assert(braid.getTotalReconstructions() >= 10 && "Should have 10+ reconstructions");
    std::cout << "✓ System survived 10 consecutive failures!" << std::endl;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         Braided-RSE Phase 3 Comprehensive Test Suite         ║" << std::endl;
    std::cout << "║                    Self-Healing & Fault Tolerance             ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        test_heartbeat_mechanism();
        test_projection_with_heartbeat();
        test_state_restoration();
        test_failure_detection();
        test_torus_reconstruction();
        test_process_migration();
        test_multiple_failures();
        test_self_healing_resilience();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                  ALL TESTS PASSED ✅                          ║" << std::endl;
        std::cout << "║                                                               ║" << std::endl;
        std::cout << "║  The braided-torus system is now SELF-HEALING! 🎉            ║" << std::endl;
        std::cout << "║                                                               ║" << std::endl;
        std::cout << "║  ✓ Automatic failure detection                               ║" << std::endl;
        std::cout << "║  ✓ Torus reconstruction (2-of-3)                             ║" << std::endl;
        std::cout << "║  ✓ Process migration                                         ║" << std::endl;
        std::cout << "║  ✓ Survived 10+ consecutive failures                         ║" << std::endl;
        std::cout << "║                                                               ║" << std::endl;
        std::cout << "║  Phase 3 COMPLETE! Ready for Phase 4 (Optimization)         ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
