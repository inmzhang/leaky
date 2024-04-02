#include "leaky/core/simulator.pybind.h"

py::class_<leaky::Simulator> leaky_pybind::pybind_simulator(py::module &m) {
}

void leaky_pybind::pybind_simulator_methods(py::module &m, py::class_<leaky::Simulator> &s) {
    py::enum_<leaky::ReadoutStrategy>(m, "ReadoutStrategy", py::arithmetic())
        .value(
            "RawLabel",
            leaky::ReadoutStrategy::RawLabel,
            "Return the raw measurement record, including the leakage states.")
        .value(
            "RandomLeakageProjection",
            leaky::ReadoutStrategy::RandomLeakageProjection,
            R"pbdoc("Randomly project the leakage to the ground state(50% chance for 0/1).)pbdoc")
        .value(
            "DeterministicLeakageProjection",
            leaky::ReadoutStrategy::DeterministicLeakageProjection,
            "Deterministicly project the leakage state to state 1.")
        .export_values();
}