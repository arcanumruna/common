#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include "file/Files.h"

// Helper: generate a unique temp path
static std::string tmpPath() {
    char buf[] = "/tmp/common_test_XXXXXX";
    int fd = mkstemp(buf);
    if (fd >= 0) close(fd); // close; createMmapBuffer will reopen
    return std::string(buf);
}

TEST(FilesTest, CreatesNewFileZeroInitialized) {
    std::string path = tmpPath();
    // Remove the file so createMmapBuffer creates it fresh
    ::unlink(path.c_str());

    constexpr size_t sz = 4096;
    int fd = -1;
    uint8_t* mem = createMmapBuffer(path.c_str(), sz, fd);

    ASSERT_NE(mem, nullptr);
    ASSERT_GE(fd, 0);

    // Verify memory is zero-initialised
    for (size_t i = 0; i < sz; ++i) {
        EXPECT_EQ(mem[i], 0u) << "Non-zero byte at index " << i;
        if (mem[i] != 0) break;
    }

    munmap(mem, sz);
    close(fd);
    ::unlink(path.c_str());
}

TEST(FilesTest, OpensExistingFilePreservesContent) {
    std::string path = tmpPath();
    ::unlink(path.c_str());

    constexpr size_t sz = 256;
    int fd = -1;

    // Create and write a pattern
    uint8_t* mem = createMmapBuffer(path.c_str(), sz, fd);
    for (size_t i = 0; i < sz; ++i) mem[i] = static_cast<uint8_t>(i % 256);
    munmap(mem, sz);
    close(fd);

    // Reopen the existing file
    fd = -1;
    mem = createMmapBuffer(path.c_str(), sz, fd);
    ASSERT_NE(mem, nullptr);

    // Verify pattern preserved
    for (size_t i = 0; i < sz; ++i) {
        EXPECT_EQ(mem[i], static_cast<uint8_t>(i % 256)) << "Mismatch at index " << i;
        if (mem[i] != static_cast<uint8_t>(i % 256)) break;
    }

    munmap(mem, sz);
    close(fd);
    ::unlink(path.c_str());
}

TEST(FilesTest, ThrowsOnInvalidDirectory) {
    int fd = -1;
    EXPECT_THROW(
        createMmapBuffer("/nonexistent_dir_common_test/file.bin", 4096, fd),
        std::runtime_error
    );
}
