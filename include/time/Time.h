#pragma once

#include <chrono>
#include <cstdint>

// All functions use system_clock (UTC, Unix epoch).
// Call once per tick at the coarsest entry point, derive finer units by division.

inline uint32_t getCurrentTimeSeconds() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

inline uint64_t getCurrentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline uint64_t getCurrentTimeMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
