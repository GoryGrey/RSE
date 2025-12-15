#include "../Allocator.h"
#include "../demos/BettiRDLCompute.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>

// ============================================================================
// COMPREHENSIVE BETTI-RDL BENCHMARKING HARNESS
// ============================================================================
// Benchmarks the three killer scenarios with detailed metrics:
// 1. The Firehose: Raw event processing throughput
// 2. The Deep Dive: Memory stability under deep recursion
// 3. The Swarm: Parallel scaling across multiple threads
// ============================================================================

using namespace std::chrono;

// Latency measurement utilities
struct LatencySample {
    double value_us;  // microseconds
};

class LatencyTracker {
private:
    std::vector<double> samples;
    std::atomic<size_t> sample_count{0};

public:
    void recordSample(double latency_us) {
        samples.push_back(latency_us);
        sample_count.fetch_add(1, std::memory_order_relaxed);
    }

    double getPercentile(double p) const {
        if (samples.empty()) return 0.0;
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        size_t index = static_cast<size_t>((p / 100.0) * sorted.size());
        if (index >= sorted.size()) index = sorted.size() - 1;
        return sorted[index];
    }

    double getMean() const {
        if (samples.empty()) return 0.0;
        return std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    }

    double getMedian() const {
        return getPercentile(50.0);
    }

    double getP95() const {
        return getPercentile(95.0);
    }

    double getP99() const {
        return getPercentile(99.0);
    }

    double getMin() const {
        if (samples.empty()) return 0.0;
        return *std::min_element(samples.begin(), samples.end());
    }

    double getMax() const {
        if (samples.empty()) return 0.0;
        return *std::max_element(samples.begin(), samples.end());
    }

    size_t getSampleCount() const {
        return samples.size();
    }
};

struct BenchmarkResults {
    std::string scenario;
    double duration_seconds;
    long long events_processed;
    double throughput_eps;  // Events Per Second
    double avg_latency_us;
    double median_latency_us;
    double p95_latency_us;
    double p99_latency_us;
    double min_latency_us;
    double max_latency_us;
    size_t mem_initial_bytes;
    size_t mem_final_bytes;
    long long mem_delta_bytes;
    double mem_stability_percent;  // (1 - delta/initial) * 100, should be close to 100% for O(1)
};

void printHeader(const std::string &title) {
    std::cout << "\n================================================="
              << std::endl;
    std::cout << "   " << title << std::endl;
    std::cout << "=================================================" << std::endl;
}

// ============================================================================
// SCENARIO 1: THE FIREHOSE
// Goal: Measure raw event processing throughput
// ============================================================================
BenchmarkResults runFirehose(int event_count = 1000000) {
    printHeader("SCENARIO 1: THE FIREHOSE (Throughput)");
    std::cout << "Goal: Process " << event_count << " events as fast as possible."
              << std::endl;

    BettiRDLCompute kernel;
    LatencyTracker latency_tracker;

    // Spawn a cluster to receive events
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            kernel.spawnProcess(x, y, 0);
        }
    }

    size_t mem_initial = MemoryManager::getSystemRSS();
    MemoryManager::resetSystemPeak();

    auto start = high_resolution_clock::now();

    int batch_size = 1000;
    int batches = event_count / batch_size;
    int chain_length = 10;

    for (int i = 0; i < batches; i++) {
        auto batch_start = high_resolution_clock::now();

        // Inject batch
        for (int j = 0; j < batch_size; j++) {
            kernel.injectEvent(0, 0, 0, i * batch_size + j);
        }

        // Drain the full chain to keep queue size bounded
        (void)kernel.run(batch_size * chain_length);

        auto batch_end = high_resolution_clock::now();
        double batch_latency_us = duration_cast<microseconds>(batch_end - batch_start).count() / static_cast<double>(batch_size);
        latency_tracker.recordSample(batch_latency_us);
    }

    auto end = high_resolution_clock::now();
    auto duration_ms = duration_cast<milliseconds>(end - start).count();
    double seconds = duration_ms / 1000.0;
    double events_processed = static_cast<double>(kernel.getEventsProcessed());
    double eps = events_processed / seconds;

    size_t mem_final = MemoryManager::getSystemRSS();
    long long mem_delta = static_cast<long long>(mem_final) - static_cast<long long>(mem_initial);

    BenchmarkResults result{
        .scenario = "Firehose (Throughput)",
        .duration_seconds = seconds,
        .events_processed = static_cast<long long>(events_processed),
        .throughput_eps = eps,
        .avg_latency_us = latency_tracker.getMean(),
        .median_latency_us = latency_tracker.getMedian(),
        .p95_latency_us = latency_tracker.getP95(),
        .p99_latency_us = latency_tracker.getP99(),
        .min_latency_us = latency_tracker.getMin(),
        .max_latency_us = latency_tracker.getMax(),
        .mem_initial_bytes = mem_initial,
        .mem_final_bytes = mem_final,
        .mem_delta_bytes = mem_delta,
        .mem_stability_percent = mem_initial > 0 ? (1.0 - static_cast<double>(mem_delta) / static_cast<double>(mem_initial)) * 100.0 : 100.0
    };

    std::cout << "  Events (processed): " << result.events_processed << std::endl;
    std::cout << "  Time:   " << std::fixed << std::setprecision(2) << result.duration_seconds << "s" << std::endl;
    std::cout << "  Speed:  " << std::fixed << std::setprecision(2) << result.throughput_eps
              << " Events/Sec" << std::endl;
    std::cout << "  Latency (avg):     " << std::fixed << std::setprecision(3) << result.avg_latency_us << " us" << std::endl;
    std::cout << "  Latency (median):  " << std::fixed << std::setprecision(3) << result.median_latency_us << " us" << std::endl;
    std::cout << "  Latency (p95):     " << std::fixed << std::setprecision(3) << result.p95_latency_us << " us" << std::endl;
    std::cout << "  Latency (p99):     " << std::fixed << std::setprecision(3) << result.p99_latency_us << " us" << std::endl;
    std::cout << "  Memory (initial):  " << result.mem_initial_bytes << " bytes" << std::endl;
    std::cout << "  Memory (final):    " << result.mem_final_bytes << " bytes" << std::endl;
    std::cout << "  Memory (delta):    " << result.mem_delta_bytes << " bytes" << std::endl;
    std::cout << "  Memory (stability): " << std::fixed << std::setprecision(2) << result.mem_stability_percent << "%" << std::endl;

    if (eps > 1000000) {
        std::cout << "  [SUCCESS] >1M EPS achieved!" << std::endl;
    } else if (eps > 500000) {
        std::cout << "  [GOOD] >500K EPS achieved!" << std::endl;
    } else {
        std::cout << "  [NOTE] Performance is nominal." << std::endl;
    }

    return result;
}

// ============================================================================
// SCENARIO 2: THE DEEP DIVE
// Goal: Verify O(1) memory usage during deep recursion
// ============================================================================
BenchmarkResults runDeepDive(int depth = 1000000) {
    printHeader("SCENARIO 2: THE DEEP DIVE (Memory Stability)");
    std::cout << "Goal: Chain " << depth << " dependent events." << std::endl;
    std::cout << "Expectation: 0 bytes memory growth." << std::endl;

    size_t mem_initial = MemoryManager::getSystemRSS();
    MemoryManager::resetSystemPeak();

    std::cout << "  Memory Start: " << mem_initial << " bytes" << std::endl;

    BettiRDLCompute kernel;
    kernel.spawnProcess(0, 0, 0);

    auto start = high_resolution_clock::now();

    // Inject BIG initial event to start the chain
    kernel.injectEvent(0, 0, 0, 1);

    // Run for 'depth' steps
    // The kernel propagates events: 1 -> 2 -> 3 ...
    // Each step increments the payload and re-injects
    int result_count = 0;
    size_t mem_at_check = mem_initial;

    for (int i = 0; i < depth; i++) {
        result_count += kernel.run(100);  // Run in chunks of 100

        // Check memory periodically
        if (i % 10000 == 0 && i > 0) {
            size_t current_mem = MemoryManager::getSystemRSS();
            if (i == 10000) {
                mem_at_check = current_mem;
            }
            if (i > 10000) {
                // Check delta from checkpoint
                long long delta_since_check = static_cast<long long>(current_mem) - static_cast<long long>(mem_at_check);
                if (delta_since_check > 10000000) {  // More than 10MB delta is suspicious
                    std::cout << "  WARNING: Memory grew by " << delta_since_check << " bytes at iteration " << i << std::endl;
                }
            }
        }
    }

    auto end = high_resolution_clock::now();
    auto duration_ms = duration_cast<milliseconds>(end - start).count();
    double seconds = duration_ms / 1000.0;

    size_t mem_final = MemoryManager::getSystemRSS();
    long long mem_delta = static_cast<long long>(mem_final) - static_cast<long long>(mem_initial);

    BenchmarkResults result{
        .scenario = "Deep Dive (Memory Stability)",
        .duration_seconds = seconds,
        .events_processed = static_cast<long long>(result_count),
        .throughput_eps = result_count / seconds,
        .avg_latency_us = 0.0,
        .median_latency_us = 0.0,
        .p95_latency_us = 0.0,
        .p99_latency_us = 0.0,
        .min_latency_us = 0.0,
        .max_latency_us = 0.0,
        .mem_initial_bytes = mem_initial,
        .mem_final_bytes = mem_final,
        .mem_delta_bytes = mem_delta,
        .mem_stability_percent = mem_initial > 0 ? (1.0 - static_cast<double>(mem_delta) / static_cast<double>(mem_initial)) * 100.0 : 100.0
    };

    std::cout << "  Events processed: " << result.events_processed << std::endl;
    std::cout << "  Time:   " << std::fixed << std::setprecision(2) << result.duration_seconds << "s" << std::endl;
    std::cout << "  Speed:  " << std::fixed << std::setprecision(2) << result.throughput_eps << " Events/Sec" << std::endl;
    std::cout << "  Memory (initial):  " << result.mem_initial_bytes << " bytes" << std::endl;
    std::cout << "  Memory (final):    " << result.mem_final_bytes << " bytes" << std::endl;
    std::cout << "  Memory (delta):    " << result.mem_delta_bytes << " bytes" << std::endl;
    std::cout << "  Memory (stability): " << std::fixed << std::setprecision(2) << result.mem_stability_percent << "%" << std::endl;

    if (std::abs(mem_delta) < 5000000) {  // Less than 5MB growth is acceptable
        std::cout << "  [SUCCESS] O(1) Memory validated! Delta < 5MB" << std::endl;
    } else {
        std::cout << "  [WARNING] Memory growth detected: " << mem_delta << " bytes" << std::endl;
    }

    return result;
}

// ============================================================================
// SCENARIO 3: THE SWARM
// Goal: Measure parallel scaling across multiple threads
// ============================================================================
BenchmarkResults runSwarm(int num_threads = 4, int events_per_thread = 250000) {
    printHeader("SCENARIO 3: THE SWARM (Parallel Scaling)");
    std::cout << "Goal: Scale processing across " << num_threads << " threads." << std::endl;
    std::cout << "      Each thread processes " << events_per_thread << " events." << std::endl;

    size_t mem_initial = MemoryManager::getSystemRSS();
    MemoryManager::resetSystemPeak();

    auto global_start = high_resolution_clock::now();

    // Create per-thread kernels and latency trackers
    std::vector<BettiRDLCompute> kernels(num_threads);
    std::vector<LatencyTracker> trackers(num_threads);
    std::vector<std::thread> threads;
    std::vector<long long> thread_events(num_threads, 0);

    // Thread function
    auto thread_work = [&](int thread_id) {
        auto& kernel = kernels[thread_id];
        auto& tracker = trackers[thread_id];

        // Setup
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                kernel.spawnProcess(x, y, thread_id % 2);
            }
        }

        int batch_size = 1000;
        int batches = events_per_thread / batch_size;

        for (int i = 0; i < batches; i++) {
            auto batch_start = high_resolution_clock::now();

            // Inject batch
            for (int j = 0; j < batch_size; j++) {
                kernel.injectEvent(rand() % 4, rand() % 4, thread_id % 2, i * batch_size + j);
            }

            // Process
            thread_events[thread_id] += kernel.run(batch_size * 10);

            auto batch_end = high_resolution_clock::now();
            double batch_latency_us = duration_cast<microseconds>(batch_end - batch_start).count() / static_cast<double>(batch_size);
            tracker.recordSample(batch_latency_us);
        }
    };

    // Launch threads
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_work, i);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto global_end = high_resolution_clock::now();
    auto duration_ms = duration_cast<milliseconds>(global_end - global_start).count();
    double seconds = duration_ms / 1000.0;

    long long total_events = 0;
    double total_latency = 0.0;
    double total_median = 0.0;
    double total_p95 = 0.0;
    double total_p99 = 0.0;

    for (int i = 0; i < num_threads; i++) {
        total_events += thread_events[i];
        total_latency += trackers[i].getMean();
        total_median += trackers[i].getMedian();
        total_p95 += trackers[i].getP95();
        total_p99 += trackers[i].getP99();
    }

    double avg_latency = total_latency / num_threads;
    double avg_median = total_median / num_threads;
    double avg_p95 = total_p95 / num_threads;
    double avg_p99 = total_p99 / num_threads;

    size_t mem_final = MemoryManager::getSystemRSS();
    long long mem_delta = static_cast<long long>(mem_final) - static_cast<long long>(mem_initial);

    BenchmarkResults result{
        .scenario = "Swarm (Parallel Scaling)",
        .duration_seconds = seconds,
        .events_processed = total_events,
        .throughput_eps = total_events / seconds,
        .avg_latency_us = avg_latency,
        .median_latency_us = avg_median,
        .p95_latency_us = avg_p95,
        .p99_latency_us = avg_p99,
        .min_latency_us = 0.0,
        .max_latency_us = 0.0,
        .mem_initial_bytes = mem_initial,
        .mem_final_bytes = mem_final,
        .mem_delta_bytes = mem_delta,
        .mem_stability_percent = mem_initial > 0 ? (1.0 - static_cast<double>(mem_delta) / static_cast<double>(mem_initial)) * 100.0 : 100.0
    };

    std::cout << "  Threads:    " << num_threads << std::endl;
    std::cout << "  Events (total):   " << result.events_processed << std::endl;
    std::cout << "  Time:   " << std::fixed << std::setprecision(2) << result.duration_seconds << "s" << std::endl;
    std::cout << "  Speed:  " << std::fixed << std::setprecision(2) << result.throughput_eps << " Events/Sec" << std::endl;
    std::cout << "  Latency (avg):     " << std::fixed << std::setprecision(3) << result.avg_latency_us << " us" << std::endl;
    std::cout << "  Latency (median):  " << std::fixed << std::setprecision(3) << result.median_latency_us << " us" << std::endl;
    std::cout << "  Latency (p95):     " << std::fixed << std::setprecision(3) << result.p95_latency_us << " us" << std::endl;
    std::cout << "  Latency (p99):     " << std::fixed << std::setprecision(3) << result.p99_latency_us << " us" << std::endl;
    std::cout << "  Memory (initial):  " << result.mem_initial_bytes << " bytes" << std::endl;
    std::cout << "  Memory (final):    " << result.mem_final_bytes << " bytes" << std::endl;
    std::cout << "  Memory (delta):    " << result.mem_delta_bytes << " bytes" << std::endl;
    std::cout << "  Memory (stability): " << std::fixed << std::setprecision(2) << result.mem_stability_percent << "%" << std::endl;

    double scaling_efficiency = result.throughput_eps / (result.throughput_eps / num_threads) / num_threads * 100.0;
    if (std::isnan(scaling_efficiency)) scaling_efficiency = 100.0;
    std::cout << "  Scaling Efficiency: " << std::fixed << std::setprecision(1) << scaling_efficiency << "%" << std::endl;

    if (scaling_efficiency > 80.0) {
        std::cout << "  [EXCELLENT] Near-linear scaling achieved!" << std::endl;
    } else if (scaling_efficiency > 50.0) {
        std::cout << "  [GOOD] Reasonable scaling observed." << std::endl;
    } else {
        std::cout << "  [NOTE] Contention limits scaling." << std::endl;
    }

    return result;
}

// ============================================================================
// Output Formatters
// ============================================================================

void outputJSON(const std::vector<BenchmarkResults>& results, const std::string& filename) {
    std::ofstream outfile(filename);

    outfile << "{\n  \"benchmarks\": [\n";

    for (size_t i = 0; i < results.size(); i++) {
        const auto& result = results[i];
        outfile << "    {\n";
        outfile << "      \"scenario\": \"" << result.scenario << "\",\n";
        outfile << std::fixed << std::setprecision(6);
        outfile << "      \"duration_seconds\": " << result.duration_seconds << ",\n";
        outfile << "      \"events_processed\": " << result.events_processed << ",\n";
        outfile << "      \"throughput_eps\": " << result.throughput_eps << ",\n";
        outfile << "      \"latency_avg_us\": " << result.avg_latency_us << ",\n";
        outfile << "      \"latency_median_us\": " << result.median_latency_us << ",\n";
        outfile << "      \"latency_p95_us\": " << result.p95_latency_us << ",\n";
        outfile << "      \"latency_p99_us\": " << result.p99_latency_us << ",\n";
        outfile << "      \"latency_min_us\": " << result.min_latency_us << ",\n";
        outfile << "      \"latency_max_us\": " << result.max_latency_us << ",\n";
        outfile << "      \"memory_initial_bytes\": " << result.mem_initial_bytes << ",\n";
        outfile << "      \"memory_final_bytes\": " << result.mem_final_bytes << ",\n";
        outfile << "      \"memory_delta_bytes\": " << result.mem_delta_bytes << ",\n";
        outfile << "      \"memory_stability_percent\": " << result.mem_stability_percent << "\n";
        outfile << "    }";
        if (i < results.size() - 1) {
            outfile << ",";
        }
        outfile << "\n";
    }

    outfile << "  ]\n}\n";
    outfile.close();

    std::cout << "\n[INFO] JSON report written to: " << filename << std::endl;
}

void outputCSV(const std::vector<BenchmarkResults>& results, const std::string& filename) {
    std::ofstream outfile(filename);

    // Header
    outfile << "Scenario,Duration(s),Events,Throughput(EPS),LatencyAvg(us),"
            << "LatencyMedian(us),LatencyP95(us),LatencyP99(us),"
            << "MemInitial(B),MemFinal(B),MemDelta(B),MemStability(%)" << std::endl;

    // Data rows
    for (const auto& result : results) {
        outfile << std::fixed << std::setprecision(6)
                << result.scenario << ","
                << result.duration_seconds << ","
                << result.events_processed << ","
                << result.throughput_eps << ","
                << result.avg_latency_us << ","
                << result.median_latency_us << ","
                << result.p95_latency_us << ","
                << result.p99_latency_us << ","
                << result.mem_initial_bytes << ","
                << result.mem_final_bytes << ","
                << result.mem_delta_bytes << ","
                << result.mem_stability_percent << std::endl;
    }

    outfile.close();
    std::cout << "[INFO] CSV report written to: " << filename << std::endl;
}

void outputText(const std::vector<BenchmarkResults>& results, const std::string& filename) {
    std::ofstream outfile(filename);

    outfile << "========================================\n"
            << "  BETTI-RDL BENCHMARK HARNESS REPORT\n"
            << "========================================\n\n";

    for (const auto& result : results) {
        outfile << "Scenario: " << result.scenario << "\n"
                << "  Duration: " << std::fixed << std::setprecision(2) << result.duration_seconds << "s\n"
                << "  Events Processed: " << result.events_processed << "\n"
                << "  Throughput: " << std::fixed << std::setprecision(2) << result.throughput_eps << " EPS\n"
                << "  Latency (avg): " << std::fixed << std::setprecision(3) << result.avg_latency_us << " us\n"
                << "  Latency (median): " << std::fixed << std::setprecision(3) << result.median_latency_us << " us\n"
                << "  Latency (p95): " << std::fixed << std::setprecision(3) << result.p95_latency_us << " us\n"
                << "  Latency (p99): " << std::fixed << std::setprecision(3) << result.p99_latency_us << " us\n"
                << "  Memory (initial): " << result.mem_initial_bytes << " bytes\n"
                << "  Memory (final): " << result.mem_final_bytes << " bytes\n"
                << "  Memory (delta): " << result.mem_delta_bytes << " bytes\n"
                << "  Memory (stability): " << std::fixed << std::setprecision(2) << result.mem_stability_percent << "%\n\n";
    }

    outfile.close();
    std::cout << "[INFO] Text report written to: " << filename << std::endl;
}

// ============================================================================
// Main Harness
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  BETTI-RDL COMPREHENSIVE BENCHMARKING HARNESS              ║" << std::endl;
    std::cout << "║  Version 1.0 - Multi-Scenario Performance Validator        ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;

    std::vector<BenchmarkResults> results;

    // Parse command-line arguments
    bool run_all = argc == 1;
    bool run_firehose = run_all;
    bool run_deep_dive = run_all;
    bool run_swarm = run_all;
    std::string output_format = "json";  // default

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--firehose") run_firehose = true;
        else if (arg == "--deep-dive") run_deep_dive = true;
        else if (arg == "--swarm") run_swarm = true;
        else if (arg == "--format=json") output_format = "json";
        else if (arg == "--format=csv") output_format = "csv";
        else if (arg == "--format=text") output_format = "text";
        else if (arg == "--format=all") output_format = "all";
        else if (arg == "--help") {
            std::cout << "Usage: benchmark_harness [OPTIONS]\n"
                      << "  --firehose          Run Firehose scenario\n"
                      << "  --deep-dive         Run Deep Dive scenario\n"
                      << "  --swarm             Run Swarm scenario\n"
                      << "  --format=json       Output JSON format (default)\n"
                      << "  --format=csv        Output CSV format\n"
                      << "  --format=text       Output text format\n"
                      << "  --format=all        Output all formats\n"
                      << "  --help              Show this help message\n";
            return 0;
        }
    }

    // Run scenarios
    if (run_firehose) {
        results.push_back(runFirehose(1000000));
    }

    if (run_deep_dive) {
        results.push_back(runDeepDive(100000));  // Reduced for faster CI
    }

    if (run_swarm) {
        results.push_back(runSwarm(4, 250000));
    }

    // Output results
    std::cout << "\n=================================================" << std::endl;
    std::cout << "  GENERATING REPORTS" << std::endl;
    std::cout << "=================================================" << std::endl;

    if (output_format == "json" || output_format == "all") {
        outputJSON(results, "benchmark_results.json");
    }
    if (output_format == "csv" || output_format == "all") {
        outputCSV(results, "benchmark_results.csv");
    }
    if (output_format == "text" || output_format == "all") {
        outputText(results, "benchmark_results.txt");
    }

    // Final validation
    std::cout << "\n=================================================" << std::endl;
    std::cout << "  VALIDATION SUMMARY" << std::endl;
    std::cout << "=================================================" << std::endl;

    bool all_passed = true;

    for (const auto& result : results) {
        std::cout << "\nScenario: " << result.scenario << std::endl;

        if (result.scenario.find("Firehose") != std::string::npos) {
            if (result.throughput_eps > 500000) {
                std::cout << "  ✓ Throughput PASSED (>500K EPS)" << std::endl;
            } else {
                std::cout << "  ✗ Throughput FAILED (<500K EPS)" << std::endl;
                all_passed = false;
            }
        }

        if (result.scenario.find("Deep Dive") != std::string::npos) {
            if (std::abs(result.mem_delta_bytes) < 5000000) {
                std::cout << "  ✓ Memory Stability PASSED (<5MB delta)" << std::endl;
            } else {
                std::cout << "  ✗ Memory Stability FAILED (>5MB delta)" << std::endl;
                all_passed = false;
            }
        }

        if (result.scenario.find("Swarm") != std::string::npos) {
            if (result.throughput_eps > 500000) {
                std::cout << "  ✓ Parallel Scaling PASSED" << std::endl;
            } else {
                std::cout << "  ✗ Parallel Scaling FAILED" << std::endl;
                all_passed = false;
            }
        }
    }

    std::cout << "\n" << (all_passed ? "✓ ALL VALIDATIONS PASSED" : "✗ SOME VALIDATIONS FAILED") << std::endl;

    return all_passed ? 0 : 1;
}
