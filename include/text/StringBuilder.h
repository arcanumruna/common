#pragma once

#include <string_view>

struct StringBuilder {

    static constexpr char kDigitPairs[200] = {
        '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
        '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
        '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
        '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
        '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
        '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
        '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
        '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
        '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
        '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
    };

    char* buf;
    size_t capacity;
    size_t len;

    StringBuilder(char* buf_, const size_t cap) : buf(buf_), capacity(cap), len(0) {}

    void append(const std::string_view sv) {
        size_t to_copy = std::min(capacity - len, sv.size());
        memcpy(buf + len, sv.data(), to_copy);
        len += to_copy;
    }

    void append(const char c) {
        if (len < capacity) buf[len++] = c;
    }

    void append(uint64_t value) {
        if (len >= capacity) return;
        int avail = static_cast<int>(capacity - len);
        char* dest = buf + len;
        int n = uint64_to_chars_unsafe(value, dest);
        if (n > avail) n = avail;
        len += n;
    }

    inline int uint64_to_chars_unsafe(uint64_t value, char* buffer) {
        if (value == 0) {
            buffer[0] = '0';
            return 1;
        }

        int num_digits = fast_num_digits(value);
        char* ptr = buffer + num_digits - 1;

        while (value >= 100) {
            uint64_t remainder = value % 100;
            value /= 100;

            const char* pair = &kDigitPairs[remainder * 2];
            *ptr-- = pair[1];
            *ptr-- = pair[0];
        }

        if (value >= 10) {
            const char* pair = &kDigitPairs[value * 2];
            *ptr-- = pair[1];
            *ptr = pair[0];
        } else {
            *ptr = '0' + value;
        }

        return num_digits;
    }

    inline int fast_num_digits(uint64_t n) {
        if (n < 10) return 1;
        if (n < 100) return 2;
        if (n < 1000) return 3;
        if (n < 10000) return 4;
        if (n < 100000) return 5;
        if (n < 1000000) return 6;
        if (n < 10000000) return 7;
        if (n < 100000000) return 8;
        if (n < 1000000000) return 9;
        if (n < 10000000000ULL) return 10;
        if (n < 100000000000ULL) return 11;
        if (n < 1000000000000ULL) return 12;
        if (n < 10000000000000ULL) return 13;
        if (n < 100000000000000ULL) return 14;
        if (n < 1000000000000000ULL) return 15;
        if (n < 10000000000000000ULL) return 16;
        if (n < 100000000000000000ULL) return 17;
        if (n < 1000000000000000000ULL) return 18;
        return 19;
    }

    const char* c_str() const {
        if (len < capacity) buf[len] = '\0';
        else buf[capacity-1] = '\0';
        return buf;
    }
    size_t size() const { return len; }
    std::string_view view() const {
        return std::string_view(buf, len);
    }
    void clear() { len = 0; }
};