#ifndef LEAKY_CHANNEL_H
#define LEAKY_CHANNEL_H

#include <cstdint>
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

typedef std::pair<uint8_t, uint8_t> transition;

struct LeakyPauliChannel {
    std::vector<uint8_t> initial_status_vec;
    std::vector<std::vector<transition>> transitions;
    std::vector<std::vector<double>> cumulative_probs;
    bool is_single_qubit_transition;

    LeakyPauliChannel(bool is_single_qubit_transition = true);
    uint32_t num_transitions() const;
    void add_transition(uint8_t initial_status, uint8_t final_status, uint8_t pauli_channel_idx, double probability);
    transition sample(uint8_t initial_status) const;
};
}  // namespace leaky

#endif  // LEAKY_CHANNEL_H