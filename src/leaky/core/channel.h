#ifndef LEAKY_CHANNEL_H
#define LEAKY_CHANNEL_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

#include "leaky/core/status.h"

namespace leaky {

enum TransitionType : uint8_t {
    R,
    U,
    D,
    L,
};

TransitionType get_transition_type(uint8_t from, uint8_t to);

struct Transition {
    LeakageStatus to_status;
    std::string pauli_operator;

    explicit Transition(LeakageStatus to_status, std::string_view pauli_operator);
};

struct WeightedTransition {
    Transition transition;
    double probability;

    explicit WeightedTransition(LeakageStatus to_status, std::string_view pauli_operator, double probability);
};

struct TransitionBucket {
    LeakageStatus initial_status;
    std::vector<WeightedTransition> weighted_transitions;
    mutable std::vector<double> cumulative_probs;
    mutable bool cumulative_probs_valid;

    explicit TransitionBucket(LeakageStatus initial_status);
    [[nodiscard]] double total_probability() const;
    void rebuild_cumulative_probs() const;
};

struct LeakyPauliChannel {
    std::vector<TransitionBucket> buckets;
    size_t num_qubits;

    explicit LeakyPauliChannel(size_t num_qubits = 1);
    void add_transition(LeakageStatus from, LeakageStatus to, std::string_view pauli_operator, double probability);
    [[nodiscard]] double get_prob_from_to(LeakageStatus from, LeakageStatus to, std::string_view pauli_operator) const;
    [[nodiscard]] size_t num_transitions() const;
    [[nodiscard]] std::optional<Transition> sample(LeakageStatus initial_status) const;
    [[nodiscard]] std::optional<Transition> sample(LeakageStatus initial_status, std::mt19937_64& rng) const;
    void safety_check() const;
    [[nodiscard]] std::string str() const;

   private:
    std::unordered_map<std::string, size_t> bucket_indices;

    static std::string encode_status(const LeakageStatus& status);
    [[nodiscard]] const TransitionBucket* find_bucket(const LeakageStatus& initial_status) const;
    TransitionBucket& get_or_create_bucket(const LeakageStatus& initial_status);
};

}  // namespace leaky

#endif  // LEAKY_CHANNEL_H
