#ifndef LEAKY_CHANNEL_H
#define LEAKY_CHANNEL_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <sys/types.h>
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
    std::string_view pauli_operator;

    explicit Transition(LeakageStatus to_status, std::string_view pauli_operator);
};

struct LeakyPauliChannel {
    std::vector<LeakageStatus> initial_status_vec;
    std::vector<std::vector<Transition>> transitions;
    std::vector<std::vector<double>> cumulative_probs;
    size_t num_qubits;

    explicit LeakyPauliChannel(size_t num_qubits = 1);
    void add_transition(LeakageStatus from, LeakageStatus to, std::string_view pauli_operator, double probability);
    [[nodiscard]] double get_prob_from_to(LeakageStatus from, LeakageStatus to, std::string_view pauli_operator) const;
    [[nodiscard]] size_t num_transitions() const;
    [[nodiscard]] std::optional<Transition> sample(LeakageStatus initial_status) const;
    void safety_check() const;
    [[nodiscard]] std::string str() const;
};

}  // namespace leaky

#endif  // LEAKY_CHANNEL_H
