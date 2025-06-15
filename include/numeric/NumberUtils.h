inline int fast_atoi(const char* s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (*s++ - '0');
    return val;
}

inline int16_t fast_atoi16(const char* s) { return static_cast<int16_t>(fast_atoi(s)); }