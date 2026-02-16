#include <gtest/gtest.h>
#include "network/Network.h"

TEST(NetworkTest, GetHostnameReturnsNonEmptyString) {
    std::string hostname = getHostname();
    EXPECT_FALSE(hostname.empty());
}

TEST(NetworkTest, GetHostnameIsNotUnknown) {
    // On any normal development or CI machine, gethostname() should succeed.
    std::string hostname = getHostname();
    EXPECT_NE(hostname, "unknown-host");
}
