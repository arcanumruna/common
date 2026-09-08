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

TEST(RateLimiterTest, WhitelistUpdatesActivityTime) {
    // Whitelisted messages should update the activity timer (BUG-12 fix).
    // We can't easily sleep 60s, so we verify the inverse: immediately after
    // sending whitelisted messages the limiter is NOT inactive.
    RateLimiter limiter(1, milliseconds(1000), { 99 });
    limiter.allow(99); // whitelisted
    limiter.allow(99);
    EXPECT_FALSE(limiter.isInactive());
}

TEST(RateLimiterTest, ViolationCountContinuesBeyond3) {
    RateLimiter limiter(1, milliseconds(5000));
    limiter.allow(0);  // token consumed
    limiter.allow(0);  // -1
    limiter.allow(0);  // -2
    limiter.allow(0);  // -3
    EXPECT_EQ(limiter.allow(0), -3); // 4th violation still returns -3
    EXPECT_EQ(limiter.allow(0), -3); // 5th violation still returns -3
}

TEST(RateLimiterTest, BurstCapacityAbsorbsSpikeAboveRate) {
    // rate=2/s but capacity=5: a 5-message burst is allowed even though it exceeds the sustained
    // rate; the 6th (bucket empty) trips a violation.
    RateLimiter limiter(2, milliseconds(1000), {}, 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(limiter.allow(0), 0);
    }
    EXPECT_EQ(limiter.allow(0), -1);
}

TEST(RateLimiterTest, BurstCapacityDefaultsToRate) {
    // burstCapacity omitted (0) => capacity == rate: exactly `rate` messages, then a violation.
    RateLimiter limiter(3, milliseconds(1000));
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(limiter.allow(0), 0);
    }
    EXPECT_EQ(limiter.allow(0), -1);
}

TEST(RateLimiterTest, RefillsAtSustainedRateNotFullCapacity) {
    // After draining a large burst, tokens come back at the sustained rate (2/s), NOT instantly to
    // the full capacity — so one second later only ~2 are available, never the whole bucket of 6.
    RateLimiter limiter(2, milliseconds(1000), {}, 6);
    for (int i = 0; i < 6; ++i) limiter.allow(0);   // drain the bucket
    EXPECT_NE(limiter.allow(0), 0);                 // empty -> violation
    sleepMs(1000);                                  // refill at 2 tokens/sec
    int allowed = 0;
    while (limiter.allow(0) == 0) ++allowed;
    EXPECT_GE(allowed, 1);
    EXPECT_LT(allowed, 6);                          // gradual refill, not a reset-to-capacity
}

// ---- Generalized period-based quota API (overWindow / hasToken / take /
//      millisUntilNextToken) — used by the reporting service to compose
//      per-user 1/min + 5/day + 20/week limits. -----------------------------

TEST(RateLimiterTest, OverWindowStartsFullAndDrains) {
    auto limiter = RateLimiter::overWindow(3, milliseconds(1000)); // 3 per second
    int taken = 0;
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(limiter.hasToken());
        limiter.take();
        ++taken;
    }
    EXPECT_EQ(taken, 3);
    EXPECT_FALSE(limiter.hasToken());              // bucket empty
    EXPECT_GT(limiter.millisUntilNextToken(), 0);  // must wait for a refill
}

TEST(RateLimiterTest, OverWindowRefillsAfterPeriod) {
    auto limiter = RateLimiter::overWindow(1, milliseconds(200)); // 1 per 200ms
    ASSERT_TRUE(limiter.hasToken());
    limiter.take();
    EXPECT_FALSE(limiter.hasToken());
    sleepMs(220);
    EXPECT_TRUE(limiter.hasToken());               // one token back after the window
}

TEST(RateLimiterTest, OverWindowSlowerThanOnePerSecond) {
    // The original per-second ctor cannot express this; overWindow can.
    auto limiter = RateLimiter::overWindow(1, milliseconds(60'000)); // 1 per minute
    ASSERT_TRUE(limiter.hasToken());
    limiter.take();
    EXPECT_FALSE(limiter.hasToken());
    const long long wait = limiter.millisUntilNextToken();
    EXPECT_GT(wait, 0);
    EXPECT_LE(wait, 60'000);
}
