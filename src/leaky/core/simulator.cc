#include "leaky/core/simulator.h"

#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "leaky/core/rand_gen.h"
#include "leaky/core/readout_strategy.h"
#include "leaky/core/transition.h"
#include "stim/circuit/circuit_instruction.h"
#include "stim/circuit/gate_data.h"
#include "stim/simulators/tableau_simulator.h"

using stim::GateType;

static std::array<std::string, 4> PAULI_1Q = {
    "I",
    "X",
    "Y",
    "Z",
};

static std::array<std::string, 16> PAULI_2Q = {
    "II",
    "IX",
    "IY",
    "IZ",
    "XI",
    "XX",
    "XY",
    "XZ",
    "YI",
    "YX",
    "YY",
    "YZ",
    "ZI",
    "ZX",
    "ZY",
    "ZZ",
};

leaky::TransitionType get_transition_type(uint8_t initial_status, uint8_t final_status) {
    if (initial_status == 0 && final_status == 0) {
        return leaky::TransitionType::R;
    } else if (initial_status == 0 && final_status > 0) {
        return leaky::TransitionType::U;
    } else if (initial_status > 0 && final_status == 0) {
        return leaky::TransitionType::D;
    } else {
        return leaky::TransitionType::L;
    }
}

std::string pauli_idx_to_string(uint8_t idx, bool is_single_qubit_channel) {
    if (is_single_qubit_channel) {
        return PAULI_1Q[idx];
    }
    return PAULI_2Q[idx];
}

leaky::Simulator::Simulator(uint32_t num_qubits, std::map<inst_id, const LeakyPauliChannel&> binded_leaky_channels)
    : num_qubits(num_qubits),
      leakage_status(num_qubits, 0),
      leakage_masks_record(0),
      binded_leaky_channels(binded_leaky_channels),
      tableau_simulator(std::mt19937_64(leaky::global_urng()), num_qubits) {
}

void leaky::Simulator::handle_u_or_d(uint8_t cur_status, uint8_t next_status, stim::GateTarget target) {
    auto transition_type = get_transition_type(cur_status, next_status);
    std::vector<stim::GateTarget> one_q_target = {target};
    std::vector<double> args = {0.5};
    const stim::CircuitInstruction x_error = {GateType::X_ERROR, args, one_q_target};
    const stim::CircuitInstruction reset = {GateType::R, {}, one_q_target};

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
    inst_id inst_key = {ideal_inst.gate_type, ideal_inst.args, ideal_inst.targets};
    binded_leaky_channels.insert({inst_key, channel});
}

void leaky::Simulator::do_1q_leaky_pauli_channel(
    const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel) {
    auto targets = ideal_inst.targets;
    for (auto q : targets) {
        uint32_t qubit = q.data;
        uint8_t cur_status = leakage_status[qubit];
        if (cur_status == 0) {
            tableau_simulator.do_gate(ideal_inst);
        }
        auto [next_status, pauli_channel_idx] = channel.sample(cur_status);
        leakage_status[qubit] = next_status;
        handle_u_or_d(cur_status, next_status, q);
        auto pauli_str = pauli_idx_to_string(pauli_channel_idx, true);
        tableau_simulator.do_gate({stim::GATE_DATA.at(pauli_str).id, {}, {std::vector<stim::GateTarget>{q}}});
    }
}

void leaky::Simulator::do_2q_leaky_pauli_channel(
    const stim::CircuitInstruction& ideal_inst, const LeakyPauliChannel& channel) {
    auto targets = ideal_inst.targets;
    for (size_t k = 0; k < targets.size(); k += 2) {
        auto t1 = targets[k];
        auto t2 = targets[k + 1];
        auto q1 = t1.data;
        auto q2 = t2.data;
        auto cs1 = leakage_status[q1];
        auto cs2 = leakage_status[q2];
        uint8_t cur_status = (cs1 << 4) | cs2;
        if (cur_status == 0) {
            tableau_simulator.do_gate(ideal_inst);
        }
        auto [next_status, pauli_channel_idx] = channel.sample(cur_status);
        uint8_t ns1 = next_status >> 4;
        uint8_t ns2 = next_status & 0x0F;
        leakage_status[q1] = ns1;
        leakage_status[q2] = ns2;
        handle_u_or_d(cs1, ns1, t1);
        handle_u_or_d(cs2, ns2, t2);

        auto pauli_str = pauli_idx_to_string(pauli_channel_idx, false);
        tableau_simulator.do_gate(
            {stim::GATE_DATA.at(pauli_str.substr(0, 1)).id, {}, {std::vector<stim::GateTarget>{t1}}});
        tableau_simulator.do_gate(
            {stim::GATE_DATA.at(pauli_str.substr(1, 1)).id, {}, {std::vector<stim::GateTarget>{t2}}});
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

void leaky::Simulator::do_gate(const stim::CircuitInstruction& inst) {
    bool is_single_qubit_gate = stim::GATE_DATA[inst.gate_type].flags & stim::GATE_IS_SINGLE_QUBIT_GATE;
    size_t step = is_single_qubit_gate ? 1 : 2;
    const auto& targets = inst.targets;
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

void leaky::Simulator::do_circuit(const stim::Circuit& circuit) {
    for (const auto& op : circuit.operations) {
        if (op.gate_type == GateType::REPEAT) {
            uint64_t repeats = op.repeat_block_rep_count();
            const auto& block = op.repeat_block_body(circuit);
            for (uint64_t k = 0; k < repeats; k++) {
                do_circuit(block);
            }
        } else {
            auto inst_key = inst_id{op.gate_type, op.args, op.targets};
            auto it = binded_leaky_channels.find(inst_key);
            if (it != binded_leaky_channels.end()) {
                auto channel = it->second;
                auto flags = stim::GATE_DATA[op.gate_type].flags;
                if (flags & stim::GATE_IS_UNITARY) {
                    if (flags & stim::GATE_IS_SINGLE_QUBIT_GATE) {
                        do_1q_leaky_pauli_channel(op, channel);
                    } else {
                        do_2q_leaky_pauli_channel(op, channel);
                    }
                } else {
                    do_gate(op);
                }
            } else {
                do_gate(op);
            }
        }
    }
}

void leaky::Simulator::clear(bool clear_binded_channels) {
    leakage_status = std::vector<uint8_t>(num_qubits, 0);
    leakage_masks_record.clear();
    tableau_simulator =
        stim::TableauSimulator<stim::MAX_BITWORD_WIDTH>{std::mt19937_64(leaky::global_urng()), leakage_status.size()};
    if (clear_binded_channels) {
        binded_leaky_channels.clear();
    }
}

std::vector<uint8_t> leaky::Simulator::current_measurement_record(ReadoutStrategy readout_strategy) {
    std::vector<uint8_t> results;
    auto tableau_record = tableau_simulator.measurement_record.storage;
    auto num_measurements = leakage_masks_record.size();
    results.reserve(num_measurements);
    if (readout_strategy == ReadoutStrategy::RawLabel) {
        for (auto i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            results.push_back(mask == 0 ? (uint8_t)tableau_record[i] : mask);
        }
    } else if (readout_strategy == ReadoutStrategy::RandomLeakageProjection) {
        for (auto i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            results.push_back(mask == 0 ? (uint8_t)tableau_record[i] : (leaky::rand_float(0.0, 1.0) < 0.5 ? 0 : 1));
        }
    } else if (readout_strategy == ReadoutStrategy::DeterministicLeakageProjection) {
        for (auto i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            results.push_back(mask == 0 ? (uint8_t)tableau_record[i] : 1);
        }
    } else {
        throw std::invalid_argument("Invalid readout strategy.");
    }
    return results;
}