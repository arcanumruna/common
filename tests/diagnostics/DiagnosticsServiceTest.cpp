#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "diagnostics/DiagnosticsService.h"

// Helper declared/defined before the tests that reference it (plan Step 5 fix:
// the original Step 2 code called s_probeJemalloc() before its definition).
static bool s_probeJemalloc() { return diag::JemallocCtl::profAvailable(); }

TEST(DiagnosticsServiceTest, SnapshotHasProcessThreadAndMemoryFields) {
    diag::DiagnosticsService d("unittest");
    auto s = d.snapshot();

    EXPECT_EQ(s["service"], "unittest");
    EXPECT_GT(s["rss_bytes"].get<int64_t>(), 0);
    EXPECT_GT(s["threads"].get<int>(), 0);          // at least this thread
    EXPECT_GT(s["open_fds"].get<int>(), 0);         // at least stdout/err
    ASSERT_TRUE(s["thread_list"].is_array());
    ASSERT_FALSE(s["thread_list"].empty());
    EXPECT_TRUE(s["thread_list"][0].contains("name"));
    EXPECT_TRUE(s["thread_list"][0].contains("state"));
    // jemalloc may be absent in a plain unit build:
    EXPECT_TRUE(s.contains("heap_profiling_available"));
}

TEST(DiagnosticsServiceTest, HeapProfileUnavailableDegradesGracefully) {
    diag::DiagnosticsService d("unittest");
    if (!s_probeJemalloc()) {  // helper defined above
        EXPECT_FALSE(d.heapProfileActive(true));    // returns false, does not throw
        auto r = d.heapDump("/tmp");
        EXPECT_EQ(r["status"], "unavailable");
    }
    SUCCEED();
}
