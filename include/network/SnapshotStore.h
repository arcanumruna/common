#pragma once

#include <array>

template <typename T, size_t Capacity>
class SnapshotStore {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    struct Entry {
        int64_t timestamp;
        T value;
    };

    SnapshotStore() : size(0), writeIndex(0) {}

    void addSnapshot(int64_t ts, const T& val) {
        entries[writeIndex] = {ts, val};
        writeIndex = (writeIndex + 1) & (Capacity - 1);
        if (size < Capacity) ++size;
    }

    bool floor(int64_t ts, const T*& out) const {
        for (int i = static_cast<int>(size) - 1; i >= 0; --i) {
            Entry& e = entries[(writeIndex - 1 - i + Capacity) & (Capacity - 1)];
            if (e.timestamp <= ts) {
                out = &e.value;
                return true;
            }
        }
        return false;
    }

    bool ceil(int64_t ts, const T*& out) const {
        for (size_t i = 0; i < size; ++i) {
            Entry& e = entries[(writeIndex - size + i + Capacity) & (Capacity - 1)];
            if (e.timestamp >= ts) {
                out = &e.value;
                return true;
            }
        }
        return false;
    }

    bool getLastSnapshot(T& out) {
        if (size == 0) return false;
        out = entries[(writeIndex - 1 + Capacity) & (Capacity - 1)].value;
        return true;
    }

    bool getSecondLastSnapshot(T& out) {
        if (size < 2) return false;
        out = entries[(writeIndex - 2 + Capacity) & (Capacity - 1)].value;
        return true;
    }

    size_t getSize() const { return size; }

private:
    std::array<Entry, Capacity> entries{};
    size_t size;
    size_t writeIndex;
};
