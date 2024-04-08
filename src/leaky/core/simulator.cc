#include "leaky/core/simulator.h"

#include <cstddef>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/readout_strategy.h"
#include "stim.h"
#include "stim/circuit/circuit_instruction.h"

using stim::GateType;

leaky::Simulator::Simulator(uint32_t num_qubits)
    : num_qubits(num_qubits),
      leakage_status(num_qubits, 0),
      leakage_masks_record(0),
      tableau_simulator(std::mt19937_64(leaky::global_urng()), num_qubits),
      bound_leaky_channels({}) {
}

void leaky::Simulator::handle_u_or_d(
    uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target) {
    auto transition_type = leaky::get_transition_type(cur_status, next_status);
    const auto args = std::vector<double>{0.5};
    const stim::CircuitInstruction x_error = {GateType::X_ERROR, args, target};
    const stim::CircuitInstruction reset = {GateType::R, {}, target};

    if (transition_type == leaky::TransitionType::U) {
        tableau_simulator.do_X_ERROR(x_error);
        tableau_simulator.do_RZ(reset);
    } else if (transition_type == leaky::TransitionType::D) {
        tableau_simulator.do_RZ(reset);
        tableau_simulator.do_X_ERROR(x_error);
    }
}

void leaky::Simulator::bind_leaky_channel(
    const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel) {
    size_t inst_id = std::hash<std::string>{}(ideal_inst.str());
    auto flags = stim::GATE_DATA[ideal_inst.gate_type].flags;
    if (!(flags & stim::GATE_IS_UNITARY)) {
        throw std::invalid_argument("Only unitary gates can be binded with a leaky channel.");
    }
    bound_leaky_channels.insert({inst_id, channel});
}

void leaky::Simulator::do_1q_leaky_pauli_channel(
    const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel) {
    auto targets = ideal_inst.targets;
    for (size_t i = 0; i < targets.size(); i++) {
        auto qubit = targets[i].data;
        auto target = targets.sub(i, i + 1);
        uint8_t cur_status = leakage_status[qubit];
        if (cur_status == 0) {
            tableau_simulator.do_gate({ideal_inst.gate_type, ideal_inst.args, target});
        }
        auto sample = channel.sample(cur_status);
        if (!sample.has_value()) {
            return;
        }
        auto [next_status, pauli_channel_idx] = sample.value();
        leakage_status[qubit] = next_status;
        handle_u_or_d(cur_status, next_status, target);
        auto pauli_str = leaky::pauli_idx_to_string(pauli_channel_idx, true);
        tableau_simulator.do_gate({stim::GATE_DATA.at(pauli_str).id, {}, target});
    }
}

void leaky::Simulator::do_2q_leaky_pauli_channel(
    const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel) {
    auto targets = ideal_inst.targets;
    for (size_t k = 0; k < targets.size(); k += 2) {
        auto t1 = targets.sub(k, k + 1);
        auto t2 = targets.sub(k + 1, k + 2);
        auto pair = targets.sub(k, k + 2);
        auto q1 = targets[k].data;
        auto q2 = targets[k + 1].data;
        auto cs1 = leakage_status[q1];
        auto cs2 = leakage_status[q2];
        uint8_t cur_status = (cs1 << 4) | cs2;
        if (cur_status == 0) {
            tableau_simulator.do_gate({ideal_inst.gate_type, ideal_inst.args, pair});
        }
        auto sample = channel.sample(cur_status);
        if (!sample.has_value()) {
            return;
        }
        auto [next_status, pauli_channel_idx] = sample.value();
        uint8_t ns1 = next_status >> 4;
        uint8_t ns2 = next_status & 0x0F;
        leakage_status[q1] = ns1;
        leakage_status[q2] = ns2;
        handle_u_or_d(cs1, ns1, t1);
        handle_u_or_d(cs2, ns2, t2);

        auto pauli_str = leaky::pauli_idx_to_string(pauli_channel_idx, false);
        tableau_simulator.do_gate({stim::GATE_DATA.at(pauli_str.c_str(), 1).id, {}, t1});
        tableau_simulator.do_gate({stim::GATE_DATA.at(&pauli_str.c_str()[1], 1).id, {}, t2});
    }
}

void leaky::Simulator::do_measurement(const stim::CircuitInstruction& inst) {
    const auto& targets = inst.targets;
    for (auto q : targets) {
        leakage_masks_record.push_back(leakage_status[q.qubit_value()]);
    }
    tableau_simulator.do_gate(inst);
}

void leaky::Simulator::do_reset(const stim::CircuitInstruction& inst) {
    const auto& targets = inst.targets;
    for (auto q : targets) {
        leakage_status[q.qubit_value()] = 0;
    }
    tableau_simulator.do_gate(inst);
}

void leaky::Simulator::do_gate_without_leak(const stim::CircuitInstruction& inst) {
    switch (inst.gate_type) {
        case GateType::M:
            do_measurement(inst);
            break;
        case GateType::R:
            do_reset(inst);
            break;
        case GateType::MR:
            do_measurement(inst);
            do_reset(inst);
            break;
        case GateType::MX:
        case GateType::MY:
        case GateType::RX:
        case GateType::RY:
        case GateType::MRX:
        case GateType::MRY:
        case GateType::MPP:
            throw std::invalid_argument("Only Z basis measurements and resets are supported in the leaky simulator.");
        default:
            tableau_simulator.do_gate(inst);
    }
}

void leaky::Simulator::do_gate(const stim::CircuitInstruction& inst) {
    if (bound_leaky_channels.empty()) {
        do_gate_without_leak(inst);
        return;
    }
    bool is_single_qubit_gate = stim::GATE_DATA[inst.gate_type].flags & stim::GATE_IS_SINGLE_QUBIT_GATE;
    size_t step = is_single_qubit_gate ? 1 : 2;
    for (size_t i = 0; i < inst.targets.size(); i += step) {
        auto targets = inst.targets.sub(i, i + step);
        const stim::CircuitInstruction split_inst = {inst.gate_type, inst.args, targets};
        const auto inst_id = std::hash<std::string>{}(split_inst.str());
        auto it = bound_leaky_channels.find(inst_id);
        if (it != bound_leaky_channels.end()) {
            auto channel = it->second;
            if (is_single_qubit_gate) {
                do_1q_leaky_pauli_channel(split_inst, channel);
            } else {
                do_2q_leaky_pauli_channel(split_inst, channel);
            }
        } else {
            do_gate_without_leak(split_inst);
        }
    }
}

void leaky::Simulator::do_circuit(const stim::Circuit& circuit) {
    if (circuit.count_qubits() > num_qubits) {
        throw std::invalid_argument(
            "The number of qubits in the circuit exceeds the maximum capacity of the simulator.");
    }
    for (const auto& op : circuit.operations) {
        if (op.gate_type == GateType::REPEAT) {
            uint64_t repeats = op.repeat_block_rep_count();
            const auto& block = op.repeat_block_body(circuit);
            for (uint64_t k = 0; k < repeats; k++) {
                do_circuit(block);
            }
        } else {
            do_gate(op);
        }
    }
}

void leaky::Simulator::clear(bool clear_bound_channels) {
    leakage_status = std::vector<uint8_t>(num_qubits, 0);
    leakage_masks_record.clear();
    tableau_simulator.inv_state = stim::Tableau<stim::MAX_BITWORD_WIDTH>::identity(num_qubits);
    tableau_simulator.measurement_record.storage.clear();
    if (clear_bound_channels) {
        bound_leaky_channels.clear();
    }
}

std::vector<uint8_t> leaky::Simulator::current_measurement_record(ReadoutStrategy readout_strategy) {
    auto results = std::vector<uint8_t>(leakage_masks_record.size());
    append_measurement_record_into(results.data(), readout_strategy);
    return results;
}

void leaky::Simulator::append_measurement_record_into(uint8_t* record_begin_ptr, ReadoutStrategy readout_strategy) {
    auto tableau_record = tableau_simulator.measurement_record.storage;
    auto num_measurements = leakage_masks_record.size();
    if (readout_strategy == ReadoutStrategy::RawLabel) {
        for (auto i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            *(record_begin_ptr + i) = mask == 0 ? (uint8_t)tableau_record[i] : mask + 1;
        }
    } else if (readout_strategy == ReadoutStrategy::RandomLeakageProjection) {
        for (auto i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            *(record_begin_ptr + i) =
                mask == 0 ? (uint8_t)tableau_record[i] : (leaky::rand_float(0.0, 1.0) < 0.5 ? 0 : 1);
        }
    } else if (readout_strategy == ReadoutStrategy::DeterministicLeakageProjection) {
        for (auto i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            *(record_begin_ptr + i) = mask == 0 ? (uint8_t)tableau_record[i] : 1;
        }
    } else {
        throw std::invalid_argument("Invalid readout strategy.");
    }
}