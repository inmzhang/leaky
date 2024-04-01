#ifndef LEAKY_RAND_GEN_PYBIND_H
#define LEAKY_RAND_GEN_PYBIND_H

#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace py::literals;

namespace leaky_pybind {

void pybind_rand_gen_methods(py::module &m);

}  // namespace leaky_pybind

#endif  // LEAKY_RAND_GEN_PYBIND_H