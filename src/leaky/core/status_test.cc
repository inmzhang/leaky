#include "leaky/core/status.h"

#include "gtest/gtest.h"

using namespace leaky;

TEST(status, out_of_range_access_throws) {
    LeakageStatus status(1);

    ASSERT_THROW(status.get(1), std::out_of_range);
    ASSERT_THROW(status.set(1, 1), std::out_of_range);
    ASSERT_THROW(status.reset(1), std::out_of_range);
    ASSERT_THROW(status.is_leaked(1), std::out_of_range);
}
