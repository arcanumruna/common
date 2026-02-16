#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <sys/resource.h>
#include "network/FrameSyncIdleStrategy.h"

long getCpuTimeMicros() {
    struct rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec * 1'000'000 + usage.ru_utime.tv_usec +
           usage.ru_stime.tv_sec * 1'000'000 + usage.ru_stime.tv_usec;
}

TEST(FrameSyncIdleStrategyTest, MaintainsFrameRateAccuracy) {
    constexpr int fps = 10;
    constexpr int testDurationSec = 5;
    constexpr auto frameInterval = std::chrono::nanoseconds(1'000'000'000 / fps);
    constexpr int expectedFrames = fps * testDurationSec;

    aeron::concurrent::FrameSyncIdleStrategy idleStrategy(fps);

    std::vector<std::chrono::steady_clock::time_point> timestamps;
    timestamps.reserve(expectedFrames + 1);

    timestamps.push_back(std::chrono::steady_clock::now());

    long startCpu = getCpuTimeMicros();
    auto startWall = std::chrono::steady_clock::now();
    for (int i = 0; i < expectedFrames; ++i) {
        idleStrategy.idle();
        timestamps.push_back(std::chrono::steady_clock::now());
    }
    long endCpu = getCpuTimeMicros();
    auto endWall = std::chrono::steady_clock::now();
    auto wallDuration = std::chrono::duration_cast<std::chrono::microseconds>(endWall - startWall).count();
    long cpuUsedMicros = endCpu - startCpu;

    double cpuUtilization = (static_cast<double>(cpuUsedMicros) / wallDuration) * 100.0;

    std::cout << "CPU Utilization: " << cpuUtilization << "%\n";

    EXPECT_LT(cpuUtilization, 5.0);

    std::vector<std::chrono::nanoseconds> intervals;
    for (size_t i = 1; i < timestamps.size(); ++i) {
        intervals.push_back(timestamps[i] - timestamps[i - 1]);
    }

    auto totalDuration = std::accumulate(intervals.begin(), intervals.end(), std::chrono::nanoseconds{0});
    auto averageInterval = totalDuration / intervals.size();

    auto errorNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        averageInterval > frameInterval ? averageInterval - frameInterval : frameInterval - averageInterval
    );

    std::cout << "Target FPS: " << fps
              << ", Target Interval (ns): " << frameInterval.count()
              << ", Average Interval (ns): " << averageInterval.count()
              << ", Error (ns): " << errorNs.count() << std::endl;

    // Allow a tolerance of ±5% of frameInterval
    EXPECT_LT(errorNs.count(), frameInterval.count() * 0.05);
}

TEST(FrameSyncIdleStrategyTest, IdleWithWorkCountDelegatesToIdle) {
    // idle(workCount) is documented as equivalent to idle() for FrameSyncIdleStrategy.
    // Verify it completes a small number of frames at the expected rate.
    constexpr int fps = 20;
    aeron::concurrent::FrameSyncIdleStrategy strategy(fps);

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 3; ++i) {
        strategy.idle(0);  // workCount ignored
        strategy.idle(5);
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // 6 frames at 20 fps = 300 ms; allow generous tolerance
    EXPECT_GE(elapsed, 200);
    EXPECT_LT(elapsed, 600);
}

TEST(FrameSyncIdleStrategyTest, ResetIsNoOp) {
    aeron::concurrent::FrameSyncIdleStrategy strategy(60);
    strategy.reset(); // must not crash or alter pacing
    strategy.idle();  // must still work normally after reset
    strategy.reset();
}
