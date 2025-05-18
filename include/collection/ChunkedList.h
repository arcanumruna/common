#pragma once

#include <vector>
#include <memory>
#include <iterator>

template<typename T>
class ChunkedList {
    struct Chunk {
        explicit Chunk(size_t capacity)
            : data(capacity), count(0), next(nullptr) {}

        bool isFull() const { return count == data.size(); }

        std::vector<T> data;
        size_t count;
        std::unique_ptr<Chunk> next;
    };

    const size_t chunkSize;
    size_t totalSize = 0;

    std::unique_ptr<Chunk> head;
    Chunk* tail;

public:
    explicit ChunkedList(const size_t chunkSize = 64)
        : chunkSize(chunkSize),
          head(std::make_unique<Chunk>(chunkSize)),
          tail(head.get()) {}

    void push_back(const T& value) {
        if (tail->isFull()) {
            if (!tail->next) {
                tail->next = std::make_unique<Chunk>(chunkSize);
            }
            tail = tail->next.get();
        }
        tail->data[tail->count++] = value;
        ++totalSize;
    }

    void clear() {
        totalSize = 0;
        tail = head.get();
        Chunk* current = head.get();
        while (current) {
            current->count = 0;
            current = current->next.get();
        }
    }

    size_t size() const { return totalSize; }

    // Optional: legacy functional foreach
    template<typename Fn>
    void forEach(Fn&& fn) const {
        for (const auto& val : *this) {
            fn(val);
        }
    }

    // Iterator support
    class const_iterator {
    public:
        using value_type = T;
        using reference = const T&;
        using pointer = const T*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        const_iterator(const Chunk* chunk, const size_t chunkIndex, const size_t remaining)
            : chunk(chunk), index(chunkIndex), remaining(remaining) {
            skipEmptyChunks();
        }

        reference operator*() const {
            return chunk->data[index];
        }

        pointer operator->() const {
            return &chunk->data[index];
        }

        const_iterator& operator++() {
            ++index;
            --remaining;

            if (index >= chunk->count && chunk->next) {
                chunk = chunk->next.get();
                index = 0;
                skipEmptyChunks();
            }

            return *this;
        }

        bool operator!=(const const_iterator& other) const {
            return chunk != other.chunk || index != other.index;
        }

    private:
        const Chunk* chunk;
        size_t index;
        size_t remaining;

        void skipEmptyChunks() {
            while (chunk && chunk->count == 0 && chunk->next) {
                chunk = chunk->next.get();
            }
        }
    };

    const_iterator begin() const {
        return const_iterator(head.get(), 0, totalSize);
    }

    const_iterator end() const {
        return const_iterator(nullptr, 0, 0);
    }

};
