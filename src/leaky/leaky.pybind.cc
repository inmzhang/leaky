#include "leaky/core/rand_gen.pybind.h"
#include "leaky/core/readout_strategy.h"
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
    py::enum_<leaky::ReadoutStrategy>(m, "ReadoutStrategy")
        .value("RawLabel", leaky::ReadoutStrategy::RawLabel)
        .value("RandomLeakageProjection", leaky::ReadoutStrategy::RandomLeakageProjection)
        .value("DeterministicLeakageProjection", leaky::ReadoutStrategy::DeterministicLeakageProjection)
        .export_values();
    m.def("main", &leaky_main, pybind11::kw_only(), pybind11::arg("command_line_args"), R"pbdoc(
Runs the command line tool version of leaky with the given arguments.
)pbdoc");
}