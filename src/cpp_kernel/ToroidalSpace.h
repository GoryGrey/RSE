
#pragma once
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <iostream>

// Forward delcaration
struct Process;

// Template-based Torus for compile-time optimization
template <int WIDTH, int HEIGHT, int DEPTH>
class ToroidalSpace {
public:
    // "Quantum Superposition": Multiple processes per voxel
    // Key: "x,y,z" string (Naive but functional for prototype)
    // In production, we would use a 1D array index: idx = x + WIDTH * (y + HEIGHT * z)
    std::map<std::string, std::vector<Process*>> grid;

    ToroidalSpace() {
        std::cout << "[Metal] ToroidalSpace <" << WIDTH << "x" << HEIGHT << "x" << DEPTH << "> Init." << std::endl;
    }

    // Wrap Coordinate (Topology Logic)
    int wrap(int v, int max) {
        return ((v % max) + max) % max;
    }

    std::string key(int x, int y, int z) {
        return std::to_string(wrap(x, WIDTH)) + "," + 
               std::to_string(wrap(y, HEIGHT)) + "," + 
               std::to_string(wrap(z, DEPTH));
    }

    void addProcess(Process* p, int x, int y, int z) {
        std::string k = key(x, y, z);
        grid[k].push_back(p);
    }

    void removeProcess(Process* p, int x, int y, int z) {
        std::string k = key(x, y, z);
        auto& cell = grid[k];
        
        // Remove-Erase Idiom
        cell.erase(std::remove(cell.begin(), cell.end(), p), cell.end());
        
        if (cell.empty()) {
            grid.erase(k);
        }
    }

    size_t getProcessCount() {
        size_t count = 0;
        for (auto const& [key, val] : grid) {
            count += val.size();
        }
        return count;
    }
};
