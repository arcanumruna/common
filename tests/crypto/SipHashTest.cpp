#include <gtest/gtest.h>
#include <cstring>

#include "crypto/SipHash.h"

// SipHash-2-4 128-bit known-answer vector (veorq/SipHash vectors_sip128[15]):
// key = 00..0f, input = 00..0e (15 bytes).
TEST(SipHash, ReferenceVector15) {
    uint8_t key[16]; for (int i = 0; i < 16; ++i) key[i] = static_cast<uint8_t>(i);
    uint8_t in[15];  for (int i = 0; i < 15; ++i) in[i]  = static_cast<uint8_t>(i);
    auto tag = crypto::sipHash24(key, in, sizeof(in));
    const uint8_t exp[16] = {0xa3,0x81,0x7f,0x04,0xba,0x25,0xa8,0xe6,
                             0x6d,0xf6,0x72,0x14,0xc7,0x55,0x02,0x93};
    EXPECT_EQ(0, std::memcmp(tag.data(), exp, 16));
}

TEST(SipHash, Deterministic) {
    uint8_t key[16]; for (int i = 0; i < 16; ++i) key[i] = static_cast<uint8_t>(i * 7 + 1);
    const uint8_t msg[] = {'h','e','l','l','o'};
    EXPECT_EQ(crypto::sipHash24(key, msg, sizeof(msg)), crypto::sipHash24(key, msg, sizeof(msg)));
}

TEST(SipHash, KeyAffectsOutput) {
    uint8_t k1[16]{}; uint8_t k2[16]{}; k2[0] = 1;
    const uint8_t msg[] = {1,2,3,4,5,6,7,8,9};
    EXPECT_NE(crypto::sipHash24(k1, msg, sizeof(msg)), crypto::sipHash24(k2, msg, sizeof(msg)));
}

TEST(SipHash, InputAffectsOutput) {
    uint8_t key[16]{};
    const uint8_t a[] = {1,2,3,4};
    const uint8_t b[] = {1,2,3,5};
    EXPECT_NE(crypto::sipHash24(key, a, sizeof(a)), crypto::sipHash24(key, b, sizeof(b)));
}
