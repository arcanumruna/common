#include <gtest/gtest.h>
#include <cstring>
#include <limits>
#include "text/StringBuilder.h"

TEST(StringBuilderTest, AppendStringViewNormal) {
    char buf[64] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("hello"));
    EXPECT_EQ(sb.size(), 5u);
    EXPECT_EQ(sb.view(), "hello");
}

TEST(StringBuilderTest, AppendStringViewTruncatesWhenFull) {
    char buf[4] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("hello")); // only 4 chars fit
    EXPECT_EQ(sb.size(), 4u);
    EXPECT_EQ(sb.view(), "hell");
}

TEST(StringBuilderTest, AppendCharNormal) {
    char buf[8] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append('A');
    sb.append('B');
    EXPECT_EQ(sb.size(), 2u);
    EXPECT_EQ(sb.view(), "AB");
}

TEST(StringBuilderTest, AppendCharNoOpWhenFull) {
    char buf[2] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append('X');
    sb.append('Y');
    sb.append('Z'); // no-op: buffer full
    EXPECT_EQ(sb.size(), 2u);
}

TEST(StringBuilderTest, AppendUint64Zero) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(uint64_t(0));
    EXPECT_EQ(sb.view(), "0");
}

TEST(StringBuilderTest, AppendUint64SingleDigit) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(uint64_t(7));
    EXPECT_EQ(sb.view(), "7");
}

TEST(StringBuilderTest, AppendUint64MultiDigit) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(uint64_t(1234567890));
    EXPECT_EQ(sb.view(), "1234567890");
}

TEST(StringBuilderTest, AppendUint64MaxValue) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::numeric_limits<uint64_t>::max()); // 18446744073709551615
    EXPECT_EQ(sb.view(), "18446744073709551615");
}

TEST(StringBuilderTest, AppendUint64TruncatesWhenInsufficient) {
    char buf[3] = {}; // only 3 bytes available
    StringBuilder sb(buf, sizeof(buf));
    sb.append(uint64_t(12345)); // 5 digits, only 3 fit
    EXPECT_EQ(sb.size(), 3u);
    // No memory corruption: buf content must be the first 3 digits "123"
    EXPECT_EQ(sb.view(), "123");
}

TEST(StringBuilderTest, CStrNullTerminates) {
    char buf[16] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("hi"));
    const char* cs = sb.c_str();
    EXPECT_EQ(std::strlen(cs), 2u);
    EXPECT_STREQ(cs, "hi");
}

TEST(StringBuilderTest, CStrTruncatesLastCharWhenFull) {
    char buf[4] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("ABCD")); // exactly fills buf
    EXPECT_EQ(sb.size(), 4u);
    const char* cs = sb.c_str(); // must overwrite buf[3] with '\0'
    EXPECT_EQ(std::strlen(cs), 3u);
    EXPECT_STREQ(cs, "ABC");
}

TEST(StringBuilderTest, ViewReturnsCurrentContent) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("test"));
    sb.append(uint64_t(42));
    EXPECT_EQ(sb.view(), "test42");
}

TEST(StringBuilderTest, ClearResetsLength) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("abc"));
    sb.clear();
    EXPECT_EQ(sb.size(), 0u);
}

TEST(StringBuilderTest, SizeReflectsLength) {
    char buf[32] = {};
    StringBuilder sb(buf, sizeof(buf));
    EXPECT_EQ(sb.size(), 0u);
    sb.append('X');
    EXPECT_EQ(sb.size(), 1u);
    sb.append(std::string_view("YZ"));
    EXPECT_EQ(sb.size(), 3u);
}

// BUG-L17 regression: c_str() must be non-const (it writes a null terminator)
TEST(StringBuilderTest, L17_CStrIsNonConst) {
    char buf[16] = {};
    StringBuilder sb(buf, sizeof(buf));
    sb.append(std::string_view("test"));
    // This must compile — c_str() is non-const and mutates the buffer
    const char* cs = sb.c_str();
    EXPECT_STREQ(cs, "test");
    EXPECT_EQ(buf[4], '\0'); // null terminator was written
}
