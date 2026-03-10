#pragma once

#include <string_view>

struct BatchedMessage {
    uint8_t typeId;
    const char* payload;
    size_t payloadSize;
};

// BUG-M29 fix: Changed per-message length from uint8_t to uint16_t to support messages > 254 bytes.
// Wire format: [uint16_t length][uint8_t typeId][payload...]
// length = typeId byte + payload size (does NOT include the 2-byte length prefix itself)
class MessageBatchView {
public:
    class Iterator {
    public:
        Iterator(const char* start, const char* end)
            : ptr(start), end(end) {
            advance();  // first decode
        }

        Iterator() : ptr(nullptr), end(nullptr) {}

        bool operator==(const Iterator& other) const {
            return ptr == other.ptr;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

        const BatchedMessage& operator*() const { return current; }
        const BatchedMessage* operator->() const { return &current; }

        Iterator& operator++() {
            advance();
            return *this;
        }

    private:
        const char* ptr = nullptr;
        const char* end = nullptr;
        BatchedMessage current{};

        void advance() {
            // Need at least 3 bytes: 2 for length + 1 for typeId
            if (!ptr || ptr >= end || (end - ptr) < 3) {
                ptr = nullptr;  // use nullptr as end sentinel
                return;
            }

            // Read uint16_t length (little-endian)
            uint16_t length = static_cast<uint16_t>(
                static_cast<uint8_t>(ptr[0]) |
                (static_cast<uint8_t>(ptr[1]) << 8));

            if (length < 1 || (end - ptr) < (2 + length)) {
                ptr = nullptr;
                return;
            }

            current.typeId = static_cast<uint8_t>(ptr[2]);
            current.payload = ptr + 3;
            current.payloadSize = length - 1;

            ptr += 2 + length;
        }
    };

    explicit MessageBatchView(std::string_view data)
        : data_(data.data()), size_(data.size()) {}

    Iterator begin() const { return Iterator(data_, data_ + size_); }
    Iterator end() const { return Iterator(data_ + size_, data_ + size_); }

private:
    const char* data_;
    size_t size_;
};
