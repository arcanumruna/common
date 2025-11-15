#pragma once

#include <cstdint>

struct IdGenerator {
    std::uint32_t currentId32 = 1;
    std::uint16_t currentId16 = 1;

    [[nodiscard]] std::uint32_t next() noexcept {
        return currentId32++;
    }

    [[nodiscard]] std::uint16_t next16() noexcept {
        return currentId16++;
    }

    void reset(std::uint32_t start32 = 1) noexcept {
        currentId32 = start32;
    }

    void reset16(std::uint16_t start16 = 1) noexcept {
        currentId16 = start16;
    }

    void resetAll(std::uint32_t start32 = 1, std::uint16_t start16 = 1) noexcept {
        currentId32 = start32;
        currentId16 = start16;
    }

    [[nodiscard]] std::uint32_t current() const noexcept {
        return currentId32;
    }

    [[nodiscard]] std::uint16_t current16() const noexcept {
        return currentId16;
    }
};