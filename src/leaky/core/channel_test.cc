#include "leaky/core/channel.h"

#include <vector>

#include "gtest/gtest.h"

using namespace leaky;

TEST(channel, transition_type) {
    ASSERT_EQ(get_transition_type(0, 0), TransitionType::R);
    ASSERT_EQ(get_transition_type(0, 1), TransitionType::U);
    ASSERT_EQ(get_transition_type(0, 2), TransitionType::U);
    ASSERT_EQ(get_transition_type(1, 0), TransitionType::D);
    ASSERT_EQ(get_transition_type(3, 0), TransitionType::D);
    ASSERT_EQ(get_transition_type(1, 3), TransitionType::L);
    ASSERT_EQ(get_transition_type(2, 3), TransitionType::L);
}

void add_1q_trans_helper(LeakyPauliChannel &channel, uint8_t from, uint8_t to, std::string_view pauli, double prob) {
    LeakageStatus from_status(1);
    LeakageStatus to_status(1);
    from_status.set(0, from);
    to_status.set(0, to);
    channel.add_transition(from_status, to_status, pauli, prob);
}

void assert_1q_prob_helper(
    const LeakyPauliChannel &channel, uint8_t from, uint8_t to, std::string_view pauli, double expected_prob) {
    LeakageStatus from_status(1);
    LeakageStatus to_status(1);
    from_status.set(0, from);
    to_status.set(0, to);
    ASSERT_FLOAT_EQ(channel.get_prob_from_to(from_status, to_status, pauli), expected_prob);
}

TEST(channel, add_transition_1q) {
    auto channel = LeakyPauliChannel(1);
    ASSERT_EQ(channel.num_qubits, 1);
    ASSERT_EQ(channel.num_transitions(), 0);
    add_1q_trans_helper(channel, 0, 0, "I", 0.2);
    add_1q_trans_helper(channel, 0, 0, "X", 0.3);
    add_1q_trans_helper(channel, 0, 0, "Y", 0.1);
    add_1q_trans_helper(channel, 0, 0, "Z", 0.2);
    add_1q_trans_helper(channel, 0, 1, "I", 0.15);
    add_1q_trans_helper(channel, 0, 2, "I", 0.05);
    add_1q_trans_helper(channel, 1, 0, "I", 0.5);
    add_1q_trans_helper(channel, 1, 1, "I", 0.3);
    add_1q_trans_helper(channel, 1, 2, "I", 0.2);
    ASSERT_EQ(channel.initial_status_vec.size(), 2);
    assert_1q_prob_helper(channel, 0, 0, "I", 0.2);
    assert_1q_prob_helper(channel, 0, 0, "X", 0.3);
    assert_1q_prob_helper(channel, 0, 1, "I", 0.15);
    assert_1q_prob_helper(channel, 0, 3, "I", 0.0);

    ASSERT_EQ(channel.str(), R"(Transitions:
    |C⟩ --I--> |C⟩: 0.2,
    |C⟩ --X--> |C⟩: 0.3,
    |C⟩ --Y--> |C⟩: 0.1,
    |C⟩ --Z--> |C⟩: 0.2,
    |C⟩ --I--> |2⟩: 0.15,
    |C⟩ --I--> |3⟩: 0.05,
    |2⟩ --I--> |C⟩: 0.5,
    |2⟩ --I--> |2⟩: 0.3,
    |2⟩ --I--> |3⟩: 0.2,
)");
}

TEST(channel, add_transition_2q) {
    auto channel = LeakyPauliChannel(2);
    LeakageStatus s1(2);
    LeakageStatus s2(2);
    LeakageStatus s3(2);
    s2.set(1, 1);
    s3.set(0, 1);
    channel.add_transition(s1, s1, "XY", 0.7);
    channel.add_transition(s1, s2, "ZI", 0.3);
    channel.add_transition(s2, s3, "II", 1.0);
    ASSERT_EQ(channel.initial_status_vec.size(), 2);
    ASSERT_FLOAT_EQ(channel.get_prob_from_to(s1, s1, "XY"), 0.7);
    ASSERT_FLOAT_EQ(channel.get_prob_from_to(s1, s2, "ZI"), 0.3);
    ASSERT_FLOAT_EQ(channel.get_prob_from_to(s2, s3, "II"), 1.0);
    ASSERT_EQ(channel.str(), R"(Transitions:
    |C⟩|C⟩ --XY--> |C⟩|C⟩: 0.7,
    |C⟩|C⟩ --ZI--> |C⟩|2⟩: 0.3,
    |C⟩|2⟩ --II--> |2⟩|C⟩: 1,
)");
}
