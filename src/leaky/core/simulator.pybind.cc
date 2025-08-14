#include "leaky/core/simulator.pybind.h"

#include <cstddef>
#include <iterator>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/simulator.h"
#include "stim/circuit/circuit_instruction.h"

stim::GateType parse_gate_type(std::string_view gate_name) {
    return stim::GATE_DATA.at(gate_name).id;
}

stim::GateTarget handle_to_gate_target(const pybind11::handle &obj) {
    try {
        return py::cast<stim::GateTarget>(obj);
    } catch (const pybind11::cast_error &ex) {
    }
    try {
        return stim::GateTarget{py::cast<uint32_t>(obj)};
    } catch (const pybind11::cast_error &ex) {
    }
    throw std::invalid_argument(
        "target argument wasn't a qubit index, a result from a `stim.target_*` method, or a `stim.GateTarget`.");
}

py::class_<leaky::Simulator> leaky_pybind::pybind_simulator(py::module &m) {
    return {m, "Simulator"};
}

leaky::Simulator create_simulator(
    size_t num_qubits, std::vector<leaky::LeakyPauliChannel> leaky_channels, const pybind11::object &seed) {
    if (!seed.is_none()) {
        leaky::set_seed(seed.cast<unsigned>());
    } else {
        leaky::randomize();
    }
    return leaky::Simulator(num_qubits, leaky_channels);
}

void leaky_pybind::pybind_simulator_methods(py::module &m, py::class_<leaky::Simulator> &s) {
    py::enum_<leaky::ReadoutStrategy>(m, "ReadoutStrategy", py::arithmetic())
        .value("RawLabel", leaky::ReadoutStrategy::RawLabel)
        .value("RandomLeakageProjection", leaky::ReadoutStrategy::RandomLeakageProjection)
        .value("DeterministicLeakageProjection", leaky::ReadoutStrategy::DeterministicLeakageProjection)
        .export_values();

    s.def(
        py::init(&create_simulator),
        py::arg("num_qubits"),
        py::arg("leaky_channels") = std::vector<leaky::LeakyPauliChannel>{},
        pybind11::kw_only(),
        py::arg("seed") = pybind11::none());
    s.def(
        "do_circuit",
        [](leaky::Simulator &self, const py::object &circuit) {
            auto circuit_str = pybind11::cast<std::string>(pybind11::str(circuit));
            stim::Circuit converted_circuit = stim::Circuit(circuit_str.c_str());
            self.do_circuit(converted_circuit);
        },
        py::arg("circuit"));
    s.def(
        "do_gate",
        [](leaky::Simulator &self,
           std::string_view name,
           const std::vector<pybind11::object> &target_objs,
           const std::vector<double> &args = {},
           std::string_view tag = "") {
            auto targets = std::vector<stim::GateTarget>();
            for (const auto &obj : target_objs) {
                targets.push_back(handle_to_gate_target(obj));
            }
            stim::CircuitInstruction circuit_inst = {parse_gate_type(name), args, targets, tag};
            self.do_gate(circuit_inst);
        },
        py::arg("name"),
        py::arg("targets"),
        py::arg("args") = std::vector<double>{},
        py::arg("tag") = "");
    s.def(
        "apply_leaky_channel",
        [](leaky::Simulator &self,
           const std::vector<pybind11::object> &target_objs,
           const leaky::LeakyPauliChannel &channel) {
            auto targets = std::vector<stim::GateTarget>();
            for (const auto &obj : target_objs) {
                targets.push_back(handle_to_gate_target(obj));
            }
            self.apply_leaky_channel({targets}, channel);
        },
        py::arg("targets"),
        py::arg("leaky_channel"));
    s.def("clear", &leaky::Simulator::clear);
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
        "sample",
        [](leaky::Simulator &self,
           const py::object &circuit,
           py::ssize_t shots,
           leaky::ReadoutStrategy readout_strategy) {
            auto circuit_str = pybind11::cast<std::string>(pybind11::str(circuit));
            stim::Circuit converted_circuit = stim::Circuit(circuit_str.c_str()).flattened();
            auto num_measurements = converted_circuit.count_measurements();
            // Allocate memory for the results
            py::array_t<uint8_t> results = py::array_t<uint8_t>(shots * num_measurements);
            results[py::make_tuple(py::ellipsis())] = 0;
            py::buffer_info buff = results.request();
            uint8_t *results_ptr = (uint8_t *)buff.ptr;

            for (py::ssize_t i = 0; i < shots; i++) {
                self.clear();
                self.do_circuit(converted_circuit);
                self.append_measurement_record_into(results_ptr + i * num_measurements, readout_strategy);
            }
            results.resize({shots, (py::ssize_t)num_measurements});
            return results;
        },
        py::arg("circuit"),
        py::arg("shots"),
        py::arg("readout_strategy") = leaky::ReadoutStrategy::RawLabel);
    s.def_readonly("leaky_channels", &leaky::Simulator::leaky_channels);
    s.def_readonly("leakage_status", &leaky::Simulator::leakage_status);
    s.def_readonly("leakage_masks_record", &leaky::Simulator::leakage_masks_record);
}
