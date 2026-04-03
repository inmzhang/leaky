#include "leaky/core/simulator.pybind.h"

#include <cstddef>
#include <iterator>
#include <optional>
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

std::optional<stim::GateTarget> try_reconstruct_gate_target_from_python(const pybind11::handle& obj) {
    auto py_obj = py::reinterpret_borrow<py::object>(obj);
    if (!py::hasattr(py_obj, "is_combiner")) {
        return std::nullopt;
    }

    auto get_bool_attr = [&](const char* attr_name) -> bool {
        return py::cast<bool>(py_obj.attr(attr_name));
    };

    try {
        if (get_bool_attr("is_combiner")) {
            return stim::GateTarget::combiner();
        }
        if (get_bool_attr("is_measurement_record_target")) {
            return stim::GateTarget::rec(py::cast<int32_t>(py_obj.attr("value")));
        }
        if (get_bool_attr("is_sweep_bit_target")) {
            return stim::GateTarget::sweep_bit(py::cast<uint32_t>(py_obj.attr("value")));
        }

        auto qubit = py::cast<uint32_t>(py_obj.attr("qubit_value"));
        bool inverted = py::hasattr(py_obj, "is_inverted_result_target")
            ? py::cast<bool>(py_obj.attr("is_inverted_result_target"))
            : false;

        if (get_bool_attr("is_x_target")) {
            return stim::GateTarget::x(qubit, inverted);
        }
        if (get_bool_attr("is_y_target")) {
            return stim::GateTarget::y(qubit, inverted);
        }
        if (get_bool_attr("is_z_target")) {
            return stim::GateTarget::z(qubit, inverted);
        }
        if (get_bool_attr("is_qubit_target")) {
            return stim::GateTarget::qubit(qubit, inverted);
        }
    } catch (const pybind11::cast_error& ex) {
        return std::nullopt;
    } catch (const pybind11::error_already_set& ex) {
        return std::nullopt;
    }

    return std::nullopt;
}

stim::GateTarget handle_to_gate_target(const pybind11::handle& obj) {
    try {
        return py::cast<stim::GateTarget>(obj);
    } catch (const pybind11::cast_error &ex) {
    }
    if (auto reconstructed = try_reconstruct_gate_target_from_python(obj)) {
        return *reconstructed;
    }
    try {
        return stim::GateTarget{py::cast<uint32_t>(obj)};
    } catch (const pybind11::cast_error &ex) {
    }
    throw std::invalid_argument(
        "target argument wasn't a qubit index, a result from a `stim.target_*` method, or a `stim.GateTarget`.");
}

stim::GateTarget handle_to_qubit_gate_target(const pybind11::handle& obj) {
    auto target = handle_to_gate_target(obj);
    if (!target.is_qubit_target()) {
        throw std::invalid_argument(
            "target argument wasn't a raw qubit target. `apply_leaky_channel` only accepts integers or "
            "`stim.GateTarget(qubit)` values.");
    }
    return target;
}

py::class_<leaky::Simulator> leaky_pybind::pybind_simulator(py::module &m) {
    return {m, "Simulator"};
}

leaky::Simulator create_simulator(
    size_t num_qubits, std::vector<leaky::LeakyPauliChannel> leaky_channels, const pybind11::object &seed) {
    std::optional<uint64_t> simulator_seed;
    if (!seed.is_none()) {
        simulator_seed = seed.cast<uint64_t>();
    }
    return leaky::Simulator(num_qubits, leaky_channels, simulator_seed);
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
                targets.push_back(handle_to_qubit_gate_target(obj));
            }
            self.apply_leaky_channel({targets}, channel);
        },
        py::arg("targets"),
        py::arg("channel"));
    s.def("clear", &leaky::Simulator::clear);
    s.def(
        "current_measurement_record",
        [](leaky::Simulator &self, leaky::ReadoutStrategy readout_strategy) {
            py::array_t<uint8_t> record_arr(self.leakage_masks_record.size());
            self.append_measurement_record_into(record_arr.mutable_data(), readout_strategy);
            return record_arr;
        },
        py::arg("readout_strategy") = leaky::ReadoutStrategy::RawLabel);
    s.def(
        "sample",
        [](leaky::Simulator &self,
           const py::object &circuit,
           py::ssize_t shots,
           leaky::ReadoutStrategy readout_strategy) {
            if (shots < 0) {
                throw std::invalid_argument("shots must be non-negative.");
            }
            auto circuit_str = pybind11::cast<std::string>(pybind11::str(circuit));
            stim::Circuit converted_circuit = stim::Circuit(circuit_str.c_str()).flattened();
            auto num_measurements = static_cast<py::ssize_t>(converted_circuit.count_measurements());
            py::array_t<uint8_t> results({shots, num_measurements});

            py::gil_scoped_release release;
            self.sample_into(converted_circuit, static_cast<size_t>(shots), results.mutable_data(), readout_strategy);
            return results;
        },
        py::arg("circuit"),
        py::arg("shots"),
        py::arg("readout_strategy") = leaky::ReadoutStrategy::RawLabel);
    s.def_readonly("leaky_channels", &leaky::Simulator::leaky_channels);
    s.def_readonly("leakage_status", &leaky::Simulator::leakage_status);
    s.def_readonly("leakage_masks_record", &leaky::Simulator::leakage_masks_record);
}
