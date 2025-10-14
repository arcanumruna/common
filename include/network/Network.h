#pragma once
#include <string>
#include <unistd.h>
#include <vector>

inline std::string getHostname() {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) return buf;
    return "unknown-host";
}