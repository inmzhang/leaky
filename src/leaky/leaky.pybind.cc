#include "leaky/core/channel.pybind.h"
#include "leaky/core/instruction.pybind.h"
#include "leaky/core/rand_gen.pybind.h"
#include "leaky/core/simulator.pybind.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace py::literals;

PYBIND11_MODULE(_cpp_leaky, m) {
    leaky_pybind::pybind_rand_gen_methods(m);
    auto channel = leaky_pybind::pybind_channel(m);
    leaky_pybind::pybind_channel_methods(m, channel);
    auto instruction = leaky_pybind::pybind_instruction(m);
    leaky_pybind::pybind_instruction_methods(m, instruction);
    auto simulator = leaky_pybind::pybind_simulator(m);
    leaky_pybind::pybind_simulator_methods(m, simulator);
}