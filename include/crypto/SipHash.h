#pragma once

// SipHash-2-4, 128-bit output. Header-only, allocation-free.
// Vendored from the public-domain reference implementation by
// Jean-Philippe Aumasson and Daniel J. Bernstein (https://github.com/veorq/SipHash).
// A fast keyed MAC for short messages (e.g. party-invite tokens).

#include <array>
#include <cstddef>
#include <cstdint>

namespace crypto {

namespace detail {

inline uint64_t rotl(uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

inline uint64_t load64le(const uint8_t* p) {
    return  static_cast<uint64_t>(p[0])
         | (static_cast<uint64_t>(p[1]) << 8)
         | (static_cast<uint64_t>(p[2]) << 16)
         | (static_cast<uint64_t>(p[3]) << 24)
         | (static_cast<uint64_t>(p[4]) << 32)
         | (static_cast<uint64_t>(p[5]) << 40)
         | (static_cast<uint64_t>(p[6]) << 48)
         | (static_cast<uint64_t>(p[7]) << 56);
}

inline void store64le(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

inline void sipround(uint64_t& v0, uint64_t& v1, uint64_t& v2, uint64_t& v3) {
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
}

} // namespace detail

// SipHash-2-4 with a 16-byte (128-bit) tag. `key` is 16 bytes.
inline std::array<uint8_t, 16> sipHash24(const uint8_t key[16], const uint8_t* data, size_t len) {
    constexpr int cROUNDS = 2;
    constexpr int dROUNDS = 4;

    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;

    const uint64_t k0 = detail::load64le(key);
    const uint64_t k1 = detail::load64le(key + 8);

    v3 ^= k1; v2 ^= k0; v1 ^= k1; v0 ^= k0;
    v1 ^= 0xee; // 128-bit output

    const size_t left = len & 7;
    const uint8_t* end = data + (len - left);
    for (const uint8_t* p = data; p != end; p += 8) {
        const uint64_t m = detail::load64le(p);
        v3 ^= m;
        for (int i = 0; i < cROUNDS; ++i) detail::sipround(v0, v1, v2, v3);
        v0 ^= m;
    }

    uint64_t b = static_cast<uint64_t>(len) << 56;
    for (size_t i = 0; i < left; ++i) {
        b |= static_cast<uint64_t>(end[i]) << (8 * i);
    }

    v3 ^= b;
    for (int i = 0; i < cROUNDS; ++i) detail::sipround(v0, v1, v2, v3);
    v0 ^= b;

    v2 ^= 0xee; // 128-bit output
    for (int i = 0; i < dROUNDS; ++i) detail::sipround(v0, v1, v2, v3);

    std::array<uint8_t, 16> out{};
    detail::store64le(out.data(), v0 ^ v1 ^ v2 ^ v3);

    v1 ^= 0xdd;
    for (int i = 0; i < dROUNDS; ++i) detail::sipround(v0, v1, v2, v3);
    detail::store64le(out.data() + 8, v0 ^ v1 ^ v2 ^ v3);

    return out;
}

} // namespace crypto
