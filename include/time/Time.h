#pragma once

#include <chrono>
#include <cstdint>

// All three functions use steady_clock (monotonic, never adjusted by NTP).
// Use getCurrentTimeMicros() for elapsed-time calculations requiring microsecond precision.
// If wall-clock (Unix epoch) timestamps are needed, use std::chrono::system_clock directly.

inline uint32_t getCurrentTimeSeconds() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto secs = duration_cast<seconds>(now.time_since_epoch()).count();
    return static_cast<uint32_t>(secs);
}

inline uint64_t getCurrentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

inline uint64_t getCurrentTimeMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}
