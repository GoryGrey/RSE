#pragma once
#include "../Allocator.h"
#include "../ToroidalSpace.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <unordered_map>

// Enhanced Betti-RDL with Real Computation
// Adds actual algorithm execution, not just event propagation

struct ComputeEvent {
  unsigned long long timestamp;
  int dst_node;
  int src_node;
  int value; // Actual data payload
  bool recursive;

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

  std::unordered_map<int, int> node_to_pid;    // node_id -> pid
  std::unordered_map<int, int> process_states; // pid -> accumulated value

  unsigned long long current_time = 0;
  unsigned long long events_processed = 0;
  int process_counter = 0;

  static int encodeNode(int x, int y, int z) { return x * 1024 + y * 32 + z; }

public:
  BettiRDLCompute() {
    std::cout << "[COMPUTE] Initializing Betti-RDL with real computation..."
              << std::endl;
  }

  void spawnProcess(int x, int y, int z) {
    ComputeProcess *p = new ComputeProcess(++process_counter, x, y, z);
    space.addProcess((Process *)p, x, y, z);

    process_states[p->pid] = 0;
    node_to_pid[encodeNode(x, y, z)] = p->pid;
  }

  void injectEvent(int dst_x, int dst_y, int dst_z, int value) {
    ComputeEvent evt;
    evt.timestamp = current_time;
    evt.dst_node = encodeNode(dst_x, dst_y, dst_z);
    evt.src_node = 0;
    evt.value = value;
    evt.recursive = false;

    event_queue.push(evt);
  }

  void injectRecursiveEvent(int dst_x, int dst_y, int dst_z, int initial_value) {
    ComputeEvent evt;
    evt.timestamp = current_time;
    evt.dst_node = encodeNode(dst_x, dst_y, dst_z);
    evt.src_node = 0;
    evt.value = initial_value;
    evt.recursive = true;

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

    // REAL COMPUTATION: accumulate payload into the destination process state
    auto pid_it = node_to_pid.find(evt.dst_node);
    if (pid_it != node_to_pid.end()) {
      process_states[pid_it->second] += evt.value;
    }

    if (!evt.recursive) {
      return;
    }

    // Recursion-as-replacement: emit exactly one follow-up event.
    // This keeps the queue size constant for a single recursive chain.
    ComputeEvent new_evt;
    new_evt.timestamp = current_time + 1;
    new_evt.dst_node = encodeNode(dst_x, dst_y, dst_z);
    new_evt.src_node = evt.dst_node;
    new_evt.value = evt.value + 1;
    new_evt.recursive = true;

    event_queue.push(new_evt);
  }

  void run(int max_events) {
    if (max_events <= 0)
      return;

    unsigned long long target_events =
        events_processed + static_cast<unsigned long long>(max_events);

    while (events_processed < target_events && !event_queue.empty()) {
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
