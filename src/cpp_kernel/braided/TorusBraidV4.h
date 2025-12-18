#pragma once

#include "BraidedKernelV3.h"
#include "ProjectionV3.h"

#include <memory>
#include <iostream>
#include <iomanip>
#include <optional>
#include <thread>
#include <atomic>
#include <barrier>
#include <chrono>

namespace braided {

/**
 * TorusBraidV4: Parallel execution with adaptive braid interval.
 * 
 * New in Phase 4:
 * - Parallel torus execution (3 threads)
 * - Lock-free projection exchange
 * - Adaptive braid interval
 * - Performance monitoring
 * 
 * Target: 50M+ events/sec (3× single-torus)
 */
class TorusBraidV4 {
private:
    // Three tori (heap-allocated)
    std::unique_ptr<BraidedKernelV3> torus_a_;
    std::unique_ptr<BraidedKernelV3> torus_b_;
    std::unique_ptr<BraidedKernelV3> torus_c_;
    
    // Parallel execution
    std::unique_ptr<std::thread> thread_a_;
    std::unique_ptr<std::thread> thread_b_;
    std::unique_ptr<std::thread> thread_c_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> should_exchange_{false};
    
    // Synchronization barrier (4 threads: 3 tori + 1 coordinator)
    std::unique_ptr<std::barrier<>> sync_barrier_;
    
    // Lock-free projection buffers (double buffering)
    struct ProjectionBuffer {
        std::atomic<ProjectionV3*> current{nullptr};
        ProjectionV3 buffers[2];
        std::atomic<int> write_index{0};
        
        void write(const ProjectionV3& proj) {
            int idx = write_index.load(std::memory_order_relaxed);
            int next_idx = 1 - idx;
            
            buffers[next_idx] = proj;
            current.store(&buffers[next_idx], std::memory_order_release);
            write_index.store(next_idx, std::memory_order_relaxed);
        }
        
        ProjectionV3 read() const {
            ProjectionV3* ptr = current.load(std::memory_order_acquire);
            return ptr ? *ptr : ProjectionV3{};
        }
    };
    
    ProjectionBuffer proj_buffer_a_;
    ProjectionBuffer proj_buffer_b_;
    ProjectionBuffer proj_buffer_c_;
    
    // Braid configuration
    std::atomic<uint64_t> braid_interval_;
    uint64_t heartbeat_timeout_;
    uint64_t braid_cycles_ = 0;
    
    // Adaptive braid interval
    static constexpr uint64_t MIN_BRAID_INTERVAL = 100;
    static constexpr uint64_t MAX_BRAID_INTERVAL = 10000;
    static constexpr double VIOLATION_THRESHOLD = 0.05;  // 5%
    
    // Metrics
    std::atomic<uint64_t> total_boundary_violations_{0};
    std::atomic<uint64_t> total_global_violations_{0};
    std::atomic<uint64_t> total_corrective_events_{0};
    std::atomic<uint64_t> total_projection_exchanges_{0};
    std::atomic<uint64_t> total_failures_detected_{0};
    std::atomic<uint64_t> total_reconstructions_{0};
    std::atomic<uint64_t> total_migrations_{0};
    
    // Performance metrics
    std::atomic<uint64_t> total_ticks_[3]{0, 0, 0};
    std::chrono::high_resolution_clock::time_point start_time_;
    
public:
    /**
     * Constructor.
     * @param braid_interval Initial braid interval (will adapt)
     */
    explicit TorusBraidV4(uint64_t braid_interval = 1000)
        : braid_interval_(braid_interval),
          heartbeat_timeout_(braid_interval * 3)
    {
        // Allocate tori
        torus_a_ = std::make_unique<BraidedKernelV3>();
        torus_b_ = std::make_unique<BraidedKernelV3>();
        torus_c_ = std::make_unique<BraidedKernelV3>();
        
        // Set torus IDs
        torus_a_->setTorusId(0);
        torus_b_->setTorusId(1);
        torus_c_->setTorusId(2);
        
        // Initialize heartbeats
        torus_a_->updateHeartbeat();
        torus_b_->updateHeartbeat();
        torus_c_->updateHeartbeat();
        
        // Initialize projection buffers
        proj_buffer_a_.write(torus_a_->extractProjection());
        proj_buffer_b_.write(torus_b_->extractProjection());
        proj_buffer_c_.write(torus_c_->extractProjection());
        
        // Create synchronization barrier (4 threads)
        sync_barrier_ = std::make_unique<std::barrier<>>(4);
        
        std::cout << "[TorusBraidV4] Initialized with parallel execution" << std::endl;
        std::cout << "  Initial braid_interval=" << braid_interval << std::endl;
        std::cout << "  Heartbeat_timeout=" << heartbeat_timeout_ << std::endl;
        std::cout << "  Adaptive range: [" << MIN_BRAID_INTERVAL << ", " << MAX_BRAID_INTERVAL << "]" << std::endl;
    }
    
    ~TorusBraidV4() {
        stop();
    }
    
    // Access to individual tori (for setup)
    BraidedKernelV3& getTorusA() { return *torus_a_; }
    BraidedKernelV3& getTorusB() { return *torus_b_; }
    BraidedKernelV3& getTorusC() { return *torus_c_; }
    
    /**
     * Start parallel execution.
     */
    void start() {
        if (running_.load()) {
            std::cerr << "[TorusBraidV4] Already running!" << std::endl;
            return;
        }
        
        running_.store(true);
        start_time_ = std::chrono::high_resolution_clock::now();
        
        // Launch worker threads
        thread_a_ = std::make_unique<std::thread>(&TorusBraidV4::torusWorker, this, 0, torus_a_.get());
        thread_b_ = std::make_unique<std::thread>(&TorusBraidV4::torusWorker, this, 1, torus_b_.get());
        thread_c_ = std::make_unique<std::thread>(&TorusBraidV4::torusWorker, this, 2, torus_c_.get());
        
        std::cout << "[TorusBraidV4] Parallel execution started (3 threads)" << std::endl;
    }
    
    /**
     * Stop parallel execution.
     */
    void stop() {
        if (!running_.load()) {
            return;
        }
        
        running_.store(false);
        should_exchange_.store(true);  // Wake up threads
        
        // Wait for threads to finish
        if (thread_a_ && thread_a_->joinable()) thread_a_->join();
        if (thread_b_ && thread_b_->joinable()) thread_b_->join();
        if (thread_c_ && thread_c_->joinable()) thread_c_->join();
        
        std::cout << "[TorusBraidV4] Parallel execution stopped" << std::endl;
    }
    
    /**
     * Run for a specified duration (in milliseconds).
     */
    void runFor(uint64_t duration_ms) {
        start();
        
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t last_exchange_time = 0;
        
        while (running_.load()) {
            auto now = std::chrono::high_resolution_clock::now();
            uint64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            
            if (elapsed_ms >= duration_ms) {
                break;
            }
            
            // Check if it's time for a braid exchange
            if (elapsed_ms - last_exchange_time >= braid_interval_.load() / 1000) {
                performBraidExchange();
                last_exchange_time = elapsed_ms;
            }
            
            // Sleep briefly to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        stop();
    }
    
    /**
     * Perform a single braid exchange.
     */
    void performBraidExchange() {
        braid_cycles_++;
        
        std::cout << "\n[TorusBraidV4] === Braid Exchange #" << braid_cycles_ << " ===" << std::endl;
        
        // Signal all tori to exchange
        should_exchange_.store(true, std::memory_order_release);
        
        // Wait for all tori to write projections
        sync_barrier_->arrive_and_wait();
        
        // Read projections (lock-free)
        ProjectionV3 proj_a = proj_buffer_a_.read();
        ProjectionV3 proj_b = proj_buffer_b_.read();
        ProjectionV3 proj_c = proj_buffer_c_.read();
        
        total_projection_exchanges_ += 3;
        
        // Apply constraints cyclically: A→B, B→C, C→A
        std::cout << "[TorusBraidV4] Applying constraints: A→B, B→C, C→A" << std::endl;
        
        bool success_b = torus_b_->applyConstraint(proj_a);
        bool success_c = torus_c_->applyConstraint(proj_b);
        bool success_a = torus_a_->applyConstraint(proj_c);
        
        // Update metrics
        total_boundary_violations_ += torus_a_->getBoundaryViolations() + 
                                       torus_b_->getBoundaryViolations() + 
                                       torus_c_->getBoundaryViolations();
        
        total_global_violations_ += torus_a_->getGlobalViolations() + 
                                     torus_b_->getGlobalViolations() + 
                                     torus_c_->getGlobalViolations();
        
        total_corrective_events_ += torus_a_->getCorrectiveEvents() + 
                                     torus_b_->getCorrectiveEvents() + 
                                     torus_c_->getCorrectiveEvents();
        
        // Detect failures
        detectAndRecoverFailures();
        
        // Adapt braid interval
        adaptBraidInterval();
        
        // Reset exchange flag
        should_exchange_.store(false, std::memory_order_release);
        
        // Release tori to continue execution
        sync_barrier_->arrive_and_wait();
        
        std::cout << "[TorusBraidV4] Braid exchange complete (interval=" << braid_interval_.load() << ")" << std::endl;
    }
    
    /**
     * Print comprehensive statistics.
     */
    void printStatistics() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_sec = std::chrono::duration<double>(end_time - start_time_).count();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║           TorusBraidV4 Performance Statistics                 ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Throughput
        uint64_t total_events = torus_a_->getEventsProcessed() + 
                                 torus_b_->getEventsProcessed() + 
                                 torus_c_->getEventsProcessed();
        
        double total_throughput = total_events / elapsed_sec;
        
        std::cout << "║ Elapsed Time:           " << std::setw(10) << std::fixed << std::setprecision(2) 
                  << elapsed_sec << " sec                  ║" << std::endl;
        std::cout << "║ Total Events:           " << std::setw(10) << total_events << "                          ║" << std::endl;
        std::cout << "║ Total Throughput:       " << std::setw(10) << std::fixed << std::setprecision(1) 
                  << (total_throughput / 1e6) << " M events/sec         ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Per-torus metrics
        std::cout << "║                        Torus A    Torus B    Torus C          ║" << std::endl;
        std::cout << "║ Events Processed:      " 
                  << std::setw(9) << torus_a_->getEventsProcessed() << "   "
                  << std::setw(9) << torus_b_->getEventsProcessed() << "   "
                  << std::setw(9) << torus_c_->getEventsProcessed() << "        ║" << std::endl;
        
        double throughput_a = torus_a_->getEventsProcessed() / elapsed_sec / 1e6;
        double throughput_b = torus_b_->getEventsProcessed() / elapsed_sec / 1e6;
        double throughput_c = torus_c_->getEventsProcessed() / elapsed_sec / 1e6;
        
        std::cout << "║ Throughput (M/sec):    " 
                  << std::setw(9) << std::fixed << std::setprecision(1) << throughput_a << "   "
                  << std::setw(9) << std::fixed << std::setprecision(1) << throughput_b << "   "
                  << std::setw(9) << std::fixed << std::setprecision(1) << throughput_c << "        ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Braid metrics
        std::cout << "║ Braid Cycles:           " << std::setw(10) << braid_cycles_ << "                          ║" << std::endl;
        std::cout << "║ Current Interval:       " << std::setw(10) << braid_interval_.load() << " ticks                   ║" << std::endl;
        std::cout << "║ Boundary Violations:    " << std::setw(10) << total_boundary_violations_.load() << "                          ║" << std::endl;
        std::cout << "║ Global Violations:      " << std::setw(10) << total_global_violations_.load() << "                          ║" << std::endl;
        std::cout << "║ Corrective Events:      " << std::setw(10) << total_corrective_events_.load() << "                          ║" << std::endl;
        
        std::cout << "╠════════════════════════════════════════════════════════════════╣" << std::endl;
        
        // Fault tolerance metrics
        std::cout << "║ Failures Detected:      " << std::setw(10) << total_failures_detected_.load() << "                          ║" << std::endl;
        std::cout << "║ Reconstructions:        " << std::setw(10) << total_reconstructions_.load() << "                          ║" << std::endl;
        std::cout << "║ Process Migrations:     " << std::setw(10) << total_migrations_.load() << "                          ║" << std::endl;
        
        std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
        
        // Speedup analysis
        double single_torus_baseline = 16.8e6;  // 16.8M events/sec
        double speedup = total_throughput / single_torus_baseline;
        double efficiency = speedup / 3.0 * 100.0;
        
        std::cout << "\n[Performance Analysis]" << std::endl;
        std::cout << "  Single-torus baseline: 16.8 M events/sec" << std::endl;
        std::cout << "  Braided speedup: " << std::fixed << std::setprecision(2) << speedup << "×" << std::endl;
        std::cout << "  Parallel efficiency: " << std::fixed << std::setprecision(1) << efficiency << "%" << std::endl;
    }
    
    // Getters for metrics
    uint64_t getTotalFailures() const { return total_failures_detected_.load(); }
    uint64_t getTotalReconstructions() const { return total_reconstructions_.load(); }
    uint64_t getTotalMigrations() const { return total_migrations_.load(); }
    uint64_t getBraidCycles() const { return braid_cycles_; }
    
private:
    /**
     * Worker thread for a single torus.
     */
    void torusWorker(int torus_id, BraidedKernelV3* torus) {
        ProjectionBuffer* proj_buffer = nullptr;
        if (torus_id == 0) proj_buffer = &proj_buffer_a_;
        else if (torus_id == 1) proj_buffer = &proj_buffer_b_;
        else proj_buffer = &proj_buffer_c_;
        
        std::cout << "[Torus " << torus_id << "] Worker thread started" << std::endl;
        
        while (running_.load(std::memory_order_acquire)) {
            // Execute torus tick
            torus->tick();
            total_ticks_[torus_id].fetch_add(1, std::memory_order_relaxed);
            
            // Check if braid exchange needed
            if (should_exchange_.load(std::memory_order_acquire)) {
                // Extract and write projection (lock-free)
                ProjectionV3 proj = torus->extractProjection();
                proj_buffer->write(proj);
                
                // Wait for coordinator
                sync_barrier_->arrive_and_wait();
                
                // Wait for constraints to be applied
                sync_barrier_->arrive_and_wait();
            }
        }
        
        std::cout << "[Torus " << torus_id << "] Worker thread stopped" << std::endl;
    }
    
    /**
     * Adapt braid interval based on violation rate.
     */
    void adaptBraidInterval() {
        if (braid_cycles_ < 10) {
            return;  // Need more data
        }
        
        double violation_rate = (total_boundary_violations_.load() + total_global_violations_.load()) / 
                                 static_cast<double>(braid_cycles_);
        
        uint64_t current_interval = braid_interval_.load();
        uint64_t new_interval = current_interval;
        
        if (violation_rate > VIOLATION_THRESHOLD) {
            // Too many violations → exchange more frequently
            new_interval = std::max(MIN_BRAID_INTERVAL, static_cast<uint64_t>(current_interval * 0.8));
            std::cout << "[TorusBraidV4] High violation rate (" << violation_rate 
                      << ") → decreasing interval to " << new_interval << std::endl;
        } else if (violation_rate < VIOLATION_THRESHOLD / 2) {
            // Few violations → exchange less frequently
            new_interval = std::min(MAX_BRAID_INTERVAL, static_cast<uint64_t>(current_interval * 1.2));
            std::cout << "[TorusBraidV4] Low violation rate (" << violation_rate 
                      << ") → increasing interval to " << new_interval << std::endl;
        }
        
        braid_interval_.store(new_interval, std::memory_order_relaxed);
        heartbeat_timeout_ = new_interval * 3;
    }
    
    /**
     * Detect and recover from failures (simplified for Phase 4).
     */
    void detectAndRecoverFailures() {
        // Simplified failure detection for Phase 4
        // Full implementation from Phase 3 can be integrated here
    }
};

} // namespace braided
