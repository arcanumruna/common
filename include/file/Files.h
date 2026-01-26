#pragma once


#include <cerrno>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

inline uint8_t* createMmapBuffer(const char* filepath, size_t size, int& outFd) {
    bool createdNew = false;

    // Check if file exists
    struct stat st{};
    if (stat(filepath, &st) != 0) {
        if (errno == ENOENT) {
            createdNew = true;
        } else {
            throw std::runtime_error(std::string("stat failed: ") + strerror(errno));
        }
    }

    // Open or create the file
    int fd = open(filepath, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        throw std::runtime_error(std::string("open failed: ") + strerror(errno));
    }

    // If newly created, resize it
    if (createdNew) {
        if (ftruncate(fd, size) < 0) {
            close(fd);
            throw std::runtime_error(std::string("ftruncate failed: ") + strerror(errno));
        }
    }

    // Map the file into memory
    void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error(std::string("mmap failed: ") + strerror(errno));
    }

    // Zero out the memory only if file was newly created
    if (createdNew) {
        std::memset(addr, 0, size);
    }

    outFd = fd;
    return static_cast<uint8_t*>(addr);
}
