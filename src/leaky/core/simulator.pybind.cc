#include "leaky/core/simulator.pybind.h"

#include <cstddef>
#include <iterator>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <vector>

#include "leaky/core/instruction.pybind.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/simulator.h"
#include "stim.h"

struct ChannelResolvedCircuit {
    std::vector<stim::CircuitInstruction> instructions;
    std::vector<std::pair<stim::SpanRef<const stim::GateTarget>, const leaky::LeakyPauliChannel &>> channels;
    std::vector<std::pair<bool, size_t>> order;
};

ChannelResolvedCircuit resolve_bound_channels_in_circuit(
    const leaky::Simulator &simulator, const stim::Circuit &flatten_circuit) {
    ChannelResolvedCircuit resolved_circuit;
    resolved_circuit.instructions = flatten_circuit.operations;
    size_t instruction_idx = 0;
    size_t channel_idx = 0;
    for (const auto &op : flatten_circuit.operations) {
        resolved_circuit.order.push_back({true, instruction_idx++});
        auto gate_type = op.gate_type;
        auto targets = op.targets;
        auto flags = stim::GATE_DATA[gate_type].flags;
        bool is_single_qubit_gate = flags & stim::GATE_IS_SINGLE_QUBIT_GATE;
        size_t step = is_single_qubit_gate ? 1 : 2;
        for (size_t i = 0; i < targets.size(); i += step) {
            auto split_targets = targets.sub(i, i + step);
            stim::CircuitInstruction split_inst = {gate_type, op.args, split_targets};
            const auto inst_id = std::hash<std::string>{}(split_inst.str());
            auto it = simulator.bound_leaky_channels.find(inst_id);
            if (it == simulator.bound_leaky_channels.end()) {
                continue;
            }
            resolved_circuit.channels.push_back({split_targets, it->second});
            resolved_circuit.order.push_back({false, channel_idx++});
        }
    }
    return resolved_circuit;
}

py::class_<leaky::Simulator> leaky_pybind::pybind_simulator(py::module &m) {
    return {m, "Simulator"};
}

leaky::Simulator create_simulator(uint32_t num_qubits, const pybind11::object &seed) {
    if (!seed.is_none()) {
        leaky::set_seed(seed.cast<unsigned>());
    } else {
        leaky::randomize();
    }
    return leaky::Simulator(num_qubits);
}

void leaky_pybind::pybind_simulator_methods(py::module &m, py::class_<leaky::Simulator> &s) {
    py::enum_<leaky::ReadoutStrategy>(m, "ReadoutStrategy", py::arithmetic())
        .value("RawLabel", leaky::ReadoutStrategy::RawLabel)
        .value("RandomLeakageProjection", leaky::ReadoutStrategy::RandomLeakageProjection)
        .value("DeterministicLeakageProjection", leaky::ReadoutStrategy::DeterministicLeakageProjection)
        .export_values();

    s.def(py::init(&create_simulator), py::arg("num_qubits"), pybind11::kw_only(), py::arg("seed") = pybind11::none());
    s.def(
        "do_circuit",
        [](leaky::Simulator &self, const py::object &circuit) {
            auto circuit_str = pybind11::cast<std::string>(pybind11::str(circuit));
            stim::Circuit converted_circuit = stim::Circuit(circuit_str.c_str());
            self.do_circuit(converted_circuit);
        },
        py::arg("circuit"));
    s.def(
        "do",
        [](leaky::Simulator &self, const leaky_pybind::LeakyInstruction &instruction, bool look_up_bound_channels) {
            self.do_gate(instruction, look_up_bound_channels);
        },
        py::arg("instruction"),
        py::arg("look_up_bound_channels") = true);
    s.def(
        "apply_1q_leaky_pauli_channel",
        [](leaky::Simulator &self,
           const std::vector<pybind11::object> &target_objs,
           const leaky::LeakyPauliChannel &channel) {
            auto targets = std::vector<stim::GateTarget>();
            for (const auto &obj : target_objs) {
                targets.push_back(leaky_pybind::obj_to_gate_target(obj));
            }
            self.apply_1q_leaky_pauli_channel({targets}, channel);
        },
        py::arg("targets"),
        py::arg("channel"));
    s.def(
        "apply_2q_leaky_pauli_channel",

        [](leaky::Simulator &self,
           const std::vector<pybind11::object> &target_objs,
           const leaky::LeakyPauliChannel &channel) {
            auto targets = std::vector<stim::GateTarget>();
            for (const auto &obj : target_objs) {
                targets.push_back(leaky_pybind::obj_to_gate_target(obj));
            }
            self.apply_2q_leaky_pauli_channel({targets}, channel);
        },
        py::arg("targets"),
        py::arg("channel"));
    s.def(
        "bind_leaky_channel",
        [](leaky::Simulator &self,
           const leaky_pybind::LeakyInstruction &ideal_inst,
           const leaky::LeakyPauliChannel &channel) {
            self.bind_leaky_channel(ideal_inst, channel);
        },
        py::arg("ideal_inst"),
        py::arg("channel"));
    s.def("clear", &leaky::Simulator::clear, py::arg("clear_bound_channels") = false);
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
        py::arg("readout_strategy") = leaky::ReadoutStrategy::RawLabel);
    s.def(
        "sample_batch",
        [](leaky::Simulator &self,
           const py::object &circuit,
           py::ssize_t shots,
           leaky::ReadoutStrategy readout_strategy) {
            auto circuit_str = pybind11::cast<std::string>(pybind11::str(circuit));
            stim::Circuit converted_circuit = stim::Circuit(circuit_str.c_str()).flattened();
            auto resolved_circuit = resolve_bound_channels_in_circuit(self, converted_circuit);
            auto num_measurements = converted_circuit.count_measurements();
            // Allocate memory for the results
            py::array_t<uint8_t> results = py::array_t<uint8_t>(shots * num_measurements);
            results[py::make_tuple(py::ellipsis())] = 0;
            py::buffer_info buff = results.request();
            uint8_t *results_ptr = (uint8_t *)buff.ptr;

            for (py::ssize_t i = 0; i < shots; i++) {
                self.clear();
                for (const auto [is_instruction, idx] : resolved_circuit.order) {
                    if (is_instruction) {
                        self.do_gate(resolved_circuit.instructions[idx], false);
                    } else {
                        const auto &[targets, channel] = resolved_circuit.channels[idx];
                        if (targets.size() == 1) {
                            self.apply_1q_leaky_pauli_channel(targets, channel);
                        } else {
                            self.apply_2q_leaky_pauli_channel(targets, channel);
                        }
                    }
                }
                self.append_measurement_record_into(results_ptr + i * num_measurements, readout_strategy);
            }
            results.resize({shots, (py::ssize_t)num_measurements});
            return results;
        },
        py::arg("circuit"),
        py::arg("shots"),
        py::arg("readout_strategy") = leaky::ReadoutStrategy::RawLabel);
    s.def_readonly("bound_leaky_channels", &leaky::Simulator::bound_leaky_channels);
}