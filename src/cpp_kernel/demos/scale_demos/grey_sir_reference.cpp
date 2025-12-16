#include "../BettiRDLCompute.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

struct XorShift64 {
  std::uint64_t state;

  explicit XorShift64(std::uint64_t seed) : state(seed ? seed : 1u) {}

  std::uint64_t next() {
    std::uint64_t x = state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state = x;
    return x;
  }
};

int wrap32(int v) {
  int m = v % 32;
  return m < 0 ? m + 32 : m;
}

int nodeId(int x, int y, int z) {
  const int wx = wrap32(x);
  const int wy = wrap32(y);
  const int wz = wrap32(z);
  return wx * 1024 + wy * 32 + wz;
}

} // namespace

int main(int argc, char **argv) {
  std::uint64_t seed = 42;
  int max_events = 1000;
  int runtime_processes = 64;
  int spacing = 1;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    auto read_u64 = [&](std::uint64_t &out) {
      if (i + 1 < argc) {
        out = static_cast<std::uint64_t>(std::strtoull(argv[++i], nullptr, 10));
      }
    };

    auto read_i32 = [&](int &out) {
      if (i + 1 < argc) {
        out = std::atoi(argv[++i]);
      }
    };

    if (arg == "--seed") {
      read_u64(seed);
    } else if (arg == "--max-events") {
      read_i32(max_events);
    } else if (arg == "--processes") {
      read_i32(runtime_processes);
    } else if (arg == "--spacing") {
      read_i32(spacing);
    }
  }

  if (runtime_processes < 1) {
    runtime_processes = 1;
  }

  BettiRDLCompute kernel;

  const int grid_size = static_cast<int>(std::ceil(std::sqrt(runtime_processes)));
  std::vector<std::array<int, 3>> coords;
  coords.reserve(static_cast<std::size_t>(runtime_processes));

  for (int i = 0; i < runtime_processes; i++) {
    int x = (i % grid_size) * spacing;
    int y = (i / grid_size) * spacing;
    int z = 0;
    coords.push_back({x, y, z});
    kernel.spawnProcess(x, y, z);
  }

  XorShift64 rng(seed);
  const int injections = std::min(4, runtime_processes);

  for (int i = 0; i < injections; i++) {
    const std::size_t idx = static_cast<std::size_t>(rng.next() % static_cast<std::uint64_t>(runtime_processes));
    const int value = static_cast<int>(rng.next() % 5u) + 1;
    kernel.injectEvent(coords[idx][0], coords[idx][1], coords[idx][2], value);
  }

  (void)kernel.run(max_events);

  std::map<int, int> process_states;
  for (const auto &coord : coords) {
    const int pid = nodeId(coord[0], coord[1], coord[2]);
    process_states[pid] = kernel.getProcessState(pid);
  }

  std::cout << "{";
  std::cout << "\"seed_used\":" << seed << ",";
  std::cout << "\"max_events\":" << max_events << ",";
  std::cout << "\"runtime_processes\":" << runtime_processes << ",";
  std::cout << "\"spacing\":" << spacing << ",";
  std::cout << "\"events_processed\":" << kernel.getEventsProcessed() << ",";
  std::cout << "\"current_time\":" << kernel.getCurrentTime() << ",";
  std::cout << "\"process_states\":{";

  bool first = true;
  for (const auto &entry : process_states) {
    if (!first) {
      std::cout << ",";
    }
    first = false;
    std::cout << "\"" << entry.first << "\":" << entry.second;
  }

  std::cout << "}}" << std::endl;

  return 0;
}
