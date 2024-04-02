#include "leaky/core/channel.pybind.h"
#include "leaky/core/rand_gen.pybind.h"
#include "leaky/core/simulator.pybind.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;
using namespace py::literals;

int leaky_main(const std::vector<std::string> &args) {
    std::vector<const char *> argv;
    argv.push_back("leaky.main");
    for (const auto &arg : args) {
        argv.push_back(arg.data());
    }
    return 0;
    // return leaky::main(argv.size(), argv.data());
}

PYBIND11_MODULE(_cpp_leaky, m) {
    leaky_pybind::pybind_rand_gen_methods(m);
    auto channel = leaky_pybind::pybind_channel(m);
    leaky_pybind::pybind_channel_methods(m, channel);
    auto simulator = leaky_pybind::pybind_simulator(m);
    leaky_pybind::pybind_simulator_methods(m, simulator);
    m.def("main", &leaky_main, pybind11::kw_only(), pybind11::arg("command_line_args"), R"pbdoc(
Runs the command line tool version of leaky with the given arguments.
)pbdoc");
}