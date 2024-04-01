#include "leaky/core/channel.h"

#include "gtest/gtest.h"

using namespace leaky;

// TEST(channel, transition_type) {
//     ASSERT_EQ(get_transition_type(0, 0), TransitionType::R);
//     ASSERT_EQ(get_transition_type(0, 1), TransitionType::U);
//     ASSERT_EQ(get_transition_type(0, 2), TransitionType::U);
//     ASSERT_EQ(get_transition_type(1, 0), TransitionType::D);
//     ASSERT_EQ(get_transition_type(3, 0), TransitionType::D);
//     ASSERT_EQ(get_transition_type(1, 3), TransitionType::L);
//     ASSERT_EQ(get_transition_type(2, 3), TransitionType::L);
// }

TEST(channel, add_transition) {
    auto channel = LeakyPauliChannel();
    ASSERT_TRUE(channel.is_single_qubit_transition);
    ASSERT_EQ(channel.num_transitions(), 0);
}