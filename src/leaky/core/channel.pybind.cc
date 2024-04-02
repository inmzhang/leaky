#include "leaky/core/channel.pybind.h"

#include "leaky/core/channel.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "stim.h"

py::class_<leaky::LeakyPauliChannel> leaky_pybind::pybind_channel(py::module &m) {
    return {
        m,
        "LeakyPauliChannel",
        stim::clean_doc_string(R"DOC(
            A generalized Pauli channel incorporating incoherent leakage transitions.
        )DOC")
            .data()};
}

void leaky_pybind::pybind_channel_methods(py::module &m, py::class_<leaky::LeakyPauliChannel> &c) {
    c.def(
        py::init<bool>(),
        py::arg("is_single_qubit_channel"),
        stim::clean_doc_string(R"DOC(
            Initialize a `leaky.LeakyPauliChannel`.

            Args:
                is_single_qubit_channel: Whether the channel is single-qubit or two-qubit.
        )DOC")
            .data());
    c.def_property_readonly(
        "num_transitions",
        &leaky::LeakyPauliChannel::num_transitions,
        stim::clean_doc_string(R"DOC(
            The number of transitions in the channel.
        )DOC")
            .data());
    c.def(
        "add_transition",
        &leaky::LeakyPauliChannel::add_transition,
        py::arg("initial_status"),
        py::arg("final_status"),
        py::arg("pauli_channel_idx"),
        py::arg("probability"),
        stim::clean_doc_string(
            R"DOC(
            Add a transition to the channel.

            Args:
                initial_status: The initial status of the qubit(s). If the channel is 
                single-qubit,
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
        )DOC",
            true)
            .data());
    c.def(
        "get_transitions_from_to",
        &leaky::LeakyPauliChannel::get_transitions_from_to,
        py::arg("initial_status"),
        py::arg("final_status"),
        stim::clean_doc_string(R"DOC(
            Get the transitions from an initial status to a final status.

            Args:
                initial_status: The initial status of the qubit(s).
                final_status: The final status of the qubit(s).

            Returns:
                A pair of transition and probability.
        )DOC")
            .data());
    c.def(
        "sample",
        &leaky::LeakyPauliChannel::sample,
        py::arg("initial_status"),
        stim::clean_doc_string(R"DOC(
            Sample a transition from an initial status.

            Args:
                initial_status: The initial status of the qubit(s).

            Returns:
                A transition.
        )DOC")
            .data());
    c.def(
        "safety_check",
        &leaky::LeakyPauliChannel::safety_check,
        stim::clean_doc_string(R"DOC(
            Check if the channel is valid.
        )DOC")
            .data());
    c.def(
        "__str__",
        &leaky::LeakyPauliChannel::str,
        stim::clean_doc_string(R"DOC(
            Print the transitions in the channel.
        )DOC")
            .data());
    c.def(
        "__repr__",
        &leaky::LeakyPauliChannel::repr,
        stim::clean_doc_string(R"DOC(
            Print the channel representation.
        )DOC")
            .data());
}