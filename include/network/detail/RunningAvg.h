#pragma once

#include <array>
#include <chrono>

namespace aeron { namespace concurrent { namespace detail {

// Tracks the running average of the last SLOT_COUNT duration samples.
// Used by FrameSyncIdleStrategy and FrameSyncBackoffIdleStrategy to
// adaptively estimate sleep and yield overhead.
class RunningAvg
{
public:
    static constexpr size_t SLOT_COUNT        = 10;
    static constexpr auto   DAMPEN_THRESHOLD  = std::chrono::milliseconds(10);
    static constexpr float  DAMPEN_FACTOR     = 0.9f;

    RunningAvg()
    {
        values_.fill(std::chrono::microseconds(1000)); // 1 ms default
    }

    void add(std::chrono::nanoseconds duration)
    {
        values_[index_] = duration;
        index_ = (index_ + 1) % SLOT_COUNT;
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

}}} // namespace aeron::concurrent::detail
