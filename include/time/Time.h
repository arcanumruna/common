#pragma once

inline uint32_t getCurrentTimeSeconds() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto secs = duration_cast<seconds>(now.time_since_epoch()).count();
    return static_cast<uint32_t>(secs);
}
