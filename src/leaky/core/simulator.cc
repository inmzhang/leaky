#include "leaky/core/simulator.h"

#include <charconv>
#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/readout_strategy.h"

using stim::GateType;

std::optional<int> extract_leaky_channel_index(std::string_view tag) {
    constexpr std::string_view prefix = "leaky<";
    auto pos = tag.find(prefix, 0);
    if (pos == std::string_view::npos)
        return std::nullopt;
    size_t n_start = pos + prefix.size();
    size_t n_end = n_start;
    while (n_end < tag.size() && std::isdigit(tag[n_end]))
        ++n_end;
    if (n_end == n_start || tag[n_end] != '>') {
        throw std::invalid_argument("Invalid leaky channel tag: " + std::string(tag));
    }
    int number = 0;
    auto [ptr, ec] = std::from_chars(tag.data() + n_start, tag.data() + n_end, number);
    if (ec == std::errc()) {
        return number;
    }
    throw std::invalid_argument("Invalid leaky channel tag: " + std::string(tag));
}

void leaky::Simulator::handle_transition(
    uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target, std::string_view pauli) {
    leakage_status.set(target[0].qubit_value(), next_status);
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
leaky::Simulator::Simulator(uint32_t num_qubits, std::vector<LeakyPauliChannel> leaky_channels)
    : num_qubits(num_qubits),
      leakage_status(num_qubits),
      leakage_masks_record(0),
      tableau_simulator(std::mt19937_64(leaky::global_urng()), num_qubits),
      leaky_channels(std::move(leaky_channels)) {
}

void leaky::Simulator::apply_leaky_channel(
    stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel) {
    auto step = channel.num_qubits;
    if (targets.size() % step != 0) {
        throw std::invalid_argument(
            "The number of targets in the instruction should be a multiple of the number of qubits in the channel.");
    }
    for (size_t k = 0; k < targets.size(); k += step) {
        LeakageStatus target_status(step);
        for (size_t i = 0; i < step; ++i) {
            auto qubit = targets[i + k].qubit_value();
            target_status.set(i, leakage_status.get(qubit));
        }
        auto sample = channel.sample(target_status);
        if (!sample.has_value())
            return;

        auto trans = sample.value();
        auto pauli_operator = trans.pauli_operator;

        for (size_t i = 0; i < step; ++i) {
            auto target = targets.sub(i + k, i + k + 1);
            uint8_t from = target_status.get(i);
            uint8_t to = trans.to_status.get(i);
            handle_transition(from, to, target, pauli_operator.substr(i, 1));
        }
    }
}

void leaky::Simulator::do_gate(const stim::CircuitInstruction& inst) {
    auto gate_type = inst.gate_type;
    auto targets = inst.targets;
    auto flags = stim::GATE_DATA[gate_type].flags;
    // Leaky Instructions in the form of `I[leaky<n>] q0 q1 ...`
    // representing the nth leaky Pauli channel acting on the qubits q0, q1, ...
    if (gate_type == GateType::I) {
        auto tag = inst.tag;
        auto leaky_channel_index = extract_leaky_channel_index(tag);
        if (leaky_channel_index) {
            if (*leaky_channel_index >= leaky_channels.size()) {
                throw std::invalid_argument(
                    "Leaky channel index " + std::to_string(*leaky_channel_index) + " in the instruction" + inst.str() +
                    " exceeds the number of defined leaky channels: " + std::to_string(leaky_channels.size()));
            }
            auto leaky_channel = leaky_channels[*leaky_channel_index];
            apply_leaky_channel(targets, leaky_channel);
            return;
        }
    }
    // Encounter measurements: add leakage masks to the record
    if (flags & stim::GATE_PRODUCES_RESULTS) {
        for (auto q : targets) {
            leakage_masks_record.push_back(leakage_status.get(q.qubit_value()));
        }
    }
    // Encounter resets: reset the leakage status of the qubits
    if (flags & stim::GATE_IS_RESET) {
        for (auto q : targets) {
            leakage_status.set(q.qubit_value(), 0);
        }
    }
    // Do measurements or resets
    bool is_measurement_or_reset = (flags & stim::GATE_PRODUCES_RESULTS) || (flags & stim::GATE_IS_RESET);
    if (is_measurement_or_reset) {
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
        bool is_leaked = is_single_qubit_gate ? leakage_status.is_leaked(split_targets[0].data)
                                              : (leakage_status.is_leaked(split_targets[0].data) ||
                                                 leakage_status.is_leaked(split_targets[1].data));
        if (!is_leaked) {
            tableau_simulator.do_gate(split_inst);
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

void leaky::Simulator::clear() {
    leakage_status.clear();
    leakage_masks_record.clear();
    tableau_simulator.inv_state = stim::Tableau<stim::MAX_BITWORD_WIDTH>::identity(num_qubits);
    tableau_simulator.measurement_record.storage.clear();
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
