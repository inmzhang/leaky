
#include "leaky/core/transition.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "leaky/core/rand_gen.h"

leaky::LeakyPauliChannel::LeakyPauliChannel(bool is_single_qubit_transition)
    : initial_status_vec(0),
      transitions(0),
      cumulative_probs(0),
      is_single_qubit_transition(is_single_qubit_transition) {
}

void leaky::LeakyPauliChannel::add_transition(
    uint8_t initial_status, uint8_t final_status, uint8_t pauli_channel_idx, double probability) {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), initial_status);
    if (it != initial_status_vec.end()) {
        auto idx = std::distance(initial_status_vec.begin(), it);
        transitions[idx].push_back(std::make_pair(final_status, pauli_channel_idx));
        auto &probs = cumulative_probs[idx];
        auto cum_prob = probs.back() + probability;
        assert(cum_prob - 1.0 < 1e-6);
        probs.push_back(cum_prob);
    } else {
        initial_status_vec.push_back(initial_status);
        transitions.push_back(std::vector<transition>{std::make_pair(final_status, pauli_channel_idx)});
        cumulative_probs.push_back(std::vector<double>{probability});
    }
}

leaky::transition leaky::LeakyPauliChannel::sample(uint8_t initial_status) const {
    auto it = std::find(initial_status_vec.begin(), initial_status_vec.end(), initial_status);
    assert(it != initial_status_vec.end());
    auto idx = std::distance(initial_status_vec.begin(), it);
    auto &probabilities = cumulative_probs[idx];
    auto rand_num = leaky::rand_float(0.0, 1.0);
    auto it2 = std::upper_bound(probabilities.begin(), probabilities.end(), rand_num);
    auto idx2 = std::distance(probabilities.begin(), it2);
    return transitions[idx][idx2];
}
