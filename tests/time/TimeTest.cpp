#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "time/Time.h"

TEST(TimeTest, GetCurrentTimeSecondsReturnsNonZero) {
    EXPECT_GT(getCurrentTimeSeconds(), 0u);
}

TEST(TimeTest, GetCurrentTimeMillisReturnsNonZero) {
    EXPECT_GT(getCurrentTimeMillis(), 0ull);
}

TEST(TimeTest, GetCurrentTimeMicrosReturnsNonZero) {
    EXPECT_GT(getCurrentTimeMicros(), 0ull);
}

TEST(TimeTest, TimeMillisIsMonotonic) {
    uint64_t t1 = getCurrentTimeMillis();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    uint64_t t2 = getCurrentTimeMillis();
    EXPECT_GE(t2, t1);
}

TEST(TimeTest, TimeMicrosIsMonotonic) {
    uint64_t t1 = getCurrentTimeMicros();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    uint64_t t2 = getCurrentTimeMicros();
    EXPECT_GE(t2, t1);
}

TEST(TimeTest, MicrosResolutionFinerThanMillis) {
    // Both use steady_clock; micros > millis for the same epoch
    uint64_t millis = getCurrentTimeMillis();
    uint64_t micros = getCurrentTimeMicros();
    // micros should be at least millis * 1000 (within a small delta)
    EXPECT_GE(micros, millis * 1000 - 1000); // 1 ms tolerance
}

TEST(TimeTest, ElapsedMillisIsReasonable) {
    auto t1 = getCurrentTimeMillis();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto t2 = getCurrentTimeMillis();
    uint64_t elapsed = t2 - t1;
    EXPECT_GE(elapsed, 8ull);  // at least 8 ms
    EXPECT_LE(elapsed, 100ull); // not more than 100 ms
}
