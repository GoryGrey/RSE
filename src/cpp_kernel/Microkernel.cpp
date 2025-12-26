
#include <iostream>
#include <thread>
#include <chrono>
#include <new>
#include "Allocator.h"
#include "ToroidalSpace.h"

// Process Control Block (PCB)
struct Process {
    int pid;
    int priority;
    int x, y, z;
    
    Process(int id) : pid(id), priority(1), x(0), y(0), z(0) {}
};

class BettiKernel {
    ToroidalSpace<32, 32, 32> space;
    int pid_counter = 0;
    int tick_count = 0;

public:
    BettiKernel() {
        std::cout << "[Metal] Kernel Booting..." << std::endl;
    }

    void spawnProcess() {
        void* mem = MemoryManager::getAllocator().allocateProcess(sizeof(Process));
        if (!mem) {
            panic("Process pool exhausted");
        }

        Process* p = new (mem) Process(++pid_counter);

        // Deterministic placement (avoids per-voxel overflow and makes iteration deterministic)
        size_t idx = static_cast<size_t>(p->pid - 1) % (32 * 32 * 32);
        p->x = static_cast<int>(idx % 32);
        p->y = static_cast<int>((idx / 32) % 32);
        p->z = static_cast<int>(idx / (32 * 32));

        if (!space.addProcess(p, p->x, p->y, p->z)) {
            panic("ToroidalSpace voxel capacity exceeded");
        }
    }

    void tick() {
        tick_count++;
        // Scheduler Logic: Iterate all processes
        // In C++, we would iterate the map. 
        // For benchmarks, we just count them to simulate load.
        size_t procs = space.getProcessCount();
        (void)procs;
        
        // "Context Switch" Simulation
        // In a real scheduler, we would swap register states here.
    }

    void panic(const char* msg) {
        std::cerr << "[KERNEL PANIC] " << msg << std::endl;
        exit(1);
    }
    
    void runBenchmark(int duration_ms) {
        std::cout << "[Metal] Running Scheduler Benchmark..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        // Spawn 100,000 processes (Fork Bomb Test)
        for(int i=0; i<100000; i++) {
            spawnProcess();
        }
        
        size_t initial_mem = MemoryManager::getUsedMemory();
        std::cout << "[Metal] Spawned 100k Processes. Memory: " << initial_mem << " bytes" << std::endl;

        // Run Loop
        long total_ticks = 0;
        while(true) {
            tick();
            total_ticks++;
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > duration_ms) break;
        }
        
        std::cout << "[Metal] Benchmark Complete." << std::endl;
        std::cout << "    > Total Ticks: " << total_ticks << std::endl;
        std::cout << "    > Active PIDs: " << space.getProcessCount() << std::endl;
    }
};

int main() {
    BettiKernel kernel;
    kernel.runBenchmark(5000); // Run for 5 seconds
    return 0;
}
