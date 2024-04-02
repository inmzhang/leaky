#include "leaky/core/channel.pybind.h"

#include "leaky/core/channel.h"
#include "pybind11/pybind11.h"

py::class_<leaky::LeakyPauliChannel> pybind_channel(py::module &m) {
    return py::class_<leaky::LeakyPauliChannel>(m, "LeakyPauliChannel", R"pbdoc(
        A generalized Pauli channel incorporating incoherent leakage transitions.
     )pbdoc");
}

void pybind_channel_methods(py::module &m, py::class_<leaky::LeakyPauliChannel> &c) {
    c.def(
        py::init<bool>(),
        py::arg("is_single_qubit_channel"),
        R"pbdoc(
            Initialize a `leaky.LeakyPauliChannel`.

            Args:
                is_single_qubit_channel: Whether the channel is single-qubit or two-qubit.
        )pbdoc");

    c.def_property_readonly(
        "is_single_qubit_channel",
        &leaky::LeakyPauliChannel::is_single_qubit_channel,
        R"pbdoc(
            Whether the channel is single-qubit or two-qubit.
        )pbdoc");
    c.def_property_readonly(
        "num_transitions",
        &leaky::LeakyPauliChannel::num_transitions,
        R"pbdoc(
            The number of transitions in the channel.
        )pbdoc");
    c.def(
        "add_transition",
        &leaky::LeakyPauliChannel::add_transition,
        py::arg("initial_status"),
        py::arg("final_status"),
        py::arg("pauli_channel_idx"),
        py::arg("probability"),
        R"pbdoc(
            Add a transition to the channel.

            Args:
                initial_status: The initial status of the qubit(s). If the channel is single-qubit,
                    this is a single status represented by a uint8. If the channel is two-qubit, 
                    this is a pair of status, which is a uint8 concatenated by two 4-bit status.
                final_status: The final status of the qubit(s). If the channel is single-qubit,
                    this is a single status represented by a uint8. If the channel is two-qubit, 
                    this is a pair of status, which is a uint8 concatenated by two 4-bit status.
                pauli_channel_idx: The index of the Pauli channel. For single qubit channels, this
                    is the index of the Pauli in the order [I, X, Y, Z]. For two-qubit channels, this
                    is the index of the Pauli in the order [II, IX, IY, IZ, XI, XX, XY, XZ, YI, YX, YY,
                    YZ, ZI, ZX, ZY, ZZ].
                probability: The probability of the transition.
        )pbdoc");
    c.def(
        "get_transitions_from_to",
        &leaky::LeakyPauliChannel::get_transitions_from_to,
        py::arg("initial_status"),
        py::arg("final_status"),
        R"pbdoc(
            Get the transitions from an initial status to a final status.

            Args:
                initial_status: The initial status of the qubit(s).
                final_status: The final status of the qubit(s).

            Returns:
                A pair of transition and probability.
        )pbdoc");
    c.def(
        "sample",
        &leaky::LeakyPauliChannel::sample,
        py::arg("initial_status"),
        R"pbdoc(
            Sample a transition from an initial status.

            Args:
                initial_status: The initial status of the qubit(s).

            Returns:
                A transition.
        )pbdoc");
    c.def(
        "safety_check",
        &leaky::LeakyPauliChannel::safety_check,
        R"pbdoc(
            Check if the channel is valid.
        )pbdoc");
    c.def(
        "__str__",
        &leaky::LeakyPauliChannel::str,
        R"pbdoc(
            Print the transitions in the channel.
        )pbdoc");
    c.def(
        "__repr__",
        &leaky::LeakyPauliChannel::repr,
        R"pbdoc(
            Print the channel representation.
        )pbdoc");
}