inline int fast_atoi(const char* s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (*s++ - '0');
    return val;
}

inline int16_t fast_atoi16(const char* s) { return static_cast<int16_t>(fast_atoi(s)); }


inline std::string_view floatToStringView(float value, char* buffer, size_t bufferSize) {
    auto [ptr, ec] = std::to_chars(buffer, buffer + bufferSize, value, std::chars_format::general);
    if (ec != std::errc()) {
        throw std::runtime_error("floatToStringView: to_chars conversion failed");
    }
    return std::string_view(buffer, ptr - buffer);
}

inline std::string floatToString(float f) {
    char buffer[32];
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), f, std::chars_format::general);

    if (ec != std::errc()) {
        throw std::runtime_error("floatToString: to_chars conversion failed");
    }

    // Create std::string on the heap with exact size
    return std::string(buffer, ptr);
}