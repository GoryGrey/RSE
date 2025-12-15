#pragma once
#include <vector>
#include <cmath>
#include <random>

struct Vec3 {
    float x, y, z;
};

struct Planet {
    Vec3 position;
    float radius;
    float gravity;
    float r, g, b;
};

struct Agent {
    Vec3 pos;
    Vec3 vel;
    float energy;
    // Brain weights
    float w_gravity;
    float w_cohesion;
    float w_alignment;
};

class Universe {
public:
    Universe(int agent_count);
    void tick(float dt);
    
    // Data Access for JS
    std::vector<float> get_agent_positions() const;
    std::vector<float> get_planet_data() const;
    int get_agent_count() const { return agents.size(); }
    int get_planet_count() const { return planets.size(); }

private:
    std::vector<Agent> agents;
    std::vector<Planet> planets;
    std::mt19937 rng;
};
