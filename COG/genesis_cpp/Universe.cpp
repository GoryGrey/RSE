#include "Universe.h"
#include <algorithm>

Universe::Universe(int agent_count) {
  rng.seed(42);
  std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
  std::uniform_real_distribution<float> color(0.0f, 1.0f);

  // Create 1 Million Agents (or subset for demo)
  agents.resize(agent_count);
  for (auto &a : agents) {
    a.pos = {dist(rng), dist(rng), dist(rng)};
    a.vel = {0, 0, 0};
    a.energy = 100.0f;
    a.w_gravity = 1.0f; // Evolve this later
  }

  // Create Planets
  planets.push_back({{0, 0, 0}, 20.0f, 0.5f, 1.0f, 0.5f, 0.0f}); // The Sun
  planets.push_back(
      {{60, 0, 0}, 5.0f, 0.1f, 0.0f, 0.0f, 1.0f}); // Blue Gas Giant
  planets.push_back({{-40, 40, 0}, 8.0f, 0.2f, 1.0f, 0.0f, 0.0f}); // Red Dwarf
}

void Universe::tick(float dt) {
  for (auto &a : agents) {
    // 1. Gravity from Planets
    for (const auto &p : planets) {
      float dx = p.position.x - a.pos.x;
      float dy = p.position.y - a.pos.y;
      float dz = p.position.z - a.pos.z;
      float distSq = dx * dx + dy * dy + dz * dz;
      float dist = std::sqrt(distSq);

      // F = G * m1 * m2 / r^2
      if (dist > p.radius) {
        float force = (p.gravity * 10.0f) / distSq;
        a.vel.x += (dx / dist) * force * dt;
        a.vel.y += (dy / dist) * force * dt;
        a.vel.z += (dz / dist) * force * dt;
      } else {
        // Bounce / Surface friction
        a.vel.x *= -0.5f;
        a.vel.y *= -0.5f;
        a.vel.z *= -0.5f;
        // Push out
        float push = (p.radius - dist) + 0.1f;
        a.pos.x -= (dx / dist) * push;
        a.pos.y -= (dy / dist) * push;
        a.pos.z -= (dz / dist) * push;
      }
    }

    // 2. Integration
    a.pos.x += a.vel.x * dt;
    a.pos.y += a.vel.y * dt;
    a.pos.z += a.vel.z * dt;

    // 3. Drag
    a.vel.x *= 0.99f;
    a.vel.y *= 0.99f;
    a.vel.z *= 0.99f;
  }
}

std::vector<float> Universe::get_agent_positions() const {
  std::vector<float> data;
  data.reserve(agents.size() * 3);
  for (const auto &a : agents) {
    data.push_back(a.pos.x);
    data.push_back(a.pos.y);
    data.push_back(a.pos.z);
  }
  return data;
}

std::vector<float> Universe::get_planet_data() const {
  std::vector<float> data;
  for (const auto &p : planets) {
    data.push_back(p.position.x);
    data.push_back(p.position.y);
    data.push_back(p.position.z);
    data.push_back(p.radius);
    data.push_back(p.r);
    data.push_back(p.g);
    data.push_back(p.b);
  }
  return data;
}
