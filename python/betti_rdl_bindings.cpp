#include "../src/cpp_kernel/demos/BettiRDLCompute.h"
#include <pybind11/pybind11.h>


namespace py = pybind11;

// Python bindings for Betti-RDL

class PyBettiKernel {
private:
  BettiRDLCompute kernel;

public:
  PyBettiKernel() {}

  void spawn_process(int x, int y, int z) { kernel.spawnProcess(x, y, z); }

  void inject_event(int x, int y, int z, int value) {
    kernel.injectEvent(x, y, z, value);
  }

  void run(int max_events) { kernel.run(max_events); }

  unsigned long long get_events_processed() const {
    return kernel.getEventsProcessed();
  }

  unsigned long long get_current_time() const {
    return kernel.getCurrentTime();
  }

  size_t get_process_count() const { return kernel.getProcessCount(); }

  int get_process_state(int pid) const { return kernel.getProcessState(pid); }
};

PYBIND11_MODULE(betti_rdl, m) {
  m.doc() = "Betti-RDL: Space-Time Native Computation Runtime";

  py::class_<PyBettiKernel>(m, "Kernel")
      .def(py::init<>(),
           "Initialize Betti-RDL kernel with 32x32x32 toroidal space")
      .def("spawn_process", &PyBettiKernel::spawn_process,
           "Spawn a process at spatial coordinates (x, y, z)", py::arg("x"),
           py::arg("y"), py::arg("z"))
      .def("inject_event", &PyBettiKernel::inject_event,
           "Inject an event at coordinates with value", py::arg("x"),
           py::arg("y"), py::arg("z"), py::arg("value"))
      .def("run", &PyBettiKernel::run, "Run computation for up to max_events",
           py::arg("max_events"))
      .def_property_readonly("events_processed",
                             &PyBettiKernel::get_events_processed,
                             "Number of events processed")
      .def_property_readonly("current_time", &PyBettiKernel::get_current_time,
                             "Current logical time")
      .def_property_readonly("process_count", &PyBettiKernel::get_process_count,
                             "Number of active processes")
      .def("get_process_state", &PyBettiKernel::get_process_state,
           "Get accumulated state for a process", py::arg("pid"));

  m.attr("__version__") = "1.0.0";
}
