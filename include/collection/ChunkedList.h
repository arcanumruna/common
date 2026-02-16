#pragma once

#include <memory>
#include <iterator>

template<typename T>
class ChunkedList {
    struct Chunk {
        Chunk(size_t capacity)
            : data(new T[capacity]), count(0), capacity(capacity), next(nullptr) {}

        ~Chunk() { delete[] data; }

        bool isFull() const { return count == capacity; }

        T* data;
        size_t count;
        size_t capacity;
        std::unique_ptr<Chunk> next;
    };

    const size_t chunkSize;
    size_t totalSize = 0;

    std::unique_ptr<Chunk> head;
    Chunk* tail;

public:
    explicit ChunkedList(size_t chunkSize = 64, size_t preallocateChunks = 1)
        : chunkSize(chunkSize) {
        head = std::make_unique<Chunk>(chunkSize);
        Chunk* current = head.get();

        // Preallocate and chain chunks
        for (size_t i = 1; i < preallocateChunks; ++i) {
            current->next = std::make_unique<Chunk>(chunkSize);
            current = current->next.get();
        }

        tail = head.get();  // tail points to last preallocated (empty) chunk
        while (tail->next) {
            tail = tail->next.get();
        }
    }

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

    template<typename Fn>
    void forEach(Fn&& fn) const {
        for (const auto& val : *this) {
            fn(val);
        }
    }

    class const_iterator {
    public:
        using value_type = T;
        using reference = const T&;
        using pointer = const T*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        const_iterator(const Chunk* chunk, size_t index, size_t remaining)
            : chunk(chunk), index(index), remaining(remaining) {
            skipEmptyChunks();
        }

        reference operator*() const { return chunk->data[index]; }
        pointer operator->() const { return &chunk->data[index]; }

        const_iterator& operator++() {
            ++index;
            --remaining;

            if (chunk && index >= chunk->count) {
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
            while (chunk && chunk->count == 0) {
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
