#pragma once
#include "../Allocator.h"
#include "../ToroidalSpace.h"
#include <chrono>
#include <functional>
#include <map>
#include <queue>


// Betti-RDL Integration
// Combines toroidal space (Betti) with time-native events (RDL)

// RDL Event: timestamped message between processes
struct RDLEvent {
  unsigned long long timestamp;
  int dst_node;
  int src_node;
  int payload;

  // Canonical ordering for determinism
  bool operator>(const RDLEvent &other) const {
    if (timestamp != other.timestamp)
      return timestamp > other.timestamp;
    if (dst_node != other.dst_node)
      return dst_node > other.dst_node;
    return src_node > other.src_node;
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

  // Adaptive delay rule: delays can change based on usage
  void updateDelay(int payload, unsigned long long current_time) {
    // Example: reinforce frequently used paths (decrease delay)
    if (payload > 0) {
      delay = std::max(1ULL, delay - 1); // Faster with use
    } else {
      delay = delay + 1; // Slower without use
    }
  }
};

class BettiRDLKernel {
private:
  ToroidalSpace<32, 32, 32> space; // Betti: fixed spatial arena
  std::priority_queue<RDLEvent, std::vector<RDLEvent>, std::greater<RDLEvent>>
      event_queue;                           // RDL: time-ordered events
  std::map<std::string, AdaptiveEdge> edges; // RDL: adaptive delays

  unsigned long long current_time = 0;
  unsigned long long events_processed = 0;
  int process_counter = 0;

  std::string edge_key(int x1, int y1, int z1, int x2, int y2, int z2) {
    return std::to_string(x1) + "," + std::to_string(y1) + "," +
           std::to_string(z1) + "->" + std::to_string(x2) + "," +
           std::to_string(y2) + "," + std::to_string(z2);
  }

public:
  BettiRDLKernel() {
    std::cout << "[BETTI-RDL] Initializing space-time kernel..." << std::endl;
    std::cout << "    > Spatial: ToroidalSpace<32,32,32>" << std::endl;
    std::cout << "    > Temporal: Event-driven with adaptive delays"
              << std::endl;
  }

  // Spawn a process in toroidal space
  void spawnProcess(int x, int y, int z) {
    BettiRDLProcess *p = new BettiRDLProcess(++process_counter, x, y, z);
    space.addProcess((Process *)p, x, y, z);
  }

  // Create an adaptive edge between two spatial locations
  void createEdge(int x1, int y1, int z1, int x2, int y2, int z2,
                  unsigned long long initial_delay) {
    AdaptiveEdge edge;
    edge.from_x = x1;
    edge.from_y = y1;
    edge.from_z = z1;
    edge.to_x = x2;
    edge.to_y = y2;
    edge.to_z = z2;
    edge.delay = initial_delay;

    edges[edge_key(x1, y1, z1, x2, y2, z2)] = edge;
  }

  // Inject an event into the system
  void injectEvent(int dst_x, int dst_y, int dst_z, int src_x, int src_y,
                   int src_z, int payload) {
    RDLEvent evt;
    evt.timestamp = current_time;
    evt.dst_node = dst_x * 1024 + dst_y * 32 + dst_z; // Spatial hash
    evt.src_node = src_x * 1024 + src_y * 32 + src_z;
    evt.payload = payload;

    event_queue.push(evt);
  }

  // Process events (RDL execution model)
  void tick() {
    if (event_queue.empty())
      return;

    // Get next event in canonical order
    RDLEvent evt = event_queue.top();
    event_queue.pop();

    // Advance time to event timestamp
    current_time = evt.timestamp;
    events_processed++;

    // Decode spatial coordinates
    int dst_x = evt.dst_node / 1024;
    int dst_y = (evt.dst_node % 1024) / 32;
    int dst_z = evt.dst_node % 32;

    int src_x = evt.src_node / 1024;
    int src_y = (evt.src_node % 1024) / 32;
    int src_z = evt.src_node % 32;

    // Process event (simple: increment state)
    // In real system, this would execute node rule

    // Propagate to neighbors via adaptive edges
    std::string key =
        edge_key(dst_x, dst_y, dst_z, (dst_x + 1) % 32, dst_y, dst_z);

    if (edges.find(key) != edges.end()) {
      AdaptiveEdge &edge = edges[key];

      // Update delay based on usage (RDL adaptive behavior)
      edge.updateDelay(evt.payload, current_time);

      // Create new event with delay
      RDLEvent new_evt;
      new_evt.timestamp = current_time + edge.delay;
      new_evt.dst_node = edge.to_x * 1024 + edge.to_y * 32 + edge.to_z;
      new_evt.src_node = evt.dst_node;
      new_evt.payload = evt.payload + 1;

      event_queue.push(new_evt);
    }
  }

  void run(int max_events) {
    std::cout << "\n[BETTI-RDL] Starting execution..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    size_t mem_before = MemoryManager::getUsedMemory();

    while (events_processed < max_events && !event_queue.empty()) {
      tick();

      if (events_processed % 100000 == 0) {
        std::cout << "    > Events: " << events_processed
                  << ", Time: " << current_time
                  << ", Queue: " << event_queue.size() << std::endl;
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    size_t mem_after = MemoryManager::getUsedMemory();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n[BETTI-RDL] ✓ EXECUTION COMPLETE" << std::endl;
    std::cout << "    > Events Processed: " << events_processed << std::endl;
    std::cout << "    > Final Time: " << current_time << std::endl;
    std::cout << "    > Processes: " << space.getProcessCount() << std::endl;
    std::cout << "    > Edges: " << edges.size() << std::endl;
    std::cout << "    > Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "    > Memory Before: " << mem_before << " bytes" << std::endl;
    std::cout << "    > Memory After: " << mem_after << " bytes" << std::endl;
    std::cout << "    > Memory Delta: " << (mem_after - mem_before) << " bytes"
              << std::endl;
    std::cout << "    > Events/sec: "
              << (events_processed * 1000.0 / duration.count()) << std::endl;
  }

  unsigned long long getCurrentTime() const { return current_time; }
  unsigned long long getEventsProcessed() const { return events_processed; }
};
