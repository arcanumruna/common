#pragma once

#include <chrono>
#include <thread>
#include <array>
#include <algorithm>

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

    class RunningAvg
    {
    public:
        static constexpr size_t SLOT_COUNT = 10;
        static constexpr auto DAMPEN_THRESHOLD = std::chrono::milliseconds(10);
        static constexpr float DAMPEN_FACTOR = 0.9f;

        RunningAvg()
        {
            values_.fill(std::chrono::microseconds(1000)); // 1ms default
        }

        void add(std::chrono::nanoseconds duration)
        {
            values_[index_++ % SLOT_COUNT] = duration;
            index_ %= SLOT_COUNT;
        }

        std::chrono::nanoseconds avg() const
        {
            std::chrono::nanoseconds sum(0);
            for (const auto& v : values_)
            {
                sum += v;
            }
            return sum / SLOT_COUNT;
        }

        void dampenForLowResTicker()
        {
            if (avg() > DAMPEN_THRESHOLD)
            {
                for (auto& v : values_)
                {
                    v = std::chrono::nanoseconds(static_cast<long long>(v.count() * DAMPEN_FACTOR));
                }
            }
        }

    private:
        std::array<std::chrono::nanoseconds, SLOT_COUNT> values_;
        size_t index_{0};
    };

    void initializeIfNeeded()
    {
        if (!m_initialized)
        {
            m_nextFrame = Clock::now();
            m_initialized = true;

#if defined(_WIN32)
            static std::thread([] {
                std::this_thread::sleep_for(std::chrono::hours(24 * 365 * 10));
            }).detach();
#endif
        }
    }

    int m_fps;
    std::chrono::nanoseconds m_frameIntervalNs;
    Clock::time_point m_nextFrame;
    bool m_initialized;

    RunningAvg m_sleepDurations;
    RunningAvg m_yieldDurations;
};

}} // namespace aeron::concurrent
