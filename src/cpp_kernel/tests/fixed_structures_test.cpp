#include "../Allocator.h"
#include "../FixedStructures.h"
#include "../ToroidalSpace.h"
#include "../demos/BettiRDLKernel.h"

#include <cassert>
#include <cstdint>
#include <iostream>

struct Process {
  int id;
};

static void testToroidalSpaceVoxelCapacity() {
  ToroidalSpace<32, 32, 32, 4> space;

  Process p1{1};
  Process p2{2};
  Process p3{3};
  Process p4{4};
  Process p5{5};

  assert(space.addProcess(&p1, 0, 0, 0));
  assert(space.addProcess(&p2, 0, 0, 0));
  assert(space.addProcess(&p3, 0, 0, 0));
  assert(space.addProcess(&p4, 0, 0, 0));

  // Capacity exceeded
  assert(!space.addProcess(&p5, 0, 0, 0));
  assert(space.getProcessCount() == 4);

  // Removal frees capacity deterministically
  assert(space.removeProcess(&p2, 0, 0, 0));
  assert(space.getProcessCount() == 3);
  assert(space.addProcess(&p5, 0, 0, 0));
  assert(space.getProcessCount() == 4);

  // Wrap invariants (32 wraps to 0)
  Process p6{6};
  assert(!space.addProcess(&p6, 32, 0, 0));
}

struct HeapEvent {
  int t;
  int id;

  bool operator<(const HeapEvent &other) const {
    if (t != other.t)
      return t < other.t;
    return id < other.id;
  }
};

static void testFixedMinHeapCapacityAndOrder() {
  FixedMinHeap<HeapEvent, 8> heap;

  assert(heap.push({5, 1}));
  assert(heap.push({1, 1}));
  assert(heap.push({3, 1}));
  assert(heap.push({1, 0}));
  assert(heap.push({4, 1}));
  assert(heap.push({2, 1}));
  assert(heap.push({6, 1}));
  assert(heap.push({7, 1}));

  assert(!heap.push({8, 1}));
  assert(heap.size() == 8);

  // Canonical ordering
  assert(heap.top().t == 1 && heap.top().id == 0);
  heap.pop();
  assert(heap.top().t == 1 && heap.top().id == 1);
  heap.pop();
  assert(heap.top().t == 2);
}

static void testFixedAdjacencyCapacity() {
  BettiRDLKernel kernel;

  // Fill edge capacity with unique (from,to) pairs.
  // Each edge uses from/to coordinates derived from a unique node id.
  for (std::size_t i = 0; i < 8192; ++i) {
    std::uint32_t from = static_cast<std::uint32_t>(i % LATTICE_SIZE);
    std::uint32_t to = static_cast<std::uint32_t>((i + 1) % LATTICE_SIZE);

    int fx = static_cast<int>(from / 1024u);
    int fy = static_cast<int>((from % 1024u) / 32u);
    int fz = static_cast<int>(from % 32u);

    int tx = static_cast<int>(to / 1024u);
    int ty = static_cast<int>((to % 1024u) / 32u);
    int tz = static_cast<int>(to % 32u);

    assert(kernel.createEdge(fx, fy, fz, tx, ty, tz, 1));
  }

  // One more should fail deterministically
  assert(!kernel.createEdge(0, 0, 0, 1, 0, 0, 1));
}

int main() {
  std::cout << "[FixedStructures Tests]" << std::endl;

  testToroidalSpaceVoxelCapacity();
  testFixedMinHeapCapacityAndOrder();
  testFixedAdjacencyCapacity();

  std::cout << "  ✓ all tests passed" << std::endl;
  return 0;
}
