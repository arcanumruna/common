#include <gtest/gtest.h>
#include <chrono>
#include "network/FrameSyncBackoffIdleStrategy.h"

using namespace aeron::concurrent;
using namespace std::chrono;

TEST(FrameSyncBackoffIdleStrategyTest, ResetIsNoOp) {
    FrameSyncBackoffIdleStrategy strategy(10);
    // Should not crash or change observable state
    strategy.reset();
    strategy.reset();
}

TEST(FrameSyncBackoffIdleStrategyTest, SetFpsUpdatesInterval) {
    FrameSyncBackoffIdleStrategy strategy(10);
    strategy.setFps(20);
    // Run a few frames and verify it completes faster than at 10 fps
    auto start = steady_clock::now();
    for (int i = 0; i < 5; ++i) {
        strategy.idle();
    }
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
    // 5 frames at 20 fps = 250 ms; allow generous tolerance
    EXPECT_LT(elapsed, 500);
}

TEST(FrameSyncBackoffIdleStrategyTest, SetFpsIgnoresNonPositive) {
    FrameSyncBackoffIdleStrategy strategy(10);
    strategy.setFps(0);   // must be ignored
    strategy.setFps(-5);  // must be ignored

    // Verify it still paces at 10 fps (5 frames ~ 500 ms)
    auto start = steady_clock::now();
    for (int i = 0; i < 5; ++i) {
        strategy.idle();
    }
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
    EXPECT_GE(elapsed, 350); // 5 * 100 ms = 500 ms, allow some slack
    EXPECT_LT(elapsed, 800);
}

TEST(FrameSyncBackoffIdleStrategyTest, BackoffReducesFpsOnNoWork) {
    FrameSyncBackoffIdleStrategy strategy(100); // 100 fps default

    // Call idle(0) multiple times to halve FPS: 100 -> 50 -> 25 -> ...
    // After enough calls the interval should be noticeably longer per frame.
    // We just verify it completes without hanging and doesn't crash.
    for (int i = 0; i < 10; ++i) {
        strategy.idle(0);
    }
}

TEST(FrameSyncBackoffIdleStrategyTest, BackoffRestoresOnWork) {
    FrameSyncBackoffIdleStrategy strategy(100);

    // Back off
    for (int i = 0; i < 8; ++i) {
        strategy.idle(0);
    }

    // First idle(1) restores fps to default but still drains the queued backoff frame.
    // The second idle(1) is the first frame at restored fps (~10 ms at 100 fps).
    strategy.idle(1); // may block up to ~1 s draining the backed-off frame

    auto start = steady_clock::now();
    strategy.idle(1);
    auto elapsed_restore = duration_cast<microseconds>(steady_clock::now() - start).count();

    // At 100 fps, frame interval is 10 ms = 10,000 µs. Allow generous 50 ms.
    EXPECT_LT(elapsed_restore, 50'000);
}

TEST(FrameSyncBackoffIdleStrategyTest, IdleNoArgsPacesCorrectly) {
    constexpr int fps = 20;
    FrameSyncBackoffIdleStrategy strategy(fps);
    constexpr int frames = 5;
    const auto expected_ms = 1000 / fps * frames; // 250 ms

    auto start = steady_clock::now();
    for (int i = 0; i < frames; ++i) {
        strategy.idle();
    }
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();

    EXPECT_GE(elapsed, expected_ms * 0.75);
    EXPECT_LT(elapsed, expected_ms * 2.0);
}
