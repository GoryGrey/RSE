#pragma once

#include "../Allocator.h"
#include "../FixedStructures.h"
#include "../ToroidalSpace.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>

// Enhanced Betti-RDL with Real Computation
// Adds actual algorithm execution, not just event propagation

struct ComputeEvent {
  unsigned long long timestamp;
  int dst_node;
  int src_node;
  int value;

  bool operator<(const ComputeEvent &other) const {
    if (timestamp != other.timestamp)
      return timestamp < other.timestamp;
    if (dst_node != other.dst_node)
      return dst_node < other.dst_node;
    if (src_node != other.src_node)
      return src_node < other.src_node;
    return value < other.value;
  }
};

struct ComputeProcess {
  int pid;
  int x, y, z;
  int state;

  ComputeProcess(int id, int px, int py, int pz)
      : pid(id), x(px), y(py), z(pz), state(0) {}
};

class BettiRDLCompute {
private:
  static constexpr int kDim = 32;
  static constexpr std::size_t kMaxPendingEvents = 4096;
  static constexpr std::size_t kMaxProcesses = 2048;

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

  ToroidalSpace<kDim, kDim, kDim> space;
  FixedMinHeap<ComputeEvent, kMaxPendingEvents> event_queue;
  FixedObjectPool<ComputeProcess, kMaxProcesses> process_pool;

  std::array<int, LATTICE_SIZE> process_states_{};
  std::array<std::uint8_t, LATTICE_SIZE> process_active_{};
  std::size_t process_count_ = 0;

  unsigned long long current_time = 0;
  unsigned long long events_processed = 0;  // Lifetime total

  // Thread-safety for concurrent injectEvent
  std::mutex event_injection_lock;
  FixedVector<ComputeEvent, 16384> pending_events;

public:
  BettiRDLCompute() {
    std::cout << "[COMPUTE] Initializing Betti-RDL with real computation..."
              << std::endl;
  }

  bool spawnProcess(int x, int y, int z) {
    const std::uint32_t pid = nodeId(x, y, z);

    if (process_active_[pid] == 0) {
      process_active_[pid] = 1;
      ++process_count_;
    }
    process_states_[pid] = 0;

    ComputeProcess *p = process_pool.create(static_cast<int>(pid), x, y, z);
    if (!p) {
      return false;
    }
    return space.addProcess(reinterpret_cast<Process *>(p), x, y, z);
  }

  bool injectEvent(int dst_x, int dst_y, int dst_z, int value) {
    ComputeEvent evt{};
    evt.timestamp = current_time;
    evt.dst_node = static_cast<int>(nodeId(dst_x, dst_y, dst_z));
    evt.src_node = 0;
    evt.value = value;

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
  private:
  void flushPendingEvents() {
    std::lock_guard<std::mutex> lock(event_injection_lock);
    for (std::size_t i = 0; i < pending_events.size(); ++i) {
      (void)event_queue.push(pending_events[i]);
    }
    pending_events.clear();
  }

  public:

  void tick() {
    if (event_queue.empty()) {
      return;
    }

    ComputeEvent evt = event_queue.top();
    (void)event_queue.pop();

    current_time = evt.timestamp;
    events_processed++;

    // Decode spatial coordinates
    int dst_x, dst_y, dst_z;
    decodeNode(static_cast<std::uint32_t>(evt.dst_node), dst_x, dst_y, dst_z);

    // REAL COMPUTATION: Accumulate value in the destination process state
    const std::uint32_t pid = static_cast<std::uint32_t>(evt.dst_node);
    if (process_active_[pid] != 0) {
      process_states_[pid] += evt.value;
    }

    // Propagate to neighbors (with computation)
    int next_x = (dst_x + 1) % kDim;
    if (next_x < 10) {
      ComputeEvent new_evt{};
      new_evt.timestamp = current_time + 1;
      new_evt.dst_node = static_cast<int>(nodeId(next_x, dst_y, dst_z));
      new_evt.src_node = evt.dst_node;
      new_evt.value = evt.value + 1;

      (void)event_queue.push(new_evt);
    }
  }

  // Process at most max_events NEW events, returning the count processed
  // Does not depend on lifetime events_processed total
  int run(int max_events) {
    // Flush any pending events from concurrent injections
    flushPendingEvents();

    int events_in_run = 0;
    while (events_in_run < max_events && !event_queue.empty()) {
      tick();
      events_in_run++;
    }

    return events_in_run;
  }

  int getProcessState(int pid) const {
    const std::uint32_t idx = static_cast<std::uint32_t>(pid);
    if (idx >= LATTICE_SIZE) {
      return 0;
    }
    return process_active_[idx] ? process_states_[idx] : 0;
  }

  unsigned long long getCurrentTime() const { return current_time; }
  unsigned long long getEventsProcessed() const { return events_processed; }
  std::size_t getProcessCount() const { return process_count_; }
};
