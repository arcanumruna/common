#include <gtest/gtest.h>
#include <chrono>
#include "network/detail/RunningAvg.h"

using namespace aeron::concurrent::detail;
using namespace std::chrono;

TEST(RunningAvgTest, DefaultAvgIsOneMilli) {
    RunningAvg avg;
    EXPECT_EQ(avg.avg(), microseconds(1000));
}

TEST(RunningAvgTest, AddAndAvgReflectsValues) {
    RunningAvg avg;
    // Fill all 10 slots with 2 ms each
    for (int i = 0; i < RunningAvg::SLOT_COUNT; ++i) {
        avg.add(milliseconds(2));
    }
    EXPECT_EQ(avg.avg(), milliseconds(2));
}

TEST(RunningAvgTest, CircularOverwriteWorks) {
    RunningAvg avg;
    // Overwrite default 1ms slots with 4ms
    for (int i = 0; i < RunningAvg::SLOT_COUNT; ++i) {
        avg.add(milliseconds(4));
    }
    EXPECT_EQ(avg.avg(), milliseconds(4));

    // Overwrite half with 0ms; avg should drop to 2ms
    for (int i = 0; i < RunningAvg::SLOT_COUNT / 2; ++i) {
        avg.add(nanoseconds(0));
    }
    EXPECT_EQ(avg.avg(), milliseconds(2));
}

TEST(RunningAvgTest, DampenAppliedWhenAboveThreshold) {
    RunningAvg avg;
    // Fill all slots with 20 ms (well above the 10 ms threshold)
    for (int i = 0; i < RunningAvg::SLOT_COUNT; ++i) {
        avg.add(milliseconds(20));
    }
    const auto before = avg.avg();
    avg.dampenForLowResTicker();
    const auto after = avg.avg();
    EXPECT_LT(after, before);
    // Expect factor of approximately 0.9
    const double ratio = static_cast<double>(after.count()) / static_cast<double>(before.count());
    EXPECT_NEAR(ratio, RunningAvg::DAMPEN_FACTOR, 0.01);
}

TEST(RunningAvgTest, DampenNotAppliedWhenBelowThreshold) {
    RunningAvg avg;
    // Fill all slots with 1 ms (below the 10 ms threshold)
    for (int i = 0; i < RunningAvg::SLOT_COUNT; ++i) {
        avg.add(milliseconds(1));
    }
    const auto before = avg.avg();
    avg.dampenForLowResTicker();
    const auto after = avg.avg();
    EXPECT_EQ(after, before);
}
