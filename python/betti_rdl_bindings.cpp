#include "../src/cpp_kernel/betti_rdl_c_api.h"

#include <pybind11/pybind11.h>

#include <cstdint>
#include <stdexcept>

namespace py = pybind11;

class PyBettiKernel {
private:
  BettiRDLCompute *kernel_ = nullptr;

public:
  PyBettiKernel() : kernel_(betti_rdl_create()) {
    if (!kernel_) {
      throw std::runtime_error("Failed to create Betti-RDL kernel");
    }
  }

  ~PyBettiKernel() {
    if (kernel_) {
      betti_rdl_destroy(kernel_);
      kernel_ = nullptr;
    }
  }

  PyBettiKernel(const PyBettiKernel &) = delete;
  PyBettiKernel &operator=(const PyBettiKernel &) = delete;

  void spawn_process(int x, int y, int z) {
    betti_rdl_spawn_process(kernel_, x, y, z);
  }

  void inject_event(int x, int y, int z, int value) {
    betti_rdl_inject_event(kernel_, x, y, z, value);
  }

  int run(int max_events) { return betti_rdl_run(kernel_, max_events); }

  std::uint64_t get_events_processed() const {
    return betti_rdl_get_events_processed(kernel_);
  }

  std::uint64_t get_current_time() const {
    return betti_rdl_get_current_time(kernel_);
  }

  std::size_t get_process_count() const {
    return betti_rdl_get_process_count(kernel_);
  }

  int get_process_state(int pid) const {
    return betti_rdl_get_process_state(kernel_, pid);
  }

  BettiRDLTelemetry get_telemetry() const {
    return betti_rdl_get_telemetry(kernel_);
  }

  double get_memory_mb() const {
    const auto telemetry = betti_rdl_get_telemetry(kernel_);
    return static_cast<double>(telemetry.memory_used) / (1024.0 * 1024.0);
  }
};

PYBIND11_MODULE(betti_rdl, m) {
  m.doc() = "Betti-RDL: Space-Time Native Computation Runtime";

  py::class_<BettiRDLTelemetry>(m, "Telemetry")
      .def_readonly("events_processed", &BettiRDLTelemetry::events_processed)
      .def_readonly("current_time", &BettiRDLTelemetry::current_time)
      .def_readonly("process_count", &BettiRDLTelemetry::process_count)
      .def_readonly("memory_used", &BettiRDLTelemetry::memory_used);

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
      .def("get_events_processed", &PyBettiKernel::get_events_processed,
           "Get lifetime number of events processed")
      .def("get_current_time", &PyBettiKernel::get_current_time,
           "Get current logical time")
      .def("get_process_count", &PyBettiKernel::get_process_count,
           "Get number of active processes")
      .def("get_telemetry", &PyBettiKernel::get_telemetry,
           "Get runtime telemetry")
      .def_property_readonly("events_processed",
                             &PyBettiKernel::get_events_processed,
                             "Number of events processed")
      .def_property_readonly("current_time", &PyBettiKernel::get_current_time,
                             "Current logical time")
      .def_property_readonly("process_count", &PyBettiKernel::get_process_count,
                             "Number of active processes")
      .def_property_readonly("memory_mb", &PyBettiKernel::get_memory_mb,
                             "Total memory used by runtime (MB)")
      .def("get_process_state", &PyBettiKernel::get_process_state,
           "Get accumulated state for a process", py::arg("pid"));

  m.attr("__version__") = "1.0.0";
}
