#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>

// RSE Kernel - C++ Port
// Proves architecture works without Garbage Collection

struct SymbolicAgent {
    std::string id;
    char symbol;
    int x, y, z;
    int age;
};

class ToroidalSpace {
public:
    int width, height, depth;
    std::vector<SymbolicAgent> agents;

    ToroidalSpace(int w, int h, int d) : width(w), height(h), depth(d) {
        agents.reserve(1000); // Pre-allocate to avoid resize noise
    }

    void addAgent(const SymbolicAgent& agent) {
        agents.push_back(agent);
    }

    void moveAgent(SymbolicAgent& agent, int dx, int dy, int dz) {
        agent.x = (agent.x + dx + width) % width;
        agent.y = (agent.y + dy + height) % height;
        agent.z = (agent.z + dz + depth) % depth;
    }
};

class RSEKernel {
public:
    ToroidalSpace* space;
    long long cycle;

    RSEKernel() {
        space = new ToroidalSpace(32, 32, 32);
        cycle = 0;
    }

    ~RSEKernel() {
        delete space;
    }

    void init(int count) {
        for(int i=0; i<count; i++) {
            SymbolicAgent a;
            a.id = std::to_string(rand());
            a.symbol = 'A' + (rand() % 26);
            a.x = rand() % space->width;
            a.y = rand() % space->height;
            a.z = rand() % space->depth;
            a.age = 0;
            space->addAgent(a);
        }
    }

    void step() {
        cycle++;
        for(auto& agent : space->agents) {
            agent.age++;
            if((rand() % 100) > 80) {
                space->moveAgent(agent, (rand()%3)-1, (rand()%3)-1, (rand()%3)-1);
            }
        }
    }
};

// Simple visual test
int main() {
    srand(time(0));
    std::cout << "=================================================\n";
    std::cout << "   RSE KERNEL // C++ // START                    \n";
    std::cout << "=================================================\n";
    
    RSEKernel kernel;
    kernel.init(50);
    
    // Manual loop - stack frame never grows
    // This is the definition of O(1) recursion via hydration
    long long STEPS = 1000000;
    std::cout << "[RUN] Executing " << STEPS << " steps (Manual C++ Loop)...\n";
    
    clock_t start = clock();
    
    for(long long i=0; i<STEPS; i++) {
        kernel.step();
        if(i % 100000 == 0) {
            std::cout << "    > Cycle: " << i << "\r"; // \r for overwrite
        }
    }
    
    clock_t end = clock();
    double duration = double(end - start) / CLOCKS_PER_SEC;
    
    std::cout << "\n[DONE] Finished " << STEPS << " steps in " << duration << "s.\n";
    std::cout << "       Speed: " << (STEPS/duration) << " ops/sec\n";
    
    return 0;
}
