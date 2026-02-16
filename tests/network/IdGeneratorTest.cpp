#include <gtest/gtest.h>
#include "network/IdGenerator.h"

TEST(IdGeneratorTest, Next32StartsAt1AndIncrements) {
    IdGenerator gen;
    EXPECT_EQ(gen.next(), 1u);
    EXPECT_EQ(gen.next(), 2u);
    EXPECT_EQ(gen.next(), 3u);
}

TEST(IdGeneratorTest, Next16StartsAt1AndIncrements) {
    IdGenerator gen;
    EXPECT_EQ(gen.next16(), uint16_t(1));
    EXPECT_EQ(gen.next16(), uint16_t(2));
    EXPECT_EQ(gen.next16(), uint16_t(3));
}

TEST(IdGeneratorTest, CurrentPeeksWithoutAdvancing) {
    IdGenerator gen;
    gen.next(); // consume 1
    EXPECT_EQ(gen.current(), 2u);
    EXPECT_EQ(gen.current(), 2u); // still 2 — not advanced
}

TEST(IdGeneratorTest, Current16PeeksWithoutAdvancing) {
    IdGenerator gen;
    gen.next16(); // consume 1
    EXPECT_EQ(gen.current16(), uint16_t(2));
    EXPECT_EQ(gen.current16(), uint16_t(2));
}

TEST(IdGeneratorTest, ResetRestores32Stream) {
    IdGenerator gen;
    gen.next(); gen.next(); gen.next(); // advance to 4
    gen.reset(10);
    EXPECT_EQ(gen.next(), 10u);
    EXPECT_EQ(gen.next(), 11u);
}

TEST(IdGeneratorTest, Reset16Restores16Stream) {
    IdGenerator gen;
    gen.next16(); gen.next16();
    gen.reset16(100);
    EXPECT_EQ(gen.next16(), uint16_t(100));
    EXPECT_EQ(gen.next16(), uint16_t(101));
}

TEST(IdGeneratorTest, ResetAllRestoresBothStreams) {
    IdGenerator gen;
    gen.next(); gen.next16();
    gen.resetAll(5, 50);
    EXPECT_EQ(gen.next(),   5u);
    EXPECT_EQ(gen.next16(), uint16_t(50));
}

TEST(IdGeneratorTest, StreamsAreIndependent) {
    IdGenerator gen;
    for (int i = 0; i < 10; ++i) gen.next();     // advance 32-bit to 11
    EXPECT_EQ(gen.current16(), uint16_t(1));      // 16-bit untouched

    for (int i = 0; i < 5; ++i) gen.next16();    // advance 16-bit to 6
    EXPECT_EQ(gen.current(), 11u);                // 32-bit untouched
}
