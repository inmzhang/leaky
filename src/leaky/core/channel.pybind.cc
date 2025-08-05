#include "leaky/core/channel.pybind.h"

#include <pybind11/cast.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include "leaky/core/channel.h"
#include "pybind11/pybind11.h"

py::class_<leaky::Transition> leaky_pybind::pybind_transition(py::module &m) {
    return {m, "Transition"};
}

void leaky_pybind::pybind_transition_methods(py::module &m, py::class_<leaky::Transition> &c) {
    c.def_readonly("to_status", &leaky::Transition::to_status);
    c.def_readonly("pauli_operator", &leaky::Transition::pauli_operator);
}

py::class_<leaky::LeakyPauliChannel> leaky_pybind::pybind_channel(py::module &m) {
    return {m, "LeakyPauliChannel"};
}

void leaky_pybind::pybind_channel_methods(py::module &m, py::class_<leaky::LeakyPauliChannel> &c) {
    c.def(py::init<size_t>(), py::arg("num_qubits"));
    c.def_property_readonly("num_transitions", &leaky::LeakyPauliChannel::num_transitions);
    c.def(
        "add_transition",
        &leaky::LeakyPauliChannel::add_transition,
        py::arg("from_status"),
        py::arg("to_status"),
        py::arg("pauli_operator"),
        py::arg("probability"));
    c.def(
        "get_prob_from_to",
        &leaky::LeakyPauliChannel::get_prob_from_to,
        py::arg("from_status"),
        py::arg("to_status"),
        py::arg("pauli_operator"));
    c.def("sample", &leaky::LeakyPauliChannel::sample, py::arg("initial_status"));
    c.def("safety_check", &leaky::LeakyPauliChannel::safety_check);
    c.def("__str__", &leaky::LeakyPauliChannel::str);
}
