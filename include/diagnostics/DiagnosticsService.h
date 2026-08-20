#pragma once
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <dirent.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "diagnostics/JemallocCtl.h"

namespace diag {

class DiagnosticsService {
public:
    explicit DiagnosticsService(std::string serviceName)
        : service_(std::move(serviceName)) {}

    nlohmann::json snapshot() const {
        nlohmann::json j;
        j["service"] = service_;
        j["heap_profiling_available"] = JemallocCtl::profAvailable();

        // RSS/VSZ from /proc/self/statm (pages)
        long pageSize = ::sysconf(_SC_PAGESIZE);
        long long sizePages = 0, residentPages = 0;
        if (std::ifstream f("/proc/self/statm"); f) f >> sizePages >> residentPages;
        j["vsz_bytes"] = static_cast<int64_t>(sizePages) * pageSize;
        j["rss_bytes"] = static_cast<int64_t>(residentPages) * pageSize;

        // jemalloc allocator stats (0 when unavailable)
        j["heap"] = {
            {"allocated", (int64_t)JemallocCtl::statsBytes("stats.allocated")},
            {"active",    (int64_t)JemallocCtl::statsBytes("stats.active")},
            {"resident",  (int64_t)JemallocCtl::statsBytes("stats.resident")},
            {"mapped",    (int64_t)JemallocCtl::statsBytes("stats.mapped")},
        };

        // fds — countDir's own opendir handle appears in /proc/self/fd while iterating,
        // so the raw count is inflated by 1.
        int fdCount = countDir("/proc/self/fd");
        j["open_fds"] = fdCount > 0 ? fdCount - 1 : 0;

        // threads
        nlohmann::json threads = nlohmann::json::array();
        if (DIR* d = ::opendir("/proc/self/task"); d) {
            while (dirent* e = ::readdir(d)) {
                if (e->d_name[0] == '.') continue;
                nlohmann::json t;
                t["tid"] = e->d_name;
                t["name"] = readLine(std::string("/proc/self/task/") + e->d_name + "/comm");
                t["state"] = threadState(std::string("/proc/self/task/") + e->d_name + "/stat");
                threads.push_back(std::move(t));
            }
            ::closedir(d);
        }
        j["threads"] = static_cast<int>(threads.size());
        j["thread_list"] = std::move(threads);
        return j;
    }

    bool heapProfileActive(bool on) { return JemallocCtl::setProfActive(on); }

    nlohmann::json heapDump(const std::string& dir) {
        if (!JemallocCtl::profAvailable())
            return {{"status", "unavailable"}, {"reason", "jemalloc prof not enabled"}};
        std::string path = dir + "/" + service_ + "-" + nowStamp() + ".heap";
        auto r = JemallocCtl::dump(path);
        if (!r) return {{"status", "error"}, {"reason", "prof.dump failed"}};
        return {{"status", "ok"}, {"file", *r}};
    }

    // Async leak-hunt: active=true -> dump t0 -> sleep -> dump t1 -> active=false.
    // Returns immediately; files + captureStatus reflect completion.
    nlohmann::json heapCapture(const std::string& dir, uint32_t durationSec) {
        if (!JemallocCtl::profAvailable())
            return {{"status", "unavailable"}, {"reason", "jemalloc prof not enabled"}};
        std::string id = nowStamp();
        { std::lock_guard<std::mutex> lk(mtx_); captures_[id] = {"running", "", ""}; }

        // Lifetime contract: DiagnosticsService must outlive any in-flight capture — it
        // is constructed as a service-lifetime member and never destroyed before the
        // process exits, so the detached thread's use of `this` (service_, mtx_,
        // captures_) is safe. Do not construct DiagnosticsService as a short-lived local.
        std::thread([this, dir, durationSec, id] {
            JemallocCtl::setProfActive(true);
            std::string t0 = dir + "/" + service_ + "-" + id + "-t0.heap";
            JemallocCtl::dump(t0);
            writeStats(dir + "/" + service_ + "-" + id + "-stats-t0.json");
            std::this_thread::sleep_for(std::chrono::seconds(durationSec));
            std::string t1 = dir + "/" + service_ + "-" + id + "-t1.heap";
            JemallocCtl::dump(t1);
            writeStats(dir + "/" + service_ + "-" + id + "-stats-t1.json");
            JemallocCtl::setProfActive(false);
            std::lock_guard<std::mutex> lk(mtx_);
            captures_[id] = {"done", t0, t1};
        }).detach();

        return {{"status", "started"}, {"captureId", id},
                {"note", "poll diag.capture.status; symbolize t0/t1 in the diag sidecar"}};
    }

    nlohmann::json captureStatus(const std::string& id) const {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = captures_.find(id);
        if (it == captures_.end()) return {{"status", "unknown"}};
        return {{"status", it->second.state}, {"t0", it->second.t0}, {"t1", it->second.t1}};
    }

private:
    struct Capture { std::string state, t0, t1; };

    void writeStats(const std::string& path) {
        std::ofstream(path) << snapshot().dump(2);
    }
    static int countDir(const char* path) {
        int n = 0;
        if (DIR* d = ::opendir(path); d) {
            while (dirent* e = ::readdir(d)) if (e->d_name[0] != '.') ++n;
            ::closedir(d);
        }
        return n;
    }
    static std::string readLine(const std::string& path) {
        std::ifstream f(path); std::string s; std::getline(f, s); return s;
    }
    static std::string threadState(const std::string& statPath) {
        // /proc/.../stat: "pid (comm) STATE ...". State is the token after the last ')'.
        std::ifstream f(statPath); std::string s((std::istreambuf_iterator<char>(f)), {});
        auto p = s.rfind(')');
        if (p == std::string::npos || p + 2 >= s.size()) return "?";
        return std::string(1, s[p + 2]);
    }
    static std::string nowStamp() {
        // Microsecond granularity so two captures/dumps in the same second get distinct
        // ids + filenames (a second-granular id would collide and overwrite captures_).
        auto t = std::chrono::system_clock::now().time_since_epoch();
        return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t).count());
    }

    std::string service_;
    mutable std::mutex mtx_;
    std::unordered_map<std::string, Capture> captures_;
};

} // namespace diag
