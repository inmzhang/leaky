
#include "leaky/core/channel.h"

#include <algorithm>
#include <array>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "leaky/core/rand_gen.h"

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

leaky::TransitionType leaky::get_transition_type(uint8_t initial_status, uint8_t final_status) {
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

std::string leaky::pauli_idx_to_string(uint8_t idx, bool is_single_qubit_channel) {
    if (is_single_qubit_channel) {
        return PAULI_1Q[idx];
    }
    return PAULI_2Q[idx];
}

leaky::LeakyPauliChannel::LeakyPauliChannel(bool is_single_qubit_transition)
    : initial_status_vec(0), transitions(0), cumulative_probs(0), is_single_qubit_channel(is_single_qubit_transition) {
}

void leaky::LeakyPauliChannel::add_transition(
    uint8_t initial_status, uint8_t final_status, uint8_t pauli_channel_idx, double probability) {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), initial_status);
    if (it != initial_status_vec.end()) {
        auto idx = std::distance(initial_status_vec.begin(), it);
        transitions[idx].emplace_back(std::make_pair(final_status, pauli_channel_idx));
        auto &probs = cumulative_probs[idx];
        auto cum_prob = probs.back() + probability;
        if (cum_prob > 1.0) {
            throw std::runtime_error("The sum of probabilities for each initial status should not exceed 1!");
        }
        probs.push_back(cum_prob);
    } else {
        initial_status_vec.push_back(initial_status);
        transitions.push_back(std::vector<transition>{std::make_pair(final_status, pauli_channel_idx)});
        cumulative_probs.push_back(std::vector<double>{probability});
    }
}

std::optional<std::pair<leaky::transition, double>> leaky::LeakyPauliChannel::get_transitions_from_to(
    uint8_t initial_status, uint8_t final_status) const {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), initial_status);
    if (it == initial_status_vec.end()) {
        return std::nullopt;
    }
    auto idx = std::distance(initial_status_vec.begin(), it);
    auto &transitions_from_initial = transitions[idx];
    auto &probs = cumulative_probs[idx];
    auto it2 =
        std::find_if(transitions_from_initial.begin(), transitions_from_initial.end(), [final_status](auto &trans) {
            return trans.first == final_status;
        });
    if (it2 == transitions_from_initial.end()) {
        return std::nullopt;
    }
    auto idx2 = std::distance(transitions_from_initial.begin(), it2);
    return {std::make_pair(*it2, probs[idx2])};
}

leaky::transition leaky::LeakyPauliChannel::sample(uint8_t initial_status) const {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), initial_status);
    auto idx = std::distance(initial_status_vec.begin(), it);
    auto &probabilities = cumulative_probs[idx];
    auto rand_num = leaky::rand_float(0.0, 1.0);
    auto it2 = std::upper_bound(probabilities.begin(), probabilities.end(), rand_num);
    auto idx2 = std::distance(probabilities.begin(), it2);
    return transitions[idx][idx2];
}

/// Do safety check for the channel
/// Check if the sum of probabilities for each initial status is 1
/// Check if the attached pauli of transitions for the qubits in D/U/L is I
void leaky::LeakyPauliChannel::safety_check() const {
    for (size_t i = 0; i < initial_status_vec.size(); i++) {
        auto initial_status = initial_status_vec[i];
        auto &transitions_from_initial = transitions[i];
        auto &probs = cumulative_probs[i];
        if (std::fabs(probs.back() - 1.0) > 1e-6) {
            throw std::runtime_error("The sum of probabilities for each initial status should be 1");
        }
        for (auto [final_status, pauli_channel_idx] : transitions_from_initial) {
            if (is_single_qubit_channel) {
                auto transition_type = leaky::get_transition_type(initial_status, final_status);
                if (transition_type != leaky::TransitionType::R && pauli_channel_idx != 0) {
                    throw std::runtime_error("The attached pauli of transitions for the qubits in D/U/L should be I");
                }
                continue;
            }
            auto i1 = initial_status >> 4;
            auto i2 = initial_status & 0x0F;
            auto f1 = final_status >> 4;
            auto f2 = final_status & 0x0F;
            auto transition_type1 = leaky::get_transition_type(i1, f1);
            auto transition_type2 = leaky::get_transition_type(i2, f2);
            if (transition_type1 != leaky::TransitionType::R && pauli_channel_idx >> 2 != 0) {
                throw std::runtime_error("The attached pauli of transitions for the qubits in D/U/L should be I");
            }
            if (transition_type2 != leaky::TransitionType::R && (pauli_channel_idx & 0x03) != 0) {
                throw std::runtime_error("The attached pauli of transitions for the qubits in D/U/L should be I");
            }
        }
    }
}

std::string leakage_status_to_string(uint8_t status) {
    if (status == 0) {
        return "|C>";
    } else {
        std::stringstream s;
        s << "|" << static_cast<int>(status + 1) << ">";
        return s.str();
    }
}

std::string initial_status_to_string(uint8_t initial_status, bool is_single_qubit_transition) {
    return is_single_qubit_transition
               ? leakage_status_to_string(initial_status)
               : leakage_status_to_string(initial_status >> 4) + leakage_status_to_string(initial_status & 0x0F);
}

uint8_t leaky::LeakyPauliChannel::num_transitions() const {
    uint8_t count = 0;
    for (size_t i = 0; i < initial_status_vec.size(); i++) {
        count += transitions[i].size();
    }
    return count;
}

std::string leaky::LeakyPauliChannel::str() const {
    std::stringstream out;
    out << "Transitions:\n";
    for (size_t i = 0; i < initial_status_vec.size(); i++) {
        auto initial_status = initial_status_vec[i];
        std::string initial_status_str = initial_status_to_string(initial_status, is_single_qubit_channel);
        for (size_t j = 0; j < transitions[i].size(); j++) {
            auto prob = j == 0 ? cumulative_probs[i][j] : cumulative_probs[i][j] - cumulative_probs[i][j - 1];
            auto [final_status, pauli_channel_idx] = transitions[i][j];
            auto final_status_str = initial_status_to_string(final_status, is_single_qubit_channel);
            auto pauli_str = leaky::pauli_idx_to_string(pauli_channel_idx, is_single_qubit_channel);
            out << "    " << initial_status_str << " --" << pauli_str << "--> " << final_status_str << ": " << prob
                << ",\n";
        }
    }
    if (initial_status_vec.empty()) {
        out << "   None\n";
    }
    return out.str();
}

std::string leaky::LeakyPauliChannel::repr() const {
    std::stringstream out;
    out << "LeakyPauliChannel(is_single_qubit_channel=" << std::boolalpha << is_single_qubit_channel << ", with "
        << unsigned(num_transitions()) << " transitions attached)\n";
    return out.str();
}
