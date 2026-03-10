#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "numeric/NumberUtils.h"

// ---- fast_atoi ----

TEST(NumberUtilsTest, FastAtoiNormal) {
    EXPECT_EQ(fast_atoi("123"), 123);
}

TEST(NumberUtilsTest, FastAtoiStopsAtNonDigit) {
    EXPECT_EQ(fast_atoi("12x4"), 12);
}

TEST(NumberUtilsTest, FastAtoiEmptyString) {
    EXPECT_EQ(fast_atoi(""), 0);
}

TEST(NumberUtilsTest, FastAtoiLeadingZeros) {
    EXPECT_EQ(fast_atoi("007"), 7);
}

// ---- fast_atoi16 ----

TEST(NumberUtilsTest, FastAtoi16CastsCorrectly) {
    EXPECT_EQ(fast_atoi16("100"), int16_t(100));
    EXPECT_EQ(fast_atoi16("0"), int16_t(0));
}

// ---- stringViewToUINT64 ----

TEST(NumberUtilsTest, StringViewToUINT64Valid) {
    EXPECT_EQ(stringViewToUINT64("0"), 0ull);
    EXPECT_EQ(stringViewToUINT64("42"), 42ull);
    EXPECT_EQ(stringViewToUINT64("18446744073709551615"), UINT64_MAX);
}

TEST(NumberUtilsTest, StringViewToUINT64InvalidArgument) {
    EXPECT_THROW(stringViewToUINT64("abc"), std::runtime_error);
}

TEST(NumberUtilsTest, StringViewToUINT64OutOfRange) {
    EXPECT_THROW(stringViewToUINT64("99999999999999999999"), std::runtime_error);
}

TEST(NumberUtilsTest, StringViewToUINT64TrailingChars) {
    EXPECT_THROW(stringViewToUINT64("42abc"), std::runtime_error);
}

TEST(NumberUtilsTest, StringViewToUINT64EmptyThrows) {
    EXPECT_THROW(stringViewToUINT64(""), std::runtime_error);
}

// ---- floatToString ----

TEST(NumberUtilsTest, FloatToStringNormal) {
    EXPECT_EQ(floatToString(1.5f), "1.5");
}

TEST(NumberUtilsTest, FloatToStringZero) {
    EXPECT_EQ(floatToString(0.0f), "0");
}

TEST(NumberUtilsTest, FloatToStringNegative) {
    EXPECT_EQ(floatToString(-1.0f), "-1");
}

// ---- floatToStringView ----

TEST(NumberUtilsTest, FloatToStringViewNormal) {
    char buf[32];
    auto sv = floatToStringView(2.5f, buf, sizeof(buf));
    EXPECT_EQ(sv, "2.5");
}

TEST(NumberUtilsTest, FloatToStringViewBufferTooSmall) {
    char buf[1]; // definitely too small
    EXPECT_THROW(floatToStringView(1234567.0f, buf, sizeof(buf)), std::runtime_error);
}

// ---- uint32ToString ----

TEST(NumberUtilsTest, Uint32ToStringNormal) {
    EXPECT_EQ(uint32ToString(42u), "42");
}

TEST(NumberUtilsTest, Uint32ToStringZero) {
    EXPECT_EQ(uint32ToString(0u), "0");
}

TEST(NumberUtilsTest, Uint32ToStringMax) {
    EXPECT_EQ(uint32ToString(4294967295u), "4294967295");
}

// ---- fast_inv_sqrt ----

TEST(NumberUtilsTest, FastInvSqrtApproximate) {
    // 1/sqrt(4) = 0.5
    EXPECT_NEAR(fast_inv_sqrt(4.0f), 0.5f, 0.002f);
    // 1/sqrt(1) = 1.0
    EXPECT_NEAR(fast_inv_sqrt(1.0f), 1.0f, 0.002f);
    // 1/sqrt(100) = 0.1
    EXPECT_NEAR(fast_inv_sqrt(100.0f), 0.1f, 0.002f);
}

// ---- is_valid_float ----

TEST(NumberUtilsTest, IsValidFloatNormal) {
    EXPECT_TRUE(is_valid_float(1.0f));
    EXPECT_TRUE(is_valid_float(-1.0f));
    EXPECT_TRUE(is_valid_float(0.0f));
}

TEST(NumberUtilsTest, IsValidFloatNaN) {
    EXPECT_FALSE(is_valid_float(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(is_valid_float(std::numeric_limits<float>::signaling_NaN()));
}

TEST(NumberUtilsTest, IsValidFloatInf) {
    EXPECT_FALSE(is_valid_float(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(is_valid_float(-std::numeric_limits<float>::infinity()));
}

// BUG-L18 regression: fast_atoi must not overflow on huge numeric strings
TEST(NumberUtilsTest, L18_FastAtoiOverflowReturnsMax) {
    // "9999999999" is larger than INT32_MAX (2147483647)
    EXPECT_EQ(fast_atoi("9999999999"), INT32_MAX);
}

TEST(NumberUtilsTest, L18_FastAtoiMaxInt32IsOk) {
    // INT32_MAX = 2147483647 should parse correctly
    EXPECT_EQ(fast_atoi("2147483647"), 2147483647);
}

TEST(NumberUtilsTest, L18_FastAtoiJustOverMaxInt32) {
    // 2147483648 is INT32_MAX+1, should clamp
    EXPECT_EQ(fast_atoi("2147483648"), INT32_MAX);
}
