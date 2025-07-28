#include "leaky/core/simulator.h"

#include <cstddef>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/readout_strategy.h"
#include "stim.h"

using stim::GateType;

leaky::Simulator::Simulator(uint32_t num_qubits)
    : num_qubits(num_qubits),
      leakage_status(num_qubits, 0),
      leakage_masks_record(0),
      tableau_simulator(std::mt19937_64(leaky::global_urng()), num_qubits),
      bound_leaky_channels({}) {
}

void leaky::Simulator::handle_transition(
    uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target, std::string_view pauli) {
    switch (leaky::get_transition_type(cur_status, next_status)) {
        case leaky::TransitionType::R:
            tableau_simulator.do_gate({stim::GATE_DATA.at(pauli).id, {}, target, {}});
            return;
        case leaky::TransitionType::L:
            return;
        case leaky::TransitionType::U:
            tableau_simulator.do_X_ERROR({GateType::X_ERROR, std::vector<double>{0.5}, target, {}});
            // tableau_simulator.do_RZ({GateType::R, {}, target});
            return;
        case leaky::TransitionType::D:
            tableau_simulator.do_RZ({GateType::R, {}, target, {}});
            tableau_simulator.do_X_ERROR({GateType::X_ERROR, std::vector<double>{0.5}, target, {}});
            return;
    }
}

void leaky::Simulator::bind_leaky_channel(
    const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel) {
    size_t inst_id = std::hash<std::string>{}(ideal_inst.str());
    if (!(stim::GATE_DATA[ideal_inst.gate_type].flags & stim::GATE_IS_UNITARY)) {
        throw std::invalid_argument("Only unitary gates can be binded with a leaky channel.");
    }
    bound_leaky_channels.insert({inst_id, channel});
}

void leaky::Simulator::apply_1q_leaky_pauli_channel(
    stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel) {
    for (size_t i = 0; i < targets.size(); i++) {
        auto qubit = targets[i].data;
        auto target = targets.sub(i, i + 1);
        uint8_t cur_status = leakage_status[qubit];
        auto sample = channel.sample(cur_status);
        if (!sample.has_value()) {
            return;
        }
        auto [next_status, pauli_channel_idx] = sample.value();
        leakage_status[qubit] = next_status;
        auto pauli_str = leaky::pauli_idx_to_string(pauli_channel_idx, true);
        handle_transition(cur_status, next_status, target, pauli_str.c_str());
    }
}

void leaky::Simulator::apply_2q_leaky_pauli_channel(
    stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel) {
    for (size_t k = 0; k < targets.size(); k += 2) {
        auto t1 = targets.sub(k, k + 1);
        auto t2 = targets.sub(k + 1, k + 2);
        auto q1 = targets[k].data;
        auto q2 = targets[k + 1].data;
        auto cs1 = leakage_status[q1];
        auto cs2 = leakage_status[q2];
        uint8_t cur_status = (cs1 << 4) | cs2;
        auto sample = channel.sample(cur_status);
        if (!sample.has_value()) {
            return;
        }
        auto [next_status, pauli_channel_idx] = sample.value();
        uint8_t ns1 = next_status >> 4;
        uint8_t ns2 = next_status & 0x0F;
        leakage_status[q1] = ns1;
        leakage_status[q2] = ns2;
        auto pauli_str = leaky::pauli_idx_to_string(pauli_channel_idx, false);
        handle_transition(cs1, ns1, t1, pauli_str.substr(0, 1));
        handle_transition(cs2, ns2, t2, pauli_str.substr(1, 1));
    }
}

void leaky::Simulator::do_gate(const stim::CircuitInstruction& inst, bool look_up_bound_channels) {
    // Handle measurements and resets.
    auto gate_type = inst.gate_type;
    auto targets = inst.targets;
    auto flags = stim::GATE_DATA[gate_type].flags;
    // Skip annotations.
    if (flags & stim::GATE_HAS_NO_EFFECT_ON_QUBITS) {
        return;
    }
    // Encounter measurements: add leakage masks to the record
    if (flags & stim::GATE_PRODUCES_RESULTS) {
        for (auto q : targets) {
            leakage_masks_record.push_back(leakage_status[q.qubit_value()]);
        }
    }
    // Encounter resets: reset the leakage status of the qubits
    if (flags & stim::GATE_IS_RESET) {
        for (auto q : targets) {
            leakage_status[q.qubit_value()] = 0;
        }
    }
    if ((flags & stim::GATE_PRODUCES_RESULTS) || (flags & stim::GATE_IS_RESET)) {
        tableau_simulator.do_gate(inst);
        return;
    }
    // Do noisy channels
    if (flags & stim::GATE_IS_NOISY) {
        tableau_simulator.do_gate(inst);
        return;
    }

    bool is_single_qubit_gate = flags & stim::GATE_IS_SINGLE_QUBIT_GATE;
    size_t step = is_single_qubit_gate ? 1 : 2;
    for (size_t i = 0; i < targets.size(); i += step) {
        auto split_targets = targets.sub(i, i + step);
        stim::CircuitInstruction split_inst = {gate_type, inst.args, split_targets, {}};
        // If all qubits are in the R state, we can apply the ideal gate.
        bool all_target_is_in_r =
            is_single_qubit_gate
                ? leakage_status[split_targets[0].data] == 0
                : ((leakage_status[split_targets[0].data] << 4) | leakage_status[split_targets[1].data]) == 0;
        if (all_target_is_in_r) {
            tableau_simulator.do_gate(split_inst);
        }
        if (!look_up_bound_channels || bound_leaky_channels.empty()) {
            continue;
        }
        // Look up the bound leaky channel for the ideal gate.
        const auto inst_id = std::hash<std::string>{}(split_inst.str());
        auto it = bound_leaky_channels.find(inst_id);
        if (it == bound_leaky_channels.end()) {
            continue;
        }
        const auto& channel = it->second;
        if (is_single_qubit_gate) {
            apply_1q_leaky_pauli_channel(split_targets, channel);
        } else {
            apply_2q_leaky_pauli_channel(split_targets, channel);
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
