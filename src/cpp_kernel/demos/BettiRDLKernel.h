#pragma once

#include "../FixedStructures.h"
#include "../ToroidalSpace.h"

#include <algorithm>
#include <cstdint>
#ifdef RSE_KERNEL
#include "../os/KernelStubs.h"
#include <cstddef>
constexpr std::size_t LATTICE_SIZE = 32 * 32 * 32;
#else
#include "../Allocator.h"
#include <chrono>
#include <iostream>
#include <mutex>
#endif

// Betti-RDL Integration
// Combines toroidal space (Betti) with time-native events (RDL)

// RDL Event: timestamped message between processes
struct RDLEvent {
  unsigned long long timestamp;
  int dst_node;
  int src_node;
  int payload;

  // Canonical ordering for determinism
  bool operator<(const RDLEvent &other) const {
    if (timestamp != other.timestamp)
      return timestamp < other.timestamp;
    if (dst_node != other.dst_node)
      return dst_node < other.dst_node;
    if (src_node != other.src_node)
      return src_node < other.src_node;
    return payload < other.payload;
  }
};

// Betti-RDL Process: exists in toroidal space, processes RDL events
struct BettiRDLProcess {
  int pid;
  int x, y, z; // Position in toroidal space
  int state;   // Current state value

  BettiRDLProcess(int id, int px, int py, int pz)
      : pid(id), x(px), y(py), z(pz), state(0) {}
};

// Edge with adaptive delay (RDL concept)
struct AdaptiveEdge {
  int from_x, from_y, from_z;
  int to_x, to_y, to_z;
  unsigned long long delay; // Time delay (RDL memory)

  void updateDelay(int payload, unsigned long long /*current_time*/) {
    if (payload > 0) {
      delay = std::max(1ULL, delay - 1);
    } else {
      delay = delay + 1;
    }
  }
};

class BettiRDLKernel {
private:
  static constexpr int kDim = 32;
  static constexpr std::uint32_t kInvalidEdge = 0xFFFFFFFFu;

  static constexpr std::uint32_t nodeId(int x, int y, int z) {
    const int wx = ToroidalSpace<kDim, kDim, kDim>::wrap(x, kDim);
    const int wy = ToroidalSpace<kDim, kDim, kDim>::wrap(y, kDim);
    const int wz = ToroidalSpace<kDim, kDim, kDim>::wrap(z, kDim);
    return static_cast<std::uint32_t>(wx * 1024 + wy * 32 + wz);
  }

  static constexpr void decodeNode(std::uint32_t node, int &x, int &y, int &z) {
    x = static_cast<int>(node / 1024u);
    y = static_cast<int>((node % 1024u) / 32u);
    z = static_cast<int>(node % 32u);
  }

  static constexpr std::size_t kMaxPendingEvents = 8192;
  static constexpr std::size_t kMaxEdges = 8192;
  static constexpr std::size_t kMaxProcesses = 4096;

  ToroidalSpace<kDim, kDim, kDim> space;
  FixedMinHeap<RDLEvent, kMaxPendingEvents> event_queue;
  FixedObjectPool<BettiRDLProcess, kMaxProcesses> process_pool;

  struct EdgeEntry {
    std::uint32_t from_node;
    std::uint32_t to_node;
    AdaptiveEdge edge;
    std::uint32_t next_out = kInvalidEdge;
  };

  std::array<std::uint32_t, LATTICE_SIZE> out_head_{};
  std::array<std::uint32_t, LATTICE_SIZE> out_tail_{};
  std::array<EdgeEntry, kMaxEdges> edges_{};
  std::size_t edge_count_ = 0;

  unsigned long long current_time = 0;
  unsigned long long events_processed = 0;  // Lifetime total
  int process_counter = 0;

  // Thread-safety for concurrent injectEvent
  std::mutex event_injection_lock;
  FixedVector<RDLEvent, 16384> pending_events;

  [[nodiscard]] bool insertOrUpdateEdge(const AdaptiveEdge &edge) {
    const std::uint32_t from = nodeId(edge.from_x, edge.from_y, edge.from_z);
    const std::uint32_t to = nodeId(edge.to_x, edge.to_y, edge.to_z);

    // Check existing edges for this source (bounded, deterministic scan)
    for (std::uint32_t idx = out_head_[from]; idx != kInvalidEdge;
         idx = edges_[idx].next_out) {
      if (edges_[idx].to_node == to) {
        edges_[idx].edge = edge;
        return true;
      }
    }

    if (edge_count_ >= kMaxEdges) {
      assert(false && "AdaptiveEdge capacity exceeded");
      return false;
    }

    const std::uint32_t new_idx = static_cast<std::uint32_t>(edge_count_++);
    edges_[new_idx] = EdgeEntry{from, to, edge, kInvalidEdge};

    if (out_head_[from] == kInvalidEdge) {
      out_head_[from] = new_idx;
      out_tail_[from] = new_idx;
    } else {
      edges_[out_tail_[from]].next_out = new_idx;
      out_tail_[from] = new_idx;
    }

    return true;
  }

public:
  BettiRDLKernel() {
#ifndef RSE_KERNEL
    std::cout << "[BETTI-RDL] Initializing space-time kernel..." << std::endl;
    std::cout << "    > Spatial: ToroidalSpace<32,32,32>" << std::endl;
    std::cout << "    > Temporal: Event-driven with adaptive delays" << std::endl;
#endif

    out_head_.fill(kInvalidEdge);
    out_tail_.fill(kInvalidEdge);
  }

  bool spawnProcess(int x, int y, int z) {
    BettiRDLProcess *p = process_pool.create(++process_counter, x, y, z);
    if (!p) {
      return false;
    }
    return space.addProcess(reinterpret_cast<Process *>(p), x, y, z);
  }

  bool createEdge(int x1, int y1, int z1, int x2, int y2, int z2,
                  unsigned long long initial_delay) {
    AdaptiveEdge edge{};
    edge.from_x = x1;
    edge.from_y = y1;
    edge.from_z = z1;
    edge.to_x = x2;
    edge.to_y = y2;
    edge.to_z = z2;
    edge.delay = initial_delay;

    return insertOrUpdateEdge(edge);
  }

  bool injectEvent(int dst_x, int dst_y, int dst_z, int src_x, int src_y,
                   int src_z, int payload) {
    RDLEvent evt{};
    evt.timestamp = current_time;
    evt.dst_node = static_cast<int>(nodeId(dst_x, dst_y, dst_z));
    evt.src_node = static_cast<int>(nodeId(src_x, src_y, src_z));
    evt.payload = payload;

    // Thread-safe injection: add to pending queue
    {
      std::lock_guard<std::mutex> lock(event_injection_lock);
      if (!pending_events.push_back(evt)) {
        return false;
      }
    }
    return true;
  }

  // Transfer pending events to the main event queue (single-threaded from scheduler)
  void flushPendingEvents() {
    std::lock_guard<std::mutex> lock(event_injection_lock);
    for (std::size_t i = 0; i < pending_events.size(); ++i) {
      (void)event_queue.push(pending_events[i]);
    }
    pending_events.clear();
  }

  void tick() {
    if (event_queue.empty()) {
      return;
    }

    RDLEvent evt = event_queue.top();
    (void)event_queue.pop();

    current_time = evt.timestamp;
    events_processed++;

    // Iterate all outgoing edges from the destination node
    const std::uint32_t dst_node = static_cast<std::uint32_t>(evt.dst_node);
    for (std::uint32_t idx = out_head_[dst_node]; idx != kInvalidEdge;
         idx = edges_[idx].next_out) {
      EdgeEntry &entry = edges_[idx];

      entry.edge.updateDelay(evt.payload, current_time);

      RDLEvent new_evt{};
      new_evt.timestamp = current_time + entry.edge.delay;
      new_evt.dst_node = static_cast<int>(entry.to_node);
      new_evt.src_node = evt.dst_node;
      new_evt.payload = evt.payload + 1;

      (void)event_queue.push(new_evt);
    }
  }

  // Process at most max_events NEW events, returning the count processed
  // Does not depend on lifetime events_processed total
  int run(int max_events) {
#ifdef RSE_KERNEL
    flushPendingEvents();
    int events_in_run = 0;
    while (events_in_run < max_events && !event_queue.empty()) {
      tick();
      events_in_run++;
    }
    return events_in_run;
#else
    std::cout << "\n[BETTI-RDL] Starting execution..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    size_t mem_before = MemoryManager::getUsedMemory();

    // Flush any pending events from concurrent injections
    flushPendingEvents();

    int events_in_run = 0;
    while (events_in_run < max_events && !event_queue.empty()) {
      tick();
      events_in_run++;

      if (events_processed % 100000 == 0) {
        std::cout << "    > Events (lifetime): " << events_processed
                  << ", Events (this run): " << events_in_run
                  << ", Time: " << current_time
                  << ", Queue: " << event_queue.size() << std::endl;
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    size_t mem_after = MemoryManager::getUsedMemory();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n[BETTI-RDL] ✓ EXECUTION COMPLETE" << std::endl;
    std::cout << "    > Events Processed (this run): " << events_in_run << std::endl;
    std::cout << "    > Events Processed (lifetime): " << events_processed << std::endl;
    std::cout << "    > Final Time: " << current_time << std::endl;
    std::cout << "    > Processes: " << space.getProcessCount() << std::endl;
    std::cout << "    > Edges: " << edge_count_ << std::endl;
    std::cout << "    > Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "    > Memory Before: " << mem_before << " bytes" << std::endl;
    std::cout << "    > Memory After: " << mem_after << " bytes" << std::endl;
    std::cout << "    > Memory Delta: " << (mem_after - mem_before) << " bytes"
              << std::endl;
    if (duration.count() > 0) {
      std::cout << "    > Events/sec: "
                << (events_in_run * 1000.0 / duration.count()) << std::endl;
    }

    return events_in_run;
#endif
  }

  unsigned long long getCurrentTime() const { return current_time; }
  unsigned long long getEventsProcessed() const { return events_processed; }

  uint32_t getActiveProcessCount() const {
    return static_cast<uint32_t>(space.getProcessCount());
  }

  uint32_t getPendingEventCount() const {
    return static_cast<uint32_t>(event_queue.size() + pending_events.size());
  }

  uint32_t getEdgeCount() const {
    return static_cast<uint32_t>(edge_count_);
  }

  void fillBoundaryStates(uint32_t *out, size_t count) const {
    if (!out || count < static_cast<size_t>(kDim * kDim)) {
      return;
    }
    for (int y = 0; y < kDim; ++y) {
      for (int z = 0; z < kDim; ++z) {
        const size_t idx = static_cast<size_t>(y * kDim + z);
        out[idx] = static_cast<uint32_t>(space.getCellProcessCount(0, y, z));
      }
    }
  }
  
  /**
   * Reset kernel to initial state while preserving allocators.
   * Critical for Phase 3 reconstruction - maintains O(1) memory usage.
   */
  void reset() {
    // Clear event queue
    while (!event_queue.empty()) {
      (void)event_queue.pop();  // Ignore return value
    }
    
    // Clear pending events
    {
      std::lock_guard<std::mutex> lock(event_injection_lock);
      pending_events.clear();
    }
    
    // Clear process pool (but keep allocator)
    process_pool.clear();
    
    // Clear edges
    edge_count_ = 0;
    out_head_.fill(kInvalidEdge);
    out_tail_.fill(kInvalidEdge);
    
    // Clear toroidal space
    space.clear();
    
    // Reset counters
    current_time = 0;
    events_processed = 0;
    process_counter = 0;
    
    // Note: Allocators are NOT reset - they keep their memory pools
    // This maintains O(1) memory usage across multiple reset() calls
  }
};
