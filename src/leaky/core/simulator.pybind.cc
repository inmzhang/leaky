#include "leaky/core/simulator.pybind.h"

#include <iostream>
#include <iterator>
#include <vector>

py::class_<leaky::Simulator> leaky_pybind::pybind_simulator(py::module &m) {
    return {
        m,
        "Simulator",
        stim::clean_doc_string(R"DOC(
            A simulator for quantum circuits with incoherent leakage transitions.
        )DOC")
            .data(),
    };
}

void leaky_pybind::pybind_simulator_methods(py::module &m, py::class_<leaky::Simulator> &s) {
    py::enum_<leaky::ReadoutStrategy>(m, "ReadoutStrategy", py::arithmetic())
        .value(
            "RawLabel",
            leaky::ReadoutStrategy::RawLabel,
            "Return the raw measurement record, including the leakage states.")
        .value(
            "RandomLeakageProjection",
            leaky::ReadoutStrategy::RandomLeakageProjection,
            stim::clean_doc_string(R"DOC("Randomly project the leakage to the ground state(50% chance for 0/1).))DOC")
                .data())
        .value(
            "DeterministicLeakageProjection",
            leaky::ReadoutStrategy::DeterministicLeakageProjection,
            "Deterministicly project the leakage state to state 1.")
        .export_values();

    s.def(
        py::init<uint32_t>(),
        py::arg("num_qubits"),
        stim::clean_doc_string(R"DOC(
            Initialize a simulator with the given number of qubits.

            Args:
                num_qubits: The number of qubits in the simulator.
        ))DOC")
            .data());
    s.def(
        "do_circuit",
        &leaky::Simulator::do_circuit,
        py::arg("circuit"),
        stim::clean_doc_string(R"DOC(
            Apply a circuit to the simulator.

            Args:
                circuit: The `stim.Circuit` to apply.
        ))DOC")
            .data());
    s.def(
        "do",
        [](leaky::Simulator &self, const py::object &obj) {
            if (py::isinstance<stim::Circuit>(obj)) {
                std::cout << py::cast<stim::Circuit>(obj).str() << std::endl;
                self.do_circuit(py::cast<stim::Circuit>(obj));
            } else {
                const stim::CircuitInstruction &circuit_instruction = py::cast<stim::CircuitInstruction>(obj);
                self.do_gate(circuit_instruction);
            }
        },
        py::arg("circuit_or_instruction"),
        stim::clean_doc_string(R"DOC(
            Apply a circuit or instruction to the simulator.

            Args:
                circuit_or_instruction: The `stim.Circuit` or `stim.CircuitInstruction`
                    to apply.
        ))DOC")
            .data());
    s.def(
        "do_1q_leaky_pauli_channel",
        &leaky::Simulator::do_1q_leaky_pauli_channel,
        py::arg("ideal_inst"),
        py::arg("channel"),
        stim::clean_doc_string(R"DOC(
            Apply a single qubit leaky Pauli channel to a circuit instruction.

            Args:
                ideal_inst: The ideal circuit instruction to apply the channel to.
                channel: The leaky channel to apply.
            ))DOC")
            .data());
    s.def(
        "do_2q_leaky_pauli_channel",
        &leaky::Simulator::do_2q_leaky_pauli_channel,
        py::arg("ideal_inst"),
        py::arg("channel"),
        stim::clean_doc_string(R"DOC(
            Apply a two qubits leaky Pauli channel to a circuit instruction.

            Args:
                ideal_inst: The ideal circuit instruction to apply the channel to.
                channel: The leaky channel to apply.
        ))DOC")
            .data());
    s.def(
        "bind_leaky_channel",
        &leaky::Simulator::bind_leaky_channel,
        py::arg("ideal_inst"),
        py::arg("channel"),
        stim::clean_doc_string(R"DOC(
            Bind a leaky channel to a circuit instruction.

            A bound channel will be applied to the simulator whenever the bound 
            instruction is applied.

            Args:
                ideal_inst: The ideal circuit instruction to bind the channel to.
                channel: The leaky channel to bind.
        ))DOC")
            .data());
    s.def(
        "clear",
        &leaky::Simulator::clear,
        py::arg("clear_bound_channels") = false,
        stim::clean_doc_string(R"DOC(
            Clear the simulator's state.

            Args:
                clear_bound_channels: Whether to also clear the bound leaky channels.
        ))DOC")
            .data());
    s.def(
        "current_measurement_record",
        [](leaky::Simulator &self, leaky::ReadoutStrategy readout_strategy) {
            auto record_ = self.current_measurement_record(readout_strategy);
            std::vector<uint8_t> *record_vec = new std::vector<uint8_t>();
            std::move(record_.begin(), record_.end(), std::back_inserter(*record_vec));
            auto record_capsule = py::capsule(record_vec, [](void *record) {
                delete reinterpret_cast<std::vector<uint8_t> *>(record);
            });
            py::array_t<uint8_t> record_arr =
                py::array_t<uint8_t>(record_vec->size(), record_vec->data(), record_capsule);
            return record_arr;
        },
        py::arg("readout_strategy") = leaky::ReadoutStrategy::RawLabel,
        stim::clean_doc_string(R"DOC(
            Get the current measurement record.

            Args:
                readout_strategy: The readout strategy to use.

            Returns:
                A numpy array of measurement results with `dtype=uint8`.
        ))DOC")
            .data());
}