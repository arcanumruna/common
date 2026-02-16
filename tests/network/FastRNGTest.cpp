#include <gtest/gtest.h>
#include "network/FastRNG.h"

TEST(FastRNGTest, ZeroStateTriggersClockreSeed) {
    FastRNG rng; // state = 0 by default
    // First call must not return 0 (the clock-reseed fallback ensures a non-zero state)
    const uint32_t val = rng.next();
    // state is no longer 0 after the call
    EXPECT_NE(rng.state, 0u);
    (void)val;
}

TEST(FastRNGTest, StateChangesOnEachCall) {
    FastRNG rng;
    rng.next(); // trigger initial seed
    uint32_t prev = rng.state;
    bool changed = false;
    for (int i = 0; i < 100; ++i) {
        rng.next();
        if (rng.state != prev) { changed = true; break; }
        prev = rng.state;
    }
    EXPECT_TRUE(changed);
}

TEST(FastRNGTest, NextRangeRespectsBounds) {
    FastRNG rng;
    rng.next(); // seed
    for (int i = 0; i < 1000; ++i) {
        uint32_t v = rng.next(5, 10);
        EXPECT_GE(v, 5u);
        EXPECT_LT(v, 10u);
    }
}

TEST(FastRNGTest, NextRangeMinEqualsMaxReturnsMin) {
    FastRNG rng;
    rng.next();
    EXPECT_EQ(rng.next(7, 7), 7u);
}

TEST(FastRNGTest, NextRangeMinGreaterThanMaxReturnsMin) {
    FastRNG rng;
    rng.next();
    EXPECT_EQ(rng.next(10, 5), 10u);
}

TEST(FastRNGTest, NextFloatInZeroToOne) {
    FastRNG rng;
    rng.next(); // seed
    for (int i = 0; i < 1000; ++i) {
        float v = rng.nextFloat();
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }
}

TEST(FastRNGTest, RollD6InRange1To6) {
    FastRNG rng;
    rng.next(); // seed
    bool seen[7] = {};
    for (int i = 0; i < 1000; ++i) {
        uint32_t v = rng.rollD6();
        EXPECT_GE(v, 1u);
        EXPECT_LE(v, 6u);
        if (v >= 1 && v <= 6) seen[v] = true;
    }
    // All faces should appear with 1000 rolls
    for (int face = 1; face <= 6; ++face) {
        EXPECT_TRUE(seen[face]) << "Face " << face << " never appeared";
    }
}
