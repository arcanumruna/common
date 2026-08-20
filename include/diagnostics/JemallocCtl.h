#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#if defined(__has_include)
#  if __has_include(<jemalloc/jemalloc.h>)
#    include <jemalloc/jemalloc.h>
#    define DIAG_HAVE_JEMALLOC 1
#  endif
#endif

namespace diag {

class JemallocCtl {
public:
    static bool profAvailable() {
#ifdef DIAG_HAVE_JEMALLOC
        bool enabled = false; size_t sz = sizeof(enabled);
        // "opt.prof" is true only when built with --enable-prof AND MALLOC_CONF prof:true
        return ::mallctl("opt.prof", &enabled, &sz, nullptr, 0) == 0 && enabled;
#else
        return false;
#endif
    }

    static bool setProfActive(bool on) {
#ifdef DIAG_HAVE_JEMALLOC
        bool v = on;
        return ::mallctl("prof.active", nullptr, nullptr, &v, sizeof(v)) == 0;
#else
        (void)on; return false;
#endif
    }

    static std::optional<std::string> dump(const std::string& path) {
#ifdef DIAG_HAVE_JEMALLOC
        const char* p = path.c_str();
        if (::mallctl("prof.dump", nullptr, nullptr, &p, sizeof(p)) == 0) return path;
        return std::nullopt;
#else
        (void)path; return std::nullopt;
#endif
    }

    static size_t statsBytes(const char* which) {
#ifdef DIAG_HAVE_JEMALLOC
        // Refresh cached stats first: writing "epoch" advances jemalloc's stats epoch so
        // the subsequent stats.* reads are current. The same variable is intentionally
        // both new-value input and old-value output — jemalloc's documented idiom.
        uint64_t epoch = 1; size_t esz = sizeof(epoch);
        ::mallctl("epoch", &epoch, &esz, &epoch, sizeof(epoch));
        size_t v = 0, sz = sizeof(v);
        if (::mallctl(which, &v, &sz, nullptr, 0) == 0) return v;
        return 0;
#else
        (void)which; return 0;
#endif
    }
};

} // namespace diag
