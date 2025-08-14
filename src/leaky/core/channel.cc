#include "leaky/core/channel.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "leaky/core/rand_gen.h"
#include "leaky/core/status.h"

leaky::TransitionType leaky::get_transition_type(uint8_t from, uint8_t to) {
    if (from == 0 && to == 0) {
        return leaky::TransitionType::R;
    } else if (from == 0 && to > 0) {
        return leaky::TransitionType::U;
    } else if (from > 0 && to == 0) {
        return leaky::TransitionType::D;
    } else {
        return leaky::TransitionType::L;
    }
}

leaky::Transition::Transition(LeakageStatus to_status, std::string_view pauli_operator)
    : to_status(std::move(to_status)), pauli_operator(pauli_operator) {
}

leaky::LeakyPauliChannel::LeakyPauliChannel(size_t num_qubits)
    : initial_status_vec(0), transitions(0), cumulative_probs(0), num_qubits(num_qubits) {
}

void leaky::LeakyPauliChannel::add_transition(
    LeakageStatus from, LeakageStatus to, std::string_view pauli_operator, double probability) {
    if ((from.num_qubits != to.num_qubits) || (from.num_qubits != pauli_operator.length())) {
        throw std::invalid_argument(
            "The number of qubits in `from` and `to` status should both be equal to the length of the pauli operator.");
    }

    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), from);
    if (it != initial_status_vec.end()) {
        auto idx = std::distance(initial_status_vec.begin(), it);
        transitions[idx].emplace_back(to, pauli_operator);
        auto &probs = cumulative_probs[idx];
        auto cum_prob = probs.back() + probability;
        if (cum_prob - 1.0 > 1e-6) {
            std::string error_msg =
                "sum of probabilities for each initial status should not exceed 1, but get " + std::to_string(cum_prob);
            throw std::runtime_error(error_msg);
        }
        probs.push_back(cum_prob);
    } else {
        initial_status_vec.push_back(from);
        transitions.push_back(std::vector<Transition>{Transition{to, pauli_operator}});
        cumulative_probs.push_back(std::vector<double>{probability});
    }
}

double leaky::LeakyPauliChannel::get_prob_from_to(
    LeakageStatus from, LeakageStatus to, std::string_view pauli_operator) const {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), from);
    if (it == initial_status_vec.end()) {
        return 0.0;
    }
    auto idx = std::distance(initial_status_vec.begin(), it);
    auto &transitions_vec = transitions[idx];
    auto &probs = cumulative_probs[idx];
    auto it2 = std::find_if(transitions_vec.begin(), transitions_vec.end(), [to, pauli_operator](auto &trans) {
        return trans.to_status == to && trans.pauli_operator == pauli_operator;
    });
    if (it2 == transitions_vec.end()) {
        return 0.0;
    }
    auto idx2 = std::distance(transitions_vec.begin(), it2);
    auto prob = idx2 == 0 ? probs[idx2] : probs[idx2] - probs[idx2 - 1];
    return prob;
}

std::optional<leaky::Transition> leaky::LeakyPauliChannel::sample(LeakageStatus initial_status) const {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), initial_status);
    if (it == initial_status_vec.end()) {
        return std::nullopt;
    }
    auto idx = std::distance(initial_status_vec.begin(), it);
    auto &probabilities = cumulative_probs[idx];
    auto rand_num = leaky::rand_float(0.0, probabilities.back());
    auto it2 = std::upper_bound(probabilities.begin(), probabilities.end(), rand_num);
    auto idx2 = std::distance(probabilities.begin(), it2);
    return {transitions[idx][idx2]};
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
            throw std::runtime_error(
                "The sum of probabilities for each initial status should be 1, but get " +
                std::to_string(probs.back()));
        }
        for (const auto &trans : transitions_from_initial) {
            for (auto i = 0; i < initial_status.num_qubits; i++) {
                uint8_t from = initial_status.get(i);
                uint8_t to = trans.to_status.get(i);
                auto transition_type = leaky::get_transition_type(from, to);
                char pauli = trans.pauli_operator[i];
                if (transition_type != leaky::TransitionType::R && pauli != 'I') {
                    throw std::runtime_error("The attached pauli of transitions for the qubits in D/U/L should be I");
                }
            }
        }
    }
}

size_t leaky::LeakyPauliChannel::num_transitions() const {
    size_t count = 0;
    for (size_t i = 0; i < initial_status_vec.size(); i++) {
        count += transitions[i].size();
    }
    return count;
}

std::string leaky::LeakyPauliChannel::str() const {
    std::stringstream out;
    out << "Transitions:\n";
    for (size_t i = 0; i < initial_status_vec.size(); i++) {
        const auto &from_status = initial_status_vec[i];
        std::string from_status_str = from_status.str();
        for (size_t j = 0; j < transitions[i].size(); j++) {
            auto prob = j == 0 ? cumulative_probs[i][j] : cumulative_probs[i][j] - cumulative_probs[i][j - 1];
            const auto &trans = transitions[i][j];
            auto to_status_str = trans.to_status.str();
            out << "    " << from_status_str << " --" << trans.pauli_operator << "--> " << to_status_str << ": " << prob
                << ",\n";
        }
    }
    if (initial_status_vec.empty()) {
        out << "   None\n";
    }
    return out.str();
}
