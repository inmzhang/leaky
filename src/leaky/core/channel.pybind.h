#ifndef LEAKY_CHANNEL_PYBIND_H
#define LEAKY_CHANNEL_PYBIND_H

#include "leaky/core/channel.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace py::literals;

namespace leaky_pybind {

py::class_<leaky::LeakyPauliChannel> pybind_channel(py::module &m);
void pybind_channel_methods(py::module &m, py::class_<leaky::LeakyPauliChannel> &s);

}  // namespace leaky_pybind

#endif  // LEAKY_CHANNEL_PYBIND_H