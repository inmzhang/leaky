#ifndef LEAKY_STATUS_PYBIND_H
#define LEAKY_STATUS_PYBIND_H

#include "leaky/core/status.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace py::literals;

namespace leaky_pybind {

py::class_<leaky::LeakageStatus> pybind_status(py::module &m);
void pybind_status_methods(py::module &m, py::class_<leaky::LeakageStatus> &s);

}  // namespace leaky_pybind

#endif  // LEAKY_STATUS_PYBIND_H
