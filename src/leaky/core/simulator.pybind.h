#ifndef LEAKY_SIMULATOR_PYBIND_H
#define LEAKY_SIMULATOR_PYBIND_H

#include <pybind11/numpy.h>

#include "leaky/core/simulator.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace py::literals;

namespace leaky_pybind {

py::class_<leaky::Simulator> pybind_simulator(py::module &m);
void pybind_simulator_methods(py::module &m, py::class_<leaky::Simulator> &s);

}  // namespace leaky_pybind

#endif  // LEAKY_SIMULATOR_PYBIND_H