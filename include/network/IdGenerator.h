#pragma once

class IdGenerator {
public:
    explicit IdGenerator(std::uint32_t start = 1)
        : currentId_(start) {}

    std::uint32_t next() {
        return currentId_++;
    }

    void reset(std::uint32_t start = 1) {
        currentId_ = start;
    }

private:
    std::uint32_t currentId_;
};
