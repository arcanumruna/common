#pragma once

#include <charconv>
#include <string_view>
#include <stdexcept>


inline int fast_atoi(const char* s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (*s++ - '0');
    return val;
}

inline int16_t fast_atoi16(const char* s) { return static_cast<int16_t>(fast_atoi(s)); }


inline std::string_view floatToStringView(float value, char* buffer, size_t bufferSize) {
    auto [ptr, ec] = std::to_chars(buffer, buffer + bufferSize, value, std::chars_format::general);
    if (ec != std::errc()) {
        throw std::runtime_error("floatToStringView: conversion failed");
    }
    return std::string_view(buffer, ptr - buffer);
}

inline std::string floatToString(float f) {
    char buffer[32];
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), f, std::chars_format::general);

    if (ec != std::errc()) {
        throw std::runtime_error("floatToString: conversion failed");
    }

    // Create std::string on the heap with exact size
    return std::string(buffer, ptr);
}

inline uint64_t stringViewToUINT64(std::string_view sv) {
    uint64_t value;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

    if (ec == std::errc::invalid_argument) {
        throw std::runtime_error("Invalid input: not a valid number");
    } else if (ec == std::errc::result_out_of_range) {
        throw std::runtime_error("Number out of range for uint64_t");
    } else if (ec != std::errc()) {
        throw std::runtime_error("Unknown parsing error");
    }

    if (ptr != sv.data() + sv.size()) {
        throw std::runtime_error("Invalid input: trailing characters");
    }

    return value;
}

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86_FP)
#include <xmmintrin.h>
inline float fast_inv_sqrt(float x) {
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(x)));
}
#else
inline float fast_inv_sqrt(float x) {
    float xhalf = 0.5f * x;
    int32_t i;
    std::memcpy(&i, &x, sizeof(float));
    i = 0x5f375a86 - (i >> 1);
    std::memcpy(&x, &i, sizeof(float));
    x = x * (1.5f - xhalf * x * x);
    return x;
}
#endif

inline bool is_valid_float(float x) {
    int32_t bits;
    std::memcpy(&bits, &x, sizeof(float));
    return ((bits & 0x7F800000) != 0x7F800000);
}