#pragma once

#include <cstdint>

struct IdGenerator {
    // BUG-L6 fix: Changed from uint32_t to uint64_t to prevent overflow on long-running servers
    std::uint64_t currentId64 = 1;
    std::uint16_t currentId16 = 1;

    [[nodiscard]] std::uint64_t next() noexcept {
        return currentId64++;
    }

    [[nodiscard]] std::uint16_t next16() noexcept {
        return currentId16++;
    }

    void reset(std::uint64_t start64 = 1) noexcept {
        currentId64 = start64;
    }

    void reset16(std::uint16_t start16 = 1) noexcept {
        currentId16 = start16;
    }

    void resetAll(std::uint64_t start64 = 1, std::uint16_t start16 = 1) noexcept {
        currentId64 = start64;
        currentId16 = start16;
    }

    [[nodiscard]] std::uint64_t current() const noexcept {
        return currentId64;
    }

    [[nodiscard]] std::uint16_t current16() const noexcept {
        return currentId16;
    }
};
