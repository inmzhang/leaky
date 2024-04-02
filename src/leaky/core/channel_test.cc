#include "leaky/core/channel.h"

#include <algorithm>
#include <optional>
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

TEST(channel, add_transition_1q) {
    auto channel = LeakyPauliChannel();
    ASSERT_TRUE(channel.is_single_qubit_channel);
    channel.add_transition(0, 0, 0, 0.2);
    channel.add_transition(0, 0, 1, 0.3);
    channel.add_transition(0, 0, 2, 0.1);
    channel.add_transition(0, 0, 3, 0.2);
    channel.add_transition(0, 1, 0, 0.15);
    channel.add_transition(0, 2, 0, 0.05);
    channel.add_transition(1, 0, 0, 0.5);
    channel.add_transition(1, 1, 0, 0.3);
    channel.add_transition(1, 2, 0, 0.2);
    ASSERT_EQ(channel.initial_status_vec.size(), 2);
    ASSERT_FLOAT_EQ(channel.get_transitions_from_to(0, 0).value().second, 0.2);
    ASSERT_FLOAT_EQ(channel.get_transitions_from_to(0, 1).value().second, 0.95);
    ASSERT_EQ(channel.get_transitions_from_to(0, 3), std::nullopt);
    ASSERT_EQ(channel.str(), R"(Transitions:
    |C> --I--> |C>: 0.2,
    |C> --X--> |C>: 0.3,
    |C> --Y--> |C>: 0.1,
    |C> --Z--> |C>: 0.2,
    |C> --I--> |2>: 0.15,
    |C> --I--> |3>: 0.05,
    |2> --I--> |C>: 0.5,
    |2> --I--> |2>: 0.3,
    |2> --I--> |3>: 0.2,
)");
}

TEST(channel, add_transition_2q) {
    auto channel = LeakyPauliChannel(false);
    ASSERT_FALSE(channel.is_single_qubit_channel);
    channel.add_transition(0x00, 0x00, 6, 1.0);
    channel.add_transition(0x01, 0x10, 0, 1.0);
    ASSERT_EQ(channel.initial_status_vec.size(), 2);
    ASSERT_FLOAT_EQ(channel.get_transitions_from_to(0x00, 0x00).value().second, 1.0);
    ASSERT_FLOAT_EQ(channel.get_transitions_from_to(0x01, 0x10).value().second, 1.0);
    ASSERT_EQ(channel.str(), R"(Transitions:
    |C>|C> --XY--> |C>|C>: 1,
    |C>|2> --II--> |2>|C>: 1,
)");
}

TEST(channel, safety_check) {
    auto channel = LeakyPauliChannel();
    channel.add_transition(0, 0, 0, 0.2);
    channel.add_transition(0, 0, 1, 0.3);
    EXPECT_THROW(channel.safety_check(), std::runtime_error);

    auto channel2 = LeakyPauliChannel();
    channel2.add_transition(0, 0, 2, 0.5);
    channel2.add_transition(0, 1, 2, 0.5);
    EXPECT_THROW(channel2.safety_check(), std::runtime_error);

    auto channel3 = LeakyPauliChannel(false);
    channel3.add_transition(0x00, 0x00, 6, 0.5);
    channel3.add_transition(0x00, 0x00, 2, 0.49);
    EXPECT_THROW(channel3.safety_check(), std::runtime_error);

    auto channel4 = LeakyPauliChannel(false);
    channel4.add_transition(0x00, 0x10, 6, 1.0);
    EXPECT_THROW(channel4.safety_check(), std::runtime_error);
}

TEST(channel, sample) {
    auto channel = LeakyPauliChannel();
    channel.add_transition(0, 0, 0, 0.25);
    channel.add_transition(0, 0, 1, 0.25);
    channel.add_transition(0, 0, 2, 0.25);
    channel.add_transition(0, 0, 3, 0.25);
    std::vector<uint8_t> final_status_vec;
    std::vector<uint8_t> pauli_idx_vec;
    for (size_t i = 0; i < 1000; i++) {
        auto transition = channel.sample(0);
        final_status_vec.push_back(transition.first);
        pauli_idx_vec.push_back(transition.second);
    }
    ASSERT_TRUE(std::all_of(final_status_vec.begin(), final_status_vec.end(), [](uint8_t x) {
        return x == 0;
    }));
    for (auto i = 0; i < 4; i++) {
        auto count = std::count(pauli_idx_vec.begin(), pauli_idx_vec.end(), i);
        ASSERT_TRUE(count > 200);
        ASSERT_TRUE(count < 300);
    }
}