#pragma once

#include <string_view>

struct BatchedMessage {
    uint8_t typeId;
    const char* payload;
    size_t payloadSize;
};

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
            if (!ptr || ptr >= end || (end - ptr) < 2) {
                ptr = nullptr;  // use nullptr as end sentinel
                return;
            }

            uint8_t length = static_cast<uint8_t>(ptr[0]);
            if (length < 1 || (end - ptr) < (1 + length)) {
                ptr = nullptr;
                return;
            }

            current.typeId = static_cast<uint8_t>(ptr[1]);
            current.payload = ptr + 2;
            current.payloadSize = length - 1;

            ptr += 1 + length;
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
