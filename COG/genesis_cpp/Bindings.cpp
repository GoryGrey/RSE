#include "Universe.h"
#include <emscripten/bind.h>


using namespace emscripten;

// Vector binding
EMSCRIPTEN_BINDINGS(my_module) {
  register_vector<float>("VectorFloat");

  class_<Universe>("Universe")
      .constructor<int>()
      .function("tick", &Universe::tick)
      .function("get_agent_positions", &Universe::get_agent_positions)
      .function("get_planet_data", &Universe::get_planet_data)
      .function("get_agent_count", &Universe::get_agent_count)
      .function("get_planet_count", &Universe::get_planet_count);
}
