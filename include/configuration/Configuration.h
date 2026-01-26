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
    const char* v = std::getenv(key);
    if (!v) return def;

    if constexpr (std::is_floating_point_v<T>) {
        char* end;
        double result = std::strtod(v, &end);
        if (end == v || *end != '\0') {
            throw std::runtime_error(std::string("Invalid numeric env: ") + key);
        }
        return static_cast<T>(result);
    } else {
        T out{};
        auto sv = std::string_view(v);
        auto res = std::from_chars(sv.data(), sv.data() + sv.size(), out);
        if (res.ec == std::errc()) return out;
        throw std::runtime_error(std::string("Invalid numeric env: ") + key);
    }
}