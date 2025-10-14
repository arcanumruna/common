#pragma once
#include <charconv>
#include <cstdlib>
#include <string>

inline std::string getEnv(const char* key, const char* def = nullptr) {
    if (const char* v = std::getenv(key)) return std::string(v);
    if (def) return std::string(def);
    throw std::runtime_error(std::string("Missing required env: ") + key);
}

template <typename T>
inline T getEnvNum(const char* key, T def) {
    if (const char* v = std::getenv(key)) {
        T out{};
        auto sv = std::string_view(v);
        auto res = std::from_chars(sv.data(), sv.data()+sv.size(), out);
        if (res.ec == std::errc()) return out;
        throw std::runtime_error(std::string("Invalid numeric env: ") + key);
    }
    return def;
}