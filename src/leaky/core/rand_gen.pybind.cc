#include "leaky/core/rand_gen.pybind.h"

#include "leaky/core/rand_gen.h"
#include "stim.h"

void leaky_pybind::pybind_rand_gen_methods(py::module &m) {
    m.def(
        "randomize",
        &leaky::randomize,
        stim::clean_doc_string(R"DOC(
            Choose a random seed using std::random_device

            Examples:
                >>> import leaky
                >>> leaky.randomize()
        ))DOC")
            .data());
    m.def(
        "set_seed",
        &leaky::set_seed,
        "seed"_a,
        stim::clean_doc_string(R"DOC(
            Sets the seed of the random number generator

            Args:
                seed: int
                    The seed for the random number generator (must be non-negative)

            Examples:
                >>> import leaky
                >>> leaky.set_seed(10)

        ))DOC")
            .data());

    m.def(
        "rand_float",
        &leaky::rand_float,
        "begin"_a,
        "end"_a,
        stim::clean_doc_string(R"DOC(
            Generate a floating point number chosen uniformly at random
            over the interval between `from` and `to`

            Args:
                begin:
                    Smallest float that can be drawn from the distribution
                end:
                    Largest float that can be drawn from the distribution

            Returns:
                The random float
        ))DOC")
            .data());
}