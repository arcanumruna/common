#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>

#include "concurrent/agrona/AgronaRecordDescriptor.h"
#include "concurrent/agrona/AgronaRingBufferDescriptor.h"
#include "concurrent/agrona/M2ORingBuffer.h"
#include "concurrent/agrona/O2ORingBuffer.h"

namespace rb = agrona::concurrent::ringbuffer;

namespace {

constexpr std::size_t kCapacity = 1024;
constexpr std::size_t kLength = kCapacity + rb::AgronaRingBufferDescriptor::TRAILER_LENGTH;

template <class Ring>
struct RingBufferTest : ::testing::Test {
    RingBufferTest()
        : memory(zeroed()), ring(memory, kLength) {}
    ~RingBufferTest() override { std::free(memory); }

    static std::uint8_t* zeroed() {
        auto* p = static_cast<std::uint8_t*>(std::aligned_alloc(64, kLength));
        std::memset(p, 0, kLength);
        return p;
    }

    // Claims, fills with `value` and commits one record. Returns false when the ring is full.
    bool write(std::int32_t type, std::uint64_t value) {
        const auto index = ring.tryClaim(type, sizeof(value));
        if (index < 0) {
            return false;
        }
        std::memcpy(memory + index, &value, sizeof(value));
        ring.commit(index);
        return true;
    }

    struct Seen { std::int32_t type; std::uint64_t value; std::int32_t length; };

    std::vector<Seen> drain(int limit) {
        std::vector<Seen> out;
        const int n = ring.read(
            [&](std::int32_t type, agrona::concurrent::AgronaAtomicBuffer& buffer, std::int32_t index, std::int32_t length) {
                std::uint64_t v = 0;
                std::memcpy(&v, buffer.buffer() + index, sizeof(v));
                out.push_back({type, v, length});
            },
            limit);
        EXPECT_EQ(n, static_cast<int>(out.size()));
        return out;
    }

    std::uint8_t* memory;
    Ring ring;
};

using Rings = ::testing::Types<rb::M2ORingBuffer, rb::O2ORingBuffer>;
TYPED_TEST_SUITE(RingBufferTest, Rings);

TYPED_TEST(RingBufferTest, RoundTripKeepsTypeLengthAndOrder) {
    ASSERT_EQ(this->ring.capacity(), static_cast<std::int32_t>(kCapacity));
    for (std::uint64_t i = 1; i <= 5; ++i) {
        ASSERT_TRUE(this->write(static_cast<std::int32_t>(i), i * 1000));
    }
    const auto seen = this->drain(100);
    ASSERT_EQ(seen.size(), 5u);
    for (std::uint64_t i = 1; i <= 5; ++i) {
        EXPECT_EQ(seen[i - 1].type, static_cast<std::int32_t>(i));
        EXPECT_EQ(seen[i - 1].value, i * 1000);
        EXPECT_EQ(seen[i - 1].length, 8);
    }
    EXPECT_TRUE(this->drain(100).empty());
}

TYPED_TEST(RingBufferTest, FullRingRefusesThenAcceptsAfterDrain) {
    // One record is an 8-byte header plus 8 bytes of payload: 64 records fill 1024 bytes exactly. The one-to-one
    // ring keeps room for one more header after each claim (as Aeron's does), so it takes 63.
    const int expected = std::is_same_v<TypeParam, rb::O2ORingBuffer> ? 63 : 64;
    int written = 0;
    while (this->write(7, static_cast<std::uint64_t>(written))) {
        ++written;
        ASSERT_LE(written, 64);
    }
    EXPECT_EQ(written, expected);
    EXPECT_EQ(this->drain(1000).size(), static_cast<std::size_t>(expected));
    EXPECT_TRUE(this->write(7, 99));
    const auto seen = this->drain(1000);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].value, 99u);
}

TYPED_TEST(RingBufferTest, WrapsManyTimesWithoutLossOrReorder) {
    std::uint64_t next = 0;
    std::uint64_t expected = 0;
    for (int round = 0; round < 500; ++round) {
        for (int i = 0; i < 7; ++i) {
            ASSERT_TRUE(this->write(3, next++));
        }
        // A read stops at the end of the buffer, so drain until empty (bounded).
        for (int pass = 0; pass < 3; ++pass) {
            for (const auto& s : this->drain(100)) {
                ASSERT_EQ(s.value, expected++);
            }
        }
        ASSERT_EQ(expected, next);
    }
}

TYPED_TEST(RingBufferTest, ReadHonoursMessageLimit) {
    for (std::uint64_t i = 0; i < 10; ++i) {
        ASSERT_TRUE(this->write(1, i));
    }
    EXPECT_EQ(this->drain(4).size(), 4u);
    EXPECT_EQ(this->drain(100).size(), 6u);
}

TYPED_TEST(RingBufferTest, AbortedClaimIsSkipped) {
    ASSERT_TRUE(this->write(1, 11));
    const auto index = this->ring.tryClaim(1, 8);
    ASSERT_GE(index, 0);
    this->ring.abort(index);
    ASSERT_TRUE(this->write(1, 33));
    const auto seen = this->drain(100);
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0].value, 11u);
    EXPECT_EQ(seen[1].value, 33u);
}

TYPED_TEST(RingBufferTest, RejectsNonPositiveTypeId) {
    EXPECT_THROW((void)this->ring.tryClaim(0, 8), aeron::util::IllegalArgumentException);
    EXPECT_THROW((void)this->ring.tryClaim(-1, 8), aeron::util::IllegalArgumentException);
}

TEST(AgronaRecordDescriptor, LayoutMatchesAeron) {
    namespace rd = rb::AgronaRecordDescriptor;
    EXPECT_EQ(rd::HEADER_LENGTH, 8);
    EXPECT_EQ(rd::ALIGNMENT, 8);
    EXPECT_EQ(rd::PADDING_MSG_TYPE_ID, -1);
    EXPECT_EQ(rd::lengthOffset(64), 64);
    EXPECT_EQ(rd::typeOffset(64), 68);
    EXPECT_EQ(rd::encodedMsgOffset(64), 72);
    const std::int64_t header = rd::makeHeader(24, 9);
    EXPECT_EQ(rd::recordLength(header), 24);
    EXPECT_EQ(rd::messageTypeId(header), 9);
}

}  // namespace
