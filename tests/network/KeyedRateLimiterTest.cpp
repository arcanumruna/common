#include <gtest/gtest.h>
#include <cctype>
#include <string>
#include "network/KeyedRateLimiter.h"

using namespace std::chrono;

namespace {
struct FakeClock {
    RateLimiter::TimePoint t{seconds(1000)};
    RateLimiter::NowFn fn() { return [this] { return t; }; }
    void advance(milliseconds d) { t += d; }
};
using Limiter = KeyedRateLimiter<std::string>;
}

TEST(KeyedRateLimiterTest, KeysAreIndependent) {
    FakeClock c;
    Limiter l(2, seconds(60), 100, seconds(60), c.fn());
    EXPECT_TRUE(l.tryAcquire("a"));
    EXPECT_TRUE(l.tryAcquire("a"));
    EXPECT_FALSE(l.tryAcquire("a"));
    EXPECT_TRUE(l.tryAcquire("b"));
}

TEST(KeyedRateLimiterTest, TwentiethAllowedTwentyFirstDenied) {
    FakeClock c;
    Limiter l(20, seconds(60), 100, seconds(60), c.fn());
    for (int i = 1; i <= 20; ++i) EXPECT_TRUE(l.tryAcquire("k")) << i;
    EXPECT_FALSE(l.tryAcquire("k"));
}

TEST(KeyedRateLimiterTest, TokenReturnsAfterExpectedTime) {
    FakeClock c;
    Limiter l(20, seconds(60), 100, seconds(60), c.fn());
    for (int i = 0; i < 20; ++i) ASSERT_TRUE(l.tryAcquire("k"));
    EXPECT_EQ(l.millisUntilNextToken("k"), 3000);   // 60s / 20
    c.advance(milliseconds(2999));
    EXPECT_FALSE(l.tryAcquire("k"));
    c.advance(milliseconds(1));
    EXPECT_TRUE(l.tryAcquire("k"));
    EXPECT_FALSE(l.tryAcquire("k"));
}

TEST(KeyedRateLimiterTest, HasTokenDoesNotConsumeOrInsert) {
    FakeClock c;
    Limiter l(1, seconds(60), 100, seconds(60), c.fn());
    EXPECT_TRUE(l.hasToken("new"));
    EXPECT_EQ(l.size(), 0u);
    EXPECT_EQ(l.millisUntilNextToken("new"), 0);
    EXPECT_EQ(l.size(), 0u);
    EXPECT_TRUE(l.tryAcquire("x"));
    EXPECT_FALSE(l.hasToken("x"));
    c.advance(seconds(60));
    EXPECT_TRUE(l.hasToken("x"));
    EXPECT_TRUE(l.hasToken("x"));      // still not consumed
    EXPECT_TRUE(l.tryAcquire("x"));
}

TEST(KeyedRateLimiterTest, LockForIsExactly60Seconds) {
    FakeClock c;
    Limiter l(20, seconds(60), 100, seconds(60), c.fn());
    l.lockFor("k", seconds(60));
    EXPECT_TRUE(l.isLocked("k"));
    EXPECT_FALSE(l.tryAcquire("k"));
    EXPECT_EQ(l.millisUntilNextToken("k"), 60000);
    c.advance(milliseconds(59999));
    EXPECT_TRUE(l.isLocked("k"));
    EXPECT_FALSE(l.tryAcquire("k"));
    c.advance(milliseconds(1));
    EXPECT_FALSE(l.isLocked("k"));
    EXPECT_TRUE(l.tryAcquire("k"));
    EXPECT_FALSE(l.isLocked("other"));
}

TEST(KeyedRateLimiterTest, IdleKeysPrunedAndSizeBounded) {
    FakeClock c;
    Limiter l(5, seconds(60), 100000, seconds(60), c.fn());
    for (int i = 0; i < 1000; ++i) {
        (void)l.tryAcquire("k" + std::to_string(i));
        c.advance(seconds(1));
    }
    // Amortized auto-prune keeps only roughly the last idleEvict(+interval) worth of keys.
    EXPECT_LE(l.size(), 80u);
    c.advance(seconds(200));
    l.prune();
    EXPECT_EQ(l.size(), 0u);
}

TEST(KeyedRateLimiterTest, NonFullOrLockedKeysAreNotPruned) {
    FakeClock c;
    Limiter l(1, seconds(600), 100, seconds(60), c.fn());
    (void)l.tryAcquire("drained");              // bucket not full for 600s
    l.lockFor("locked", seconds(300));
    c.advance(seconds(120));
    l.prune();
    EXPECT_EQ(l.size(), 2u);
    EXPECT_FALSE(l.tryAcquire("drained"));
    c.advance(seconds(600));
    l.prune();
    EXPECT_EQ(l.size(), 0u);
}

TEST(KeyedRateLimiterTest, CapRejectsNewKeysAndCounts) {
    FakeClock c;
    Limiter l(5, seconds(60), 3, seconds(60), c.fn());
    EXPECT_TRUE(l.tryAcquire("a"));
    EXPECT_TRUE(l.tryAcquire("b"));
    EXPECT_TRUE(l.tryAcquire("c"));
    EXPECT_FALSE(l.tryAcquire("d"));
    EXPECT_FALSE(l.tryAcquire("e"));
    EXPECT_EQ(l.rejectedNewKeys(), 2u);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_TRUE(l.tryAcquire("a"));       // existing keys keep working
    // After the old keys go idle, prune frees room and new keys are admitted.
    c.advance(seconds(120));
    EXPECT_TRUE(l.tryAcquire("d"));
    EXPECT_EQ(l.rejectedNewKeys(), 2u);
}

namespace {
// Fill a 3-key limiter (maxEvents=5, idleEvict=60s) with a, b, c at the current fake time.
Limiter fullCap(FakeClock& c) {
    Limiter l(5, seconds(60), 3, seconds(60), c.fn());
    (void)l.tryAcquire("a"); (void)l.tryAcquire("b"); (void)l.tryAcquire("c");
    return l;
}
}

TEST(KeyedRateLimiterTest, HasTokenFalseForUnknownKeyAtCap) {
    FakeClock c;
    Limiter l = fullCap(c);
    EXPECT_FALSE(l.hasToken("new"));
    EXPECT_EQ(l.size(), 3u);
    EXPECT_TRUE(l.hasToken("a"));          // existing keys unaffected
}

TEST(KeyedRateLimiterTest, TakeOnNewKeyAtCapFailsAndCounts) {
    FakeClock c;
    Limiter l = fullCap(c);
    EXPECT_FALSE(l.take("new"));
    EXPECT_EQ(l.rejectedNewKeys(), 1u);
    EXPECT_EQ(l.size(), 3u);
    EXPECT_TRUE(l.take("a"));
}

TEST(KeyedRateLimiterTest, LockForAlwaysSucceedsAtCapAndStaysBounded) {
    FakeClock c;
    Limiter l = fullCap(c);
    l.lockFor("victim", seconds(60));
    EXPECT_TRUE(l.isLocked("victim"));
    EXPECT_FALSE(l.tryAcquire("victim"));
    EXPECT_EQ(l.size(), 3u);
    c.advance(milliseconds(59999));
    EXPECT_TRUE(l.isLocked("victim"));
    c.advance(milliseconds(1));
    EXPECT_FALSE(l.isLocked("victim"));
}

TEST(KeyedRateLimiterTest, LockSurvivesFullCapFloodAndPrune) {
    FakeClock c;
    Limiter l(5, seconds(60), 4, seconds(60), c.fn());
    l.lockFor("victim", seconds(600));
    for (int i = 0; i < 1000; ++i) {
        (void)l.tryAcquire("f" + std::to_string(i));       // distinct-key flood
        if (i % 50 == 0) l.lockFor("n" + std::to_string(i), seconds(30));
        c.advance(milliseconds(100));
        ASSERT_LE(l.size(), 4u);
    }
    EXPECT_TRUE(l.isLocked("victim"));
    l.prune();
    EXPECT_TRUE(l.isLocked("victim"));
}

TEST(KeyedRateLimiterTest, AllLockedStillAcceptsNewLockAndBounded) {
    FakeClock c;
    Limiter l(5, seconds(60), 2, seconds(60), c.fn());
    l.lockFor("a", seconds(100));
    l.lockFor("b", seconds(200));
    l.lockFor("c", seconds(300));      // evicts the soonest-expiring lock (a)
    EXPECT_EQ(l.size(), 2u);
    EXPECT_TRUE(l.isLocked("c"));
    EXPECT_TRUE(l.isLocked("b"));
}

TEST(KeyedRateLimiterTest, CapPruneIsThrottled) {
    FakeClock c;
    Limiter l(5, seconds(60), 3, seconds(60), c.fn());
    (void)l.tryAcquire("a"); (void)l.tryAcquire("b"); (void)l.tryAcquire("c");
    c.advance(seconds(20));            // past the 15 s periodic/throttle interval
    (void)l.tryAcquire("x1");                // pins the cap-time prune; rejected or admitted after prune
    const auto scans = l.pruneScans();
    for (int i = 0; i < 10000; ++i) (void)l.tryAcquire("flood" + std::to_string(i));
    EXPECT_EQ(l.pruneScans(), scans);  // same instant: no further scans
    c.advance(seconds(16));
    (void)l.tryAcquire("y");
    EXPECT_LE(l.pruneScans(), scans + 1);
}

TEST(KeyedRateLimiterTest, LockNeverShortenedAndNonPositiveIsNoop) {
    FakeClock c;
    Limiter l(5, seconds(60), 10, seconds(60), c.fn());
    l.lockFor("k", seconds(60));
    l.lockFor("k", seconds(10));
    c.advance(seconds(30));
    EXPECT_TRUE(l.isLocked("k"));
    l.lockFor("z", milliseconds(0));
    l.lockFor("z", milliseconds(-5));
    EXPECT_FALSE(l.isLocked("z"));
    EXPECT_EQ(l.size(), 1u);
    l.lockFor("big", milliseconds::max());   // no overflow
    EXPECT_TRUE(l.isLocked("big"));
}

TEST(KeyedRateLimiterTest, FiveFailuresThenLockFlow) {
    FakeClock c;
    Limiter l(5, seconds(60), 100, seconds(60), c.fn());
    int fails = 0;
    for (int i = 0; i < 5; ++i) { ASSERT_TRUE(l.tryAcquire("bob")); ++fails; }
    if (fails >= 5) l.lockFor("bob", seconds(60));
    EXPECT_TRUE(l.isLocked("bob"));
    c.advance(seconds(60));
    EXPECT_FALSE(l.isLocked("bob"));
}

TEST(KeyedRateLimiterTest, DegenerateConfigsFailClosed) {
    FakeClock c;
    Limiter zeroKeys(5, seconds(60), 0, seconds(60), c.fn());
    EXPECT_FALSE(zeroKeys.tryAcquire("a"));
    EXPECT_FALSE(zeroKeys.hasToken("a"));
    zeroKeys.lockFor("a", seconds(1));
    EXPECT_EQ(zeroKeys.size(), 0u);
    Limiter zeroEvents(0, seconds(60), 10, seconds(60), c.fn());
    EXPECT_FALSE(zeroEvents.tryAcquire("a"));
}

namespace {
struct CaseHash { size_t operator()(const std::string& s) const {
    size_t h = 0; for (char ch : s) h = h * 31 + std::tolower(static_cast<unsigned char>(ch)); return h; } };
struct CaseEq { bool operator()(const std::string& a, const std::string& b) const {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true; } };
}

TEST(KeyedRateLimiterTest, CustomHashAndEq) {
    FakeClock c;
    KeyedRateLimiter<std::string, CaseHash, CaseEq> l(1, seconds(60), 10, seconds(60), c.fn());
    EXPECT_TRUE(l.tryAcquire("Bob"));
    EXPECT_FALSE(l.tryAcquire("bOB"));
    EXPECT_EQ(l.size(), 1u);
}

TEST(KeyedRateLimiterTest, HasTokenThenTakeRecoversAfterFlood) {
    FakeClock c;
    Limiter l = fullCap(c);
    EXPECT_FALSE(l.hasToken("new"));               // flooded, prune not due
    c.advance(seconds(60));                        // old keys idle past idleEvict
    ASSERT_TRUE(l.hasToken("new"));                // prune due => optimistic
    EXPECT_TRUE(l.take("new"));                    // take prunes, admits
    EXPECT_EQ(l.rejectedNewKeys(), 0u);
    EXPECT_LE(l.size(), 3u);
}

TEST(KeyedRateLimiterTest, HasTokenOptimismStillFailsClosedInTake) {
    FakeClock c;
    Limiter busy(5, seconds(60), 3, seconds(600), c.fn());
    for (const char* k : {"a", "b", "c"}) EXPECT_TRUE(busy.tryAcquire(k));
    c.advance(seconds(10));
    EXPECT_TRUE(busy.hasToken("new"));             // due (interval 150s capped at 1s)
    EXPECT_FALSE(busy.take("new"));                // prune finds nothing idle => reject
    EXPECT_EQ(busy.rejectedNewKeys(), 1u);
    EXPECT_FALSE(busy.hasToken("new"));            // throttled again until next interval
}

TEST(RateLimiterInjectedClockTest, BehavesLikeRealClock) {
    FakeClock c;
    RateLimiter l(2, milliseconds(1000), {}, 0, c.fn());
    EXPECT_EQ(l.allow(0), 0);
    EXPECT_EQ(l.allow(0), 0);
    EXPECT_EQ(l.allow(0), -1);
    c.advance(milliseconds(500));
    EXPECT_EQ(l.allow(0), 0);             // 2/s => one token per 500 ms
    EXPECT_EQ(l.allow(0), -1);   // success reset the counter
    EXPECT_FALSE(l.isInactive());
    c.advance(seconds(60));
    EXPECT_TRUE(l.isInactive());

    auto w = RateLimiter::overWindow(20, seconds(60), c.fn());
    for (int i = 0; i < 20; ++i) { ASSERT_TRUE(w.hasToken()); w.take(); }
    EXPECT_FALSE(w.hasToken());
    EXPECT_EQ(w.millisUntilNextToken(), 3000);
    c.advance(milliseconds(3000));
    EXPECT_TRUE(w.hasToken());
}
