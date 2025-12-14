#pragma once
#include "../Allocator.h"
#include "../ToroidalSpace.h"
#include <chrono>
#include <functional>
#include <map>
#include <queue>

// Enhanced Betti-RDL with Real Computation
// Adds actual algorithm execution, not just event propagation

struct ComputeEvent {
  unsigned long long timestamp;
  int dst_node;
  int src_node;
  int value; // Actual data payload

  bool operator>(const ComputeEvent &other) const {
    if (timestamp != other.timestamp)
      return timestamp > other.timestamp;
    if (dst_node != other.dst_node)
      return dst_node > other.dst_node;
    return src_node > other.src_node;
  }
};

struct ComputeProcess {
  int pid;
  int x, y, z;
  int state; // Accumulated value

  ComputeProcess(int id, int px, int py, int pz)
      : pid(id), x(px), y(py), z(pz), state(0) {}
};

class BettiRDLCompute {
private:
  ToroidalSpace<32, 32, 32> space;
  std::priority_queue<ComputeEvent, std::vector<ComputeEvent>,
                      std::greater<ComputeEvent>>
      event_queue;
  std::map<int, int> process_states; // pid -> accumulated value

  unsigned long long current_time = 0;
  unsigned long long events_processed = 0;
  int process_counter = 0;

public:
  BettiRDLCompute() {
    std::cout << "[COMPUTE] Initializing Betti-RDL with real computation..."
              << std::endl;
  }

  void spawnProcess(int x, int y, int z) {
    ComputeProcess *p = new ComputeProcess(++process_counter, x, y, z);
    space.addProcess((Process *)p, x, y, z);
    process_states[p->pid] = 0;
  }

  void injectEvent(int dst_x, int dst_y, int dst_z, int value) {
    ComputeEvent evt;
    evt.timestamp = current_time;
    evt.dst_node = dst_x * 1024 + dst_y * 32 + dst_z;
    evt.src_node = 0;
    evt.value = value;

    event_queue.push(evt);
  }

  void tick() {
    if (event_queue.empty())
      return;

    ComputeEvent evt = event_queue.top();
    event_queue.pop();

    current_time = evt.timestamp;
    events_processed++;

    // Decode spatial coordinates
    int dst_x = evt.dst_node / 1024;
    int dst_y = (evt.dst_node % 1024) / 32;
    int dst_z = evt.dst_node % 32;

    // REAL COMPUTATION: Accumulate value
    int pid = dst_x * 100 + dst_y * 10 + dst_z; // Simple pid mapping
    if (process_states.find(pid) != process_states.end()) {
      process_states[pid] += evt.value;
    }

    // Propagate to neighbors (with computation)
    int next_x = (dst_x + 1) % 32;
    if (next_x < 10) { // Only propagate within our 10-node ring
      ComputeEvent new_evt;
      new_evt.timestamp = current_time + 1; // Fixed delay for simplicity
      new_evt.dst_node = next_x * 1024;
      new_evt.src_node = evt.dst_node;
      new_evt.value = evt.value + 1; // Increment value (computation!)

      event_queue.push(new_evt);
    }
  }

  void run(int max_events) {
    while (events_processed < max_events && !event_queue.empty()) {
      tick();
    }
  }

  int getProcessState(int pid) const {
    auto it = process_states.find(pid);
    return (it != process_states.end()) ? it->second : 0;
  }

  unsigned long long getCurrentTime() const { return current_time; }
  unsigned long long getEventsProcessed() const { return events_processed; }
  size_t getProcessCount() const { return process_states.size(); }
};
