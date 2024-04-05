#include "leaky/core/simulator.pybind.h"

#include <iterator>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <vector>

#include "leaky/core/instruction.pybind.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/simulator.h"

py::class_<leaky::Simulator> leaky_pybind::pybind_simulator(py::module &m) {
    return {m, "Simulator"};
}

leaky::Simulator create_simulator(uint32_t num_qubits, const pybind11::object &seed) {
    if (!seed.is_none()) {
        leaky::set_seed(seed.cast<unsigned>());
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
    s.def("do_circuit", &leaky::Simulator::do_circuit, py::arg("circuit"));
    s.def(
        "do",
        [](leaky::Simulator &self, const leaky_pybind::LeakyInstruction &instruction) {
            self.do_gate(instruction);
        },
        py::arg("instruction"));
    s.def(
        "do_1q_leaky_pauli_channel",
        [](leaky::Simulator &self,
           const leaky_pybind::LeakyInstruction &ideal_inst,
           const leaky::LeakyPauliChannel &channel) {
            self.do_1q_leaky_pauli_channel(ideal_inst, channel);
        },
        py::arg("ideal_inst"),
        py::arg("channel"));
    s.def(
        "do_2q_leaky_pauli_channel",

        [](leaky::Simulator &self,
           const leaky_pybind::LeakyInstruction &ideal_inst,
           const leaky::LeakyPauliChannel &channel) {
            self.do_2q_leaky_pauli_channel(ideal_inst, channel);
        },
        py::arg("ideal_inst"),
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
           const stim::Circuit &circuit,
           py::ssize_t shots,
           leaky::ReadoutStrategy readout_strategy = leaky::ReadoutStrategy::RawLabel) {
            auto num_measurements = circuit.count_measurements();
            // Allocate memory for the results
            py::array_t<uint8_t> results = py::array_t<uint8_t>(shots * num_measurements);
            results[py::make_tuple(py::ellipsis())] = 0;
            py::buffer_info buff = results.request();
            uint8_t *results_ptr = (uint8_t *)buff.ptr;

            for (py::ssize_t i = 0; i < shots; i++) {
                self.clear();
                self.do_circuit(circuit);
                self.append_measurement_record_into(results_ptr + i * num_measurements, readout_strategy);
            }
            results.resize({shots, (py::ssize_t)num_measurements});
            return results;
        });
    s.def_readonly("bound_leaky_channels", &leaky::Simulator::bound_leaky_channels);
}