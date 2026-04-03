#ifndef LEAKY_CHANNEL_H
#define LEAKY_CHANNEL_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
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
    double probability_sum;
    mutable bool cumulative_probs_valid;

    explicit TransitionBucket(LeakageStatus initial_status);
    [[nodiscard]] double total_probability() const;
    void rebuild_cumulative_probs() const;
};

struct StatusKeyHash {
    using is_transparent = void;

    [[nodiscard]] size_t operator()(std::string_view key) const noexcept;
    [[nodiscard]] size_t operator()(const std::string& key) const noexcept;
};

struct StatusKeyEqual {
    using is_transparent = void;

    [[nodiscard]] bool operator()(std::string_view lhs, std::string_view rhs) const noexcept;
    [[nodiscard]] bool operator()(const std::string& lhs, const std::string& rhs) const noexcept;
    [[nodiscard]] bool operator()(const std::string& lhs, std::string_view rhs) const noexcept;
    [[nodiscard]] bool operator()(std::string_view lhs, const std::string& rhs) const noexcept;
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
    [[nodiscard]] const WeightedTransition* sample_weighted_transition(
        std::span<const uint8_t> encoded_initial_status, std::mt19937_64& rng) const;
    void safety_check() const;
    [[nodiscard]] std::string str() const;

   private:
    std::unordered_map<std::string, size_t, StatusKeyHash, StatusKeyEqual> bucket_indices;

    static std::string encode_status(const LeakageStatus& status);
    static std::string_view encode_status_view(std::span<const uint8_t> status);
    [[nodiscard]] const TransitionBucket* find_bucket(const LeakageStatus& initial_status) const;
    [[nodiscard]] const TransitionBucket* find_bucket(std::string_view encoded_initial_status) const;
    TransitionBucket& get_or_create_bucket(const LeakageStatus& initial_status);
};

}  // namespace leaky

#endif  // LEAKY_CHANNEL_H
