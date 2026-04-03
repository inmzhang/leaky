#include "leaky/core/channel.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
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

leaky::WeightedTransition::WeightedTransition(
    LeakageStatus to_status, std::string_view pauli_operator, double probability)
    : transition(std::move(to_status), pauli_operator), probability(probability) {
}

leaky::TransitionBucket::TransitionBucket(LeakageStatus initial_status)
    : initial_status(std::move(initial_status)),
      weighted_transitions(),
      cumulative_probs(),
      probability_sum(0.0),
      cumulative_probs_valid(true) {
}

double leaky::TransitionBucket::total_probability() const {
    return probability_sum;
}

void leaky::TransitionBucket::rebuild_cumulative_probs() const {
    cumulative_probs.clear();
    cumulative_probs.reserve(weighted_transitions.size());
    double cumulative_probability = 0.0;
    for (const auto& weighted_transition : weighted_transitions) {
        cumulative_probability += weighted_transition.probability;
        cumulative_probs.push_back(cumulative_probability);
    }
    cumulative_probs_valid = true;
}

leaky::LeakyPauliChannel::LeakyPauliChannel(size_t num_qubits)
    : buckets(), num_qubits(num_qubits), bucket_indices() {
}

size_t leaky::StatusKeyHash::operator()(std::string_view key) const noexcept {
    return std::hash<std::string_view>{}(key);
}

size_t leaky::StatusKeyHash::operator()(const std::string& key) const noexcept {
    return (*this)(std::string_view(key));
}

bool leaky::StatusKeyEqual::operator()(std::string_view lhs, std::string_view rhs) const noexcept {
    return lhs == rhs;
}

bool leaky::StatusKeyEqual::operator()(const std::string& lhs, const std::string& rhs) const noexcept {
    return lhs == rhs;
}

bool leaky::StatusKeyEqual::operator()(const std::string& lhs, std::string_view rhs) const noexcept {
    return std::string_view(lhs) == rhs;
}

bool leaky::StatusKeyEqual::operator()(std::string_view lhs, const std::string& rhs) const noexcept {
    return lhs == std::string_view(rhs);
}

std::string leaky::LeakyPauliChannel::encode_status(const LeakageStatus& status) {
    return std::string(status.s.begin(), status.s.end());
}

std::string_view leaky::LeakyPauliChannel::encode_status_view(std::span<const uint8_t> status) {
    return {
        reinterpret_cast<const char*>(status.data()),
        status.size(),
    };
}

const leaky::TransitionBucket* leaky::LeakyPauliChannel::find_bucket(const LeakageStatus& initial_status) const {
    return find_bucket(encode_status_view(initial_status.s));
}

const leaky::TransitionBucket* leaky::LeakyPauliChannel::find_bucket(std::string_view encoded_initial_status) const {
    auto it = bucket_indices.find(encoded_initial_status);
    if (it == bucket_indices.end()) {
        return nullptr;
    }
    return &buckets[it->second];
}

leaky::TransitionBucket& leaky::LeakyPauliChannel::get_or_create_bucket(const LeakageStatus& initial_status) {
    auto key = encode_status(initial_status);
    auto it = bucket_indices.find(key);
    if (it != bucket_indices.end()) {
        return buckets[it->second];
    }

    auto bucket_index = buckets.size();
    buckets.emplace_back(initial_status);
    bucket_indices.emplace(std::move(key), bucket_index);
    return buckets.back();
}

void leaky::LeakyPauliChannel::add_transition(
    LeakageStatus from, LeakageStatus to, std::string_view pauli_operator, double probability) {
    if (num_qubits == 0) {
        throw std::invalid_argument("A leaky channel must act on at least one qubit.");
    }
    if (from.num_qubits != num_qubits || to.num_qubits != num_qubits || pauli_operator.length() != num_qubits) {
        throw std::invalid_argument(
            "Transition width must match the channel width and the Pauli operator length.");
    }
    if (!std::isfinite(probability) || probability < 0.0) {
        throw std::invalid_argument("Transition probability must be a finite non-negative number.");
    }

    auto& bucket = get_or_create_bucket(from);
    auto next_total_probability = bucket.probability_sum + probability;
    if (next_total_probability - 1.0 > 1e-6) {
        throw std::runtime_error(
            "sum of probabilities for each initial status should not exceed 1, but get " +
            std::to_string(next_total_probability));
    }

    auto existing_transition = std::find_if(
        bucket.weighted_transitions.begin(),
        bucket.weighted_transitions.end(),
        [&](const auto& weighted_transition) {
            return weighted_transition.transition.to_status == to &&
                   weighted_transition.transition.pauli_operator == pauli_operator;
        });

    if (existing_transition != bucket.weighted_transitions.end()) {
        existing_transition->probability += probability;
    } else {
        bucket.weighted_transitions.emplace_back(to, pauli_operator, probability);
    }
    bucket.probability_sum = next_total_probability;
    bucket.cumulative_probs_valid = false;
}

double leaky::LeakyPauliChannel::get_prob_from_to(
    LeakageStatus from, LeakageStatus to, std::string_view pauli_operator) const {
    if (from.num_qubits != num_qubits || to.num_qubits != num_qubits || pauli_operator.length() != num_qubits) {
        throw std::invalid_argument(
            "Transition width must match the channel width and the Pauli operator length.");
    }

    auto bucket = find_bucket(from);
    if (bucket == nullptr) {
        return 0.0;
    }

    for (const auto& weighted_transition : bucket->weighted_transitions) {
        if (weighted_transition.transition.to_status == to &&
            weighted_transition.transition.pauli_operator == pauli_operator) {
            return weighted_transition.probability;
        }
    }
    return 0.0;
}

std::optional<leaky::Transition> leaky::LeakyPauliChannel::sample(LeakageStatus initial_status) const {
    return sample(std::move(initial_status), leaky::global_urng());
}

std::optional<leaky::Transition> leaky::LeakyPauliChannel::sample(
    LeakageStatus initial_status, std::mt19937_64& rng) const {
    if (initial_status.num_qubits != num_qubits) {
        throw std::invalid_argument("Initial status width must match the channel width.");
    }

    auto weighted_transition = sample_weighted_transition(initial_status.s, rng);
    if (weighted_transition == nullptr) {
        return std::nullopt;
    }
    return weighted_transition->transition;
}

const leaky::WeightedTransition* leaky::LeakyPauliChannel::sample_weighted_transition(
    std::span<const uint8_t> encoded_initial_status, std::mt19937_64& rng) const {
    if (encoded_initial_status.size() != num_qubits) {
        throw std::invalid_argument("Initial status width must match the channel width.");
    }

    auto bucket = find_bucket(encode_status_view(encoded_initial_status));
    if (bucket == nullptr || bucket->weighted_transitions.empty()) {
        return nullptr;
    }
    if (!bucket->cumulative_probs_valid) {
        bucket->rebuild_cumulative_probs();
    }

    if (bucket->weighted_transitions.size() == 1) {
        return &bucket->weighted_transitions.front();
    }

    auto rand_num = leaky::rand_float(0.0, bucket->probability_sum, rng);
    auto it = std::lower_bound(bucket->cumulative_probs.begin(), bucket->cumulative_probs.end(), rand_num);
    auto idx = std::distance(bucket->cumulative_probs.begin(), it);
    return &bucket->weighted_transitions[idx];
}

/// Do safety check for the channel
/// Check if the sum of probabilities for each initial status is 1
/// Check if the attached pauli of transitions for the qubits in D/U/L is I
void leaky::LeakyPauliChannel::safety_check() const {
    for (const auto& bucket : buckets) {
        if (std::fabs(bucket.probability_sum - 1.0) > 1e-6) {
            throw std::runtime_error(
                "The sum of probabilities for each initial status should be 1, but get " +
                std::to_string(bucket.probability_sum));
        }
        for (const auto& weighted_transition : bucket.weighted_transitions) {
            for (size_t i = 0; i < bucket.initial_status.num_qubits; i++) {
                uint8_t from = bucket.initial_status.s[i];
                uint8_t to = weighted_transition.transition.to_status.s[i];
                auto transition_type = leaky::get_transition_type(from, to);
                char pauli = weighted_transition.transition.pauli_operator[i];
                if (transition_type != leaky::TransitionType::R && pauli != 'I') {
                    throw std::runtime_error("The attached pauli of transitions for the qubits in D/U/L should be I");
                }
            }
        }
    }
}

size_t leaky::LeakyPauliChannel::num_transitions() const {
    size_t count = 0;
    for (const auto& bucket : buckets) {
        count += bucket.weighted_transitions.size();
    }
    return count;
}

std::string leaky::LeakyPauliChannel::str() const {
    std::stringstream out;
    out << "Transitions:\n";
    for (const auto& bucket : buckets) {
        std::string from_status_str = bucket.initial_status.str();
        for (const auto& weighted_transition : bucket.weighted_transitions) {
            auto to_status_str = weighted_transition.transition.to_status.str();
            out << "    " << from_status_str << " --" << weighted_transition.transition.pauli_operator << "--> "
                << to_status_str << ": " << weighted_transition.probability << ",\n";
        }
    }
    if (buckets.empty()) {
        out << "   None\n";
    }
    return out.str();
}
