#include "network/MessageBatchView.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

// Helper to build a batch message buffer
std::vector<char> buildMessage(uint8_t typeId, const std::string& payload) {
    std::vector<char> msg;
    msg.push_back(static_cast<char>(payload.size() + 1)); // total length
    msg.push_back(static_cast<char>(typeId));             // type ID
    msg.insert(msg.end(), payload.begin(), payload.end()); // payload
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
