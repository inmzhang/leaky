#include "leaky/core/channel.pybind.h"
#include "leaky/core/rand_gen.pybind.h"
#include "leaky/core/simulator.pybind.h"
#include "leaky/core/status.pybind.h"

namespace py = pybind11;
using namespace py::literals;

PYBIND11_MODULE(_cpp_leaky, m) {
    leaky_pybind::pybind_rand_gen_methods(m);
    auto status = leaky_pybind::pybind_status(m);
    leaky_pybind::pybind_status_methods(m, status);
    auto transition = leaky_pybind::pybind_transition(m);
    leaky_pybind::pybind_transition_methods(m, transition);
    auto channel = leaky_pybind::pybind_channel(m);
    leaky_pybind::pybind_channel_methods(m, channel);
    auto simulator = leaky_pybind::pybind_simulator(m);
    leaky_pybind::pybind_simulator_methods(m, simulator);
}
