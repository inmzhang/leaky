#include "leaky/core/simulator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "leaky/core/channel.h"
#include "leaky/core/rand_gen.h"
#include "leaky/core/readout_strategy.h"

using stim::GateType;

namespace {

uint64_t random_seed_from_device() {
    static std::random_device rd{};
    return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
}

std::mt19937_64 make_tableau_rng(std::optional<uint64_t> seed) {
    return std::mt19937_64(seed.value_or(random_seed_from_device()));
}

std::mt19937_64 make_leakage_rng(std::optional<uint64_t> seed) {
    constexpr uint64_t kLeakageSeedSalt = 0x9E3779B97F4A7C15ULL;
    return std::mt19937_64(seed.has_value() ? (*seed ^ kLeakageSeedSalt) : random_seed_from_device());
}

std::optional<size_t> extract_leaky_channel_index(std::string_view tag) {
    constexpr std::string_view prefix = "leaky<";
    if (tag.empty() || !tag.starts_with(prefix)) {
        return std::nullopt;
    }

    if (tag.back() != '>') {
        throw std::invalid_argument("Invalid leaky channel tag: " + std::string(tag));
    }

    size_t n_start = prefix.size();
    size_t n_end = n_start;
    while (n_end < tag.size() - 1 && std::isdigit(static_cast<unsigned char>(tag[n_end]))) {
        ++n_end;
    }
    if (n_end == n_start || n_end != tag.size() - 1) {
        throw std::invalid_argument("Invalid leaky channel tag: " + std::string(tag));
    }

    size_t number = 0;
    auto [ptr, ec] = std::from_chars(tag.data() + n_start, tag.data() + n_end, number);
    if (ec == std::errc() && ptr == tag.data() + n_end) {
        return number;
    }
    throw std::invalid_argument("Invalid leaky channel tag: " + std::string(tag));
}

void validate_qubit_targets(stim::SpanRef<const stim::GateTarget> targets, std::string_view context) {
    for (const auto& target : targets) {
        if (!target.is_qubit_target()) {
            throw std::invalid_argument(
                std::string(context) + " only supports raw qubit targets, but got " + target.target_str() + ".");
        }
    }
}

void apply_single_qubit_pauli(
    stim::TableauSimulator<stim::MAX_BITWORD_WIDTH>& tableau_simulator,
    stim::SpanRef<const stim::GateTarget> target,
    char pauli) {
    switch (pauli) {
        case 'I':
            return;
        case 'X':
            tableau_simulator.do_X({GateType::X, {}, target, {}});
            return;
        case 'Y':
            tableau_simulator.do_Y({GateType::Y, {}, target, {}});
            return;
        case 'Z':
            tableau_simulator.do_Z({GateType::Z, {}, target, {}});
            return;
        default:
            throw std::invalid_argument("Unsupported single-qubit Pauli operator.");
    }
}

}  // namespace

void leaky::Simulator::handle_transition(
    uint8_t cur_status, uint8_t next_status, stim::SpanRef<const stim::GateTarget> target, char pauli) {
    auto qubit = target[0].qubit_value();
    leakage_status.s[qubit] = next_status;
    switch (leaky::get_transition_type(cur_status, next_status)) {
        case leaky::TransitionType::R:
            apply_single_qubit_pauli(tableau_simulator, target, pauli);
            return;
        case leaky::TransitionType::L:
            return;
        case leaky::TransitionType::U:
            if (leaky::rand_float(0.0, 1.0, leakage_rng) < 0.5) {
                tableau_simulator.do_X({GateType::X, {}, target, {}});
            }
            return;
        case leaky::TransitionType::D:
            tableau_simulator.do_RZ({GateType::R, {}, target, {}});
            if (leaky::rand_float(0.0, 1.0, leakage_rng) < 0.5) {
                tableau_simulator.do_X({GateType::X, {}, target, {}});
            }
            return;
    }
}
leaky::Simulator::Simulator(
    uint32_t num_qubits, std::vector<LeakyPauliChannel> leaky_channels, std::optional<uint64_t> seed)
    : num_qubits(num_qubits),
      leakage_status(num_qubits),
      leakage_masks_record(),
      leakage_rng(make_leakage_rng(seed)),
      tableau_simulator(make_tableau_rng(seed), num_qubits),
      leaky_channels(std::move(leaky_channels)) {
}

uint8_t leaky::Simulator::compute_group_leakage_mask(stim::SpanRef<const stim::GateTarget> group) const {
    uint8_t leakage_mask = 0;
    bool saw_qubit = false;
    for (const auto& target : group) {
        if (target.is_combiner()) {
            continue;
        }
        if (!target.has_qubit_value()) {
            throw std::invalid_argument(
                "Result-producing instruction contains an unsupported non-qubit target: " + target.target_str() + ".");
        }
        saw_qubit = true;
        leakage_mask = std::max(leakage_mask, leakage_status.s[target.qubit_value()]);
    }
    return saw_qubit ? leakage_mask : 0;
}

void leaky::Simulator::append_result_masks(const stim::CircuitInstruction& inst) {
    auto produced_results = inst.count_measurement_results();
    if (produced_results == 0) {
        return;
    }
    if (inst.gate_type == GateType::MPAD) {
        leakage_masks_record.insert(leakage_masks_record.end(), produced_results, 0);
        return;
    }

    auto starting_size = leakage_masks_record.size();
    leakage_masks_record.reserve(starting_size + produced_results);
    inst.for_combined_target_groups([&](std::span<const stim::GateTarget> group) {
        leakage_masks_record.push_back(compute_group_leakage_mask(group));
    });

    auto appended_masks = leakage_masks_record.size() - starting_size;
    if (appended_masks != produced_results) {
        throw std::runtime_error(
            "Internal error: result mask count does not match Stim's measurement count for instruction " + inst.str());
    }
}

void leaky::Simulator::apply_leaky_channel(
    stim::SpanRef<const stim::GateTarget> targets, const LeakyPauliChannel& channel) {
    auto step = channel.num_qubits;
    if (step == 0) {
        throw std::invalid_argument("A leaky channel must act on at least one qubit.");
    }
    validate_qubit_targets(targets, "apply_leaky_channel");
    if (targets.size() % step != 0) {
        throw std::invalid_argument(
            "The number of targets in the instruction should be a multiple of the number of qubits in the channel.");
    }
    std::vector<uint8_t> target_status(step);
    for (size_t k = 0; k < targets.size(); k += step) {
        for (size_t i = 0; i < step; ++i) {
            auto qubit = targets[i + k].qubit_value();
            target_status[i] = leakage_status.s[qubit];
        }
        auto sample = channel.sample_weighted_transition(target_status, leakage_rng);
        if (sample == nullptr) {
            continue;
        }

        const auto& trans = sample->transition;
        const auto& pauli_operator = trans.pauli_operator;
        const auto* to_status = trans.to_status.s.data();
        const auto* pauli_data = pauli_operator.data();

        for (size_t i = 0; i < step; ++i) {
            auto target = targets.sub(i + k, i + k + 1);
            handle_transition(target_status[i], to_status[i], target, pauli_data[i]);
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
            const auto& leaky_channel = leaky_channels[*leaky_channel_index];
            apply_leaky_channel(targets, leaky_channel);
            return;
        }
    }
    // Encounter measurements: add leakage masks to the record
    if (flags & stim::GATE_PRODUCES_RESULTS) {
        append_result_masks(inst);
    }
    // Encounter resets: reset the leakage status of the qubits
    if (flags & stim::GATE_IS_RESET) {
        for (const auto& target : targets) {
            if (!target.has_qubit_value()) {
                throw std::invalid_argument("Reset instruction contains a non-qubit target: " + target.target_str() + ".");
            }
            leakage_status.reset(target.qubit_value());
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

    bool has_qubit_targets = std::any_of(targets.begin(), targets.end(), [](const auto& target) {
        return target.has_qubit_value();
    });
    if (!has_qubit_targets) {
        tableau_simulator.do_gate(inst);
        return;
    }

    auto handle_ideal_gate_group = [&](std::span<const stim::GateTarget> group) {
        for (const auto& target : group) {
            if (target.has_qubit_value() && leakage_status.s[target.qubit_value()] > 0) {
                return;
            }
        }
        tableau_simulator.do_gate({gate_type, inst.args, group, {}});
    };

    if (flags & stim::GATE_TARGETS_COMBINERS) {
        inst.for_combined_target_groups(handle_ideal_gate_group);
        return;
    }

    bool is_single_qubit_gate = flags & stim::GATE_IS_SINGLE_QUBIT_GATE;
    size_t step = is_single_qubit_gate ? 1 : 2;
    if (targets.size() % step != 0) {
        throw std::invalid_argument("Unsupported target grouping for ideal instruction: " + inst.str());
    }
    for (size_t i = 0; i < targets.size(); i += step) {
        handle_ideal_gate_group(targets.sub(i, i + step));
    }
}

void leaky::Simulator::do_circuit(const stim::Circuit& circuit) {
    do_circuit_internal(circuit, true);
}

void leaky::Simulator::do_circuit_internal(const stim::Circuit& circuit, bool validate_capacity) {
    if (validate_capacity && circuit.count_qubits() > num_qubits) {
        throw std::invalid_argument(
            "The number of qubits in the circuit exceeds the maximum capacity of the simulator.");
    }
    for (const auto& op : circuit.operations) {
        if (op.gate_type == GateType::REPEAT) {
            uint64_t repeats = op.repeat_block_rep_count();
            const auto& block = op.repeat_block_body(circuit);
            for (uint64_t k = 0; k < repeats; k++) {
                do_circuit_internal(block, false);
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

void leaky::Simulator::sample_into(
    const stim::Circuit& circuit, size_t shots, uint8_t* results_begin_ptr, ReadoutStrategy readout_strategy) {
    if (circuit.count_qubits() > num_qubits) {
        throw std::invalid_argument(
            "The number of qubits in the circuit exceeds the maximum capacity of the simulator.");
    }
    const auto num_measurements = circuit.count_measurements();
    for (size_t shot = 0; shot < shots; shot++) {
        clear();
        do_circuit_internal(circuit, false);
        append_measurement_record_into(results_begin_ptr + shot * num_measurements, readout_strategy);
    }
}

void leaky::Simulator::append_measurement_record_into(uint8_t* record_begin_ptr, ReadoutStrategy readout_strategy) {
    const auto& tableau_record = tableau_simulator.measurement_record.storage;

    auto num_measurements = leakage_masks_record.size();
    if (tableau_record.size() != num_measurements) {
        throw std::runtime_error(
            "Internal error: leakage masks and Stim measurement record have different lengths.");
    }
    if (readout_strategy == ReadoutStrategy::RawLabel) {
        for (size_t i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            *(record_begin_ptr + i) = mask == 0 ? (uint8_t)tableau_record[i] : mask + 1;
        }
    } else if (readout_strategy == ReadoutStrategy::RandomLeakageProjection) {
        for (size_t i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            *(record_begin_ptr + i) =
                mask == 0 ? (uint8_t)tableau_record[i] : (leaky::rand_float(0.0, 1.0, leakage_rng) < 0.5 ? 0 : 1);
        }
    } else if (readout_strategy == ReadoutStrategy::DeterministicLeakageProjection) {
        for (size_t i = 0; i < num_measurements; i++) {
            uint8_t mask = leakage_masks_record[i];
            *(record_begin_ptr + i) = mask == 0 ? (uint8_t)tableau_record[i] : 1;
        }
    } else {
        throw std::invalid_argument("Invalid readout strategy.");
    }
}
