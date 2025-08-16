#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "network/RateLimiter.h"

using namespace std::chrono;

// Helper to sleep for convenience
inline void sleepMs(std::uint64_t ms) {
    std::this_thread::sleep_for(milliseconds(ms));
}

TEST(RateLimiterTest, AllowsUpToRateLimit) {
    RateLimiter limiter(5, milliseconds(1000)); // 5 messages/sec, 1s window

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(limiter.allow(0), 0);
    }
}

TEST(RateLimiterTest, FirstViolationDetected) {
    RateLimiter limiter(1, milliseconds(1000)); // 1 message/sec
    EXPECT_EQ(limiter.allow(0), 0);  // allowed
    EXPECT_EQ(limiter.allow(0), -1); // first violation
}

TEST(RateLimiterTest, SecondViolationDetected) {
    RateLimiter limiter(1, milliseconds(1000));
    limiter.allow(0);  // allowed
    limiter.allow(0);  // -1
    EXPECT_EQ(limiter.allow(0), -2);
}

TEST(RateLimiterTest, FinalViolationDetected) {
    RateLimiter limiter(1, milliseconds(1000));
    limiter.allow(0);  // allowed
    limiter.allow(0);  // -1
    limiter.allow(0);  // -2
    EXPECT_EQ(limiter.allow(0), -3);
}

TEST(RateLimiterTest, ResetsViolationAfterWindow) {
    RateLimiter limiter(1, milliseconds(100)); // 100ms window
    limiter.allow(0);  // allowed
    limiter.allow(0);  // -1
    sleepMs(200);      // 200ms
    EXPECT_EQ(limiter.allow(0), -1);  // new window starts
}

TEST(RateLimiterTest, ResetsTokensAfterInterval) {
    RateLimiter limiter(1, milliseconds(1000));
    EXPECT_EQ(limiter.allow(0), 0);  // token used
    EXPECT_NE(limiter.allow(0), 0);  // violation
    sleepMs(1000);
    EXPECT_EQ(limiter.allow(0), 0);  // token refilled
}

TEST(RateLimiterTest, IgnoresWhitelistedMessages) {
    RateLimiter limiter(1, milliseconds(1000), { 15 });
    EXPECT_EQ(limiter.allow(15), 0);
    EXPECT_EQ(limiter.allow(15), 0);
    EXPECT_EQ(limiter.allow(15), 0);
}

TEST(RateLimiterTest, DetectsInactivityCorrectly) {
    RateLimiter limiter(1, milliseconds(1000));
    EXPECT_FALSE(limiter.isInactive());
    sleepMs(65000);
    EXPECT_TRUE(limiter.isInactive());
}
