#ifndef LEAKY_CHANNEL_H
#define LEAKY_CHANNEL_H

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace leaky {

enum TransitionType : uint8_t {
    R,
    U,
    D,
    L,
};

TransitionType get_transition_type(uint8_t initial_status, uint8_t final_status);

std::string pauli_idx_to_string(uint8_t idx, bool is_single_qubit_channel);

typedef std::pair<uint8_t, uint8_t> transition;

struct LeakyPauliChannel {
    std::vector<uint8_t> initial_status_vec;
    std::vector<std::vector<transition>> transitions;
    std::vector<std::vector<double>> cumulative_probs;
    bool is_single_qubit_channel;

    explicit LeakyPauliChannel(bool is_single_qubit_transition = true);
    void add_transition(uint8_t initial_status, uint8_t final_status, uint8_t pauli_channel_idx, double probability);
    [[nodiscard]] double get_prob_from_to(uint8_t initial_status, uint8_t final_status, uint8_t pauli_idx) const;
    [[nodiscard]] uint8_t num_transitions() const;
    [[nodiscard]] std::optional<transition> sample(uint8_t initial_status) const;
    void safety_check() const;
    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string repr() const;
};

}  // namespace leaky

#endif  // LEAKY_CHANNEL_H