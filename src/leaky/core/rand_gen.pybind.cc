#include "leaky/core/rand_gen.pybind.h"

#include "leaky/core/rand_gen.h"

void leaky_pybind::pybind_rand_gen_methods(py::module &m) {
    m.def("randomize", &leaky::randomize);
    m.def("set_seed", &leaky::set_seed, "seed"_a);

    m.def("rand_float", &leaky::rand_float, "begin"_a, "end"_a);
}