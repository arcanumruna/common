#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

#include "detail/RunningAvg.h"

namespace aeron { namespace concurrent {

class FrameSyncIdleStrategy
{
public:
    explicit FrameSyncIdleStrategy(int fps)
        : m_fps(fps),
          m_frameIntervalNs(std::chrono::nanoseconds(1'000'000'000 / fps)),
          m_nextFrame(std::chrono::steady_clock::now()),
          m_initialized(false)
    {
    }

    inline void idle()
    {
        initializeIfNeeded();

        const auto now = Clock::now();

        // Adaptive sleep phase
        for (auto t0 = now, t1 = Clock::now();
             (m_nextFrame - t0) > m_sleepDurations.avg();
             t0 = t1)
        {
            auto timeRemaining = m_nextFrame - t0;
            auto sleepTime = std::max<std::chrono::nanoseconds>(std::chrono::microseconds(1), timeRemaining / 8);
            std::this_thread::sleep_for(sleepTime);
            t1 = Clock::now();
            m_sleepDurations.add(t1 - t0);
        }

        m_sleepDurations.dampenForLowResTicker();

        // Yield phase
        for (auto t0 = Clock::now(), t1 = Clock::now();
             (m_nextFrame - t0) > m_yieldDurations.avg();
             t0 = t1)
        {
            std::this_thread::yield();
            t1 = Clock::now();
            m_yieldDurations.add(t1 - t0);
        }

        m_nextFrame = std::max(m_nextFrame + m_frameIntervalNs, Clock::now());
    }

    inline void idle(int /*workCount*/)
    {
        idle();
    }

    inline void reset()
    {
        // Intentionally no-op
    }

private:
    using Clock = std::chrono::steady_clock;

    void initializeIfNeeded()
    {
        if (!m_initialized)
        {
            m_nextFrame = Clock::now();
            m_initialized = true;

#if defined(_WIN32)
            // Spawn a long-sleeping thread so the OS keeps its timer resolution
            // elevated for the lifetime of the process.
            static std::thread s_timerResolutionThread([] {
                std::this_thread::sleep_for(std::chrono::hours(24 * 365 * 10));
            });
            static bool s_detached = (s_timerResolutionThread.detach(), true);
            (void)s_detached;
#endif
        }
    }

    int m_fps;
    std::chrono::nanoseconds m_frameIntervalNs;
    Clock::time_point m_nextFrame;
    bool m_initialized;

    detail::RunningAvg m_sleepDurations;
    detail::RunningAvg m_yieldDurations;
};

}} // namespace aeron::concurrent
