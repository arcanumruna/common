#pragma once

#include <chrono>
#include <cstdint>

struct FastRNG {
    uint32_t state = 0;

    uint32_t next() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        if (state == 0) {
            auto now = std::chrono::steady_clock::now().time_since_epoch();
            state = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
            if (state == 0) state = 1;
        }
        return state * 0x2545F4914F6CDD1Dull;
    }

    uint32_t next(uint32_t min, uint32_t max) {
        if (min >= max) return min;
        uint32_t range = max - min;
        return min + (next() % range);
    }

    float nextFloat() {
        return static_cast<float>(next() >> 8) / static_cast<float>(1 << 24);
    }

    uint32_t rollD6() {
        return (next() % 6) + 1;
    }
};
