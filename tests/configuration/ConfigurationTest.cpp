#include <gtest/gtest.h>
#include <cstdlib>
#include "configuration/Configuration.h"

// Helper: RAII env variable setter
struct EnvGuard {
    const char* key;
    explicit EnvGuard(const char* k, const char* v) : key(k) { setenv(k, v, 1); }
    ~EnvGuard() { unsetenv(key); }
};

// ---- getEnv ----

TEST(ConfigurationTest, GetEnvReturnsValueWhenSet) {
    EnvGuard g("TEST_CFG_KEY", "hello");
    EXPECT_EQ(getEnv("TEST_CFG_KEY"), "hello");
}

TEST(ConfigurationTest, GetEnvReturnsDefaultWhenAbsent) {
    unsetenv("TEST_CFG_ABSENT");
    EXPECT_EQ(getEnv("TEST_CFG_ABSENT", "fallback"), "fallback");
}

TEST(ConfigurationTest, GetEnvThrowsWhenAbsentAndNoDefault) {
    unsetenv("TEST_CFG_REQUIRED");
    EXPECT_THROW(getEnv("TEST_CFG_REQUIRED"), std::runtime_error);
}

// ---- getEnvNum<int> ----

TEST(ConfigurationTest, GetEnvNumIntReturnsValue) {
    EnvGuard g("TEST_CFG_INT", "42");
    EXPECT_EQ(getEnvNum<int>("TEST_CFG_INT", 0), 42);
}

TEST(ConfigurationTest, GetEnvNumIntReturnsDefaultWhenAbsent) {
    unsetenv("TEST_CFG_INT_ABSENT");
    EXPECT_EQ(getEnvNum<int>("TEST_CFG_INT_ABSENT", 99), 99);
}

TEST(ConfigurationTest, GetEnvNumIntThrowsOnInvalidInput) {
    EnvGuard g("TEST_CFG_INT_BAD", "abc");
    EXPECT_THROW(getEnvNum<int>("TEST_CFG_INT_BAD", 0), std::runtime_error);
}

TEST(ConfigurationTest, GetEnvNumUint32ReturnsValue) {
    EnvGuard g("TEST_CFG_U32", "4294967295");
    EXPECT_EQ(getEnvNum<uint32_t>("TEST_CFG_U32", 0u), 4294967295u);
}

// ---- getEnvNum<float> ----

TEST(ConfigurationTest, GetEnvNumFloatReturnsValue) {
    EnvGuard g("TEST_CFG_FLOAT", "3.14");
    EXPECT_NEAR(getEnvNum<float>("TEST_CFG_FLOAT", 0.0f), 3.14f, 0.001f);
}

TEST(ConfigurationTest, GetEnvNumFloatReturnsDefaultWhenAbsent) {
    unsetenv("TEST_CFG_FLOAT_ABSENT");
    EXPECT_NEAR(getEnvNum<float>("TEST_CFG_FLOAT_ABSENT", 2.718f), 2.718f, 0.001f);
}

TEST(ConfigurationTest, GetEnvNumFloatThrowsOnInvalidInput) {
    EnvGuard g("TEST_CFG_FLOAT_BAD", "xyz");
    EXPECT_THROW(getEnvNum<float>("TEST_CFG_FLOAT_BAD", 0.0f), std::runtime_error);
}
