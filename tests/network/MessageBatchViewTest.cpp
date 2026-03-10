#include "network/MessageBatchView.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

// Helper to build a batch message buffer
// Wire format: [uint16_t LE length][uint8_t typeId][payload...]
std::vector<char> buildMessage(uint8_t typeId, const std::string& payload) {
    std::vector<char> msg;
    uint16_t length = static_cast<uint16_t>(payload.size() + 1); // typeId + payload
    msg.push_back(static_cast<char>(length & 0xFF));         // length low byte
    msg.push_back(static_cast<char>((length >> 8) & 0xFF));  // length high byte
    msg.push_back(static_cast<char>(typeId));                 // type ID
    msg.insert(msg.end(), payload.begin(), payload.end());    // payload
    return msg;
}

TEST(MessageBatchViewTest, EmptyBatch) {
    std::string_view data;
    MessageBatchView view(data);

    int count = 0;
    for (const auto& msg : view) {
        (void)msg;
        count++;
    }

    EXPECT_EQ(count, 0);
}

TEST(MessageBatchViewTest, SingleMessage) {
    std::vector<char> buffer = buildMessage(42, "abc");
    MessageBatchView view(std::string_view(buffer.data(), buffer.size()));

    auto it = view.begin();
    ASSERT_NE(it, view.end());

    EXPECT_EQ(it->typeId, 42);
    EXPECT_EQ(it->payloadSize, 3);
    EXPECT_EQ(std::string(it->payload, it->payloadSize), "abc");

    ++it;
    EXPECT_EQ(it, view.end());
}

TEST(MessageBatchViewTest, TwoMessages) {
    std::vector<char> buffer;
    auto m1 = buildMessage(1, "xy");
    auto m2 = buildMessage(2, "helo");
    buffer.insert(buffer.end(), m1.begin(), m1.end());
    buffer.insert(buffer.end(), m2.begin(), m2.end());

    MessageBatchView view(std::string_view(buffer.data(), buffer.size()));
    auto it = view.begin();

    ASSERT_NE(it, view.end());
    EXPECT_EQ(it->typeId, 1);
    EXPECT_EQ(std::string(it->payload, it->payloadSize), "xy");

    ++it;
    ASSERT_NE(it, view.end());
    EXPECT_EQ(it->typeId, 2);
    EXPECT_EQ(std::string(it->payload, it->payloadSize), "helo");

    ++it;
    EXPECT_EQ(it, view.end());
}

TEST(MessageBatchViewTest, ThreeMessages) {
    std::vector<char> buffer;
    auto m1 = buildMessage(1, "");
    auto m2 = buildMessage(7, "abc");
    auto m3 = buildMessage(9, "z");

    buffer.insert(buffer.end(), m1.begin(), m1.end());
    buffer.insert(buffer.end(), m2.begin(), m2.end());
    buffer.insert(buffer.end(), m3.begin(), m3.end());

    MessageBatchView view(std::string_view(buffer.data(), buffer.size()));

    std::vector<BatchedMessage> messages;
    for (const auto& msg : view) {
        messages.push_back(msg);
    }

    ASSERT_EQ(messages.size(), 3u);

    EXPECT_EQ(messages[0].typeId, 1);
    EXPECT_EQ(messages[0].payloadSize, 0u);

    EXPECT_EQ(messages[1].typeId, 7);
    EXPECT_EQ(std::string(messages[1].payload, messages[1].payloadSize), "abc");

    EXPECT_EQ(messages[2].typeId, 9);
    EXPECT_EQ(std::string(messages[2].payload, messages[2].payloadSize), "z");
}

TEST(MessageBatchViewTest, MalformedZeroLengthStopsIteration) {
    // A message with length == 0 is invalid; the iterator must stop immediately.
    // Need at least 3 bytes for the new format (2 length + 1 typeId)
    std::vector<char> buffer;
    buffer.push_back(0); // length low byte = 0
    buffer.push_back(0); // length high byte = 0
    buffer.push_back(5); // typeId (won't be read since length=0)

    MessageBatchView view(std::string_view(buffer.data(), buffer.size()));

    int count = 0;
    for (const auto& msg : view) { (void)msg; ++count; }
    EXPECT_EQ(count, 0);
}

TEST(MessageBatchViewTest, TruncatedPayloadStopsIteration) {
    // Header says payload is 10 bytes, but only 3 bytes are actually present.
    std::vector<char> buffer;
    uint16_t length = 11; // 1 (typeId) + 10 (payload)
    buffer.push_back(static_cast<char>(length & 0xFF));        // length low byte
    buffer.push_back(static_cast<char>((length >> 8) & 0xFF)); // length high byte
    buffer.push_back(static_cast<char>(5));  // typeId
    buffer.push_back('a'); buffer.push_back('b'); buffer.push_back('c'); // only 3 bytes

    MessageBatchView view(std::string_view(buffer.data(), buffer.size()));

    int count = 0;
    for (const auto& msg : view) { (void)msg; ++count; }
    EXPECT_EQ(count, 0); // truncated — iterator must stop before yielding the message
}
