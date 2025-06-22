#pragma once

#include <cstring>
#include <string>
#include <array>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstdint>

#include "util/Index.h"
#include "util/Exceptions.h"
#include "util/StringUtil.h"
#include "util/MacroUtil.h"
#include "concurrent/Atomic64.h"
#include "aeron.h"

namespace agrona { namespace concurrent {

/**
 * Wraps, but does not own, a buffer of memory for providing atomic operations. This is for providing a view.
 */
class AgronaAtomicBuffer
{
public:
    constexpr AgronaAtomicBuffer() noexcept = default;

    /**
     * Wrap a buffer of memory for a given length.
     *
     * @param buffer to be wrapped.
     * @param length of the buffer for bounds checking.
     */
    AgronaAtomicBuffer(std::uint8_t *buffer, std::size_t length) :
        m_buffer(buffer),
        m_length(static_cast<length_t>(length))
    {
#if !defined(DISABLE_BOUNDS_CHECKS)
        if (AERON_COND_EXPECT(length > static_cast<std::size_t>(std::numeric_limits<aeron::util::index_t>::max()), true))
        {
            throw aeron::util::OutOfBoundsException(
                aeron::util::strPrintf("length out of bounds[%p]: length=%lld", this, static_cast<long long>(length)),
                SOURCEINFO);
        }
#endif
    }

    /**
     * Wrap a buffer of memory for a given length and initialise the contents.
     *
     * @param buffer       to be wrapped.
     * @param length       of the buffer for bounds checking.
     * @param initialValue to set the memory too.
     */
    AgronaAtomicBuffer(std::uint8_t *buffer, std::size_t length, std::uint8_t initialValue) :
        m_buffer(buffer),
        m_length(static_cast<length_t>(length))
    {
#if !defined(DISABLE_BOUNDS_CHECKS)
        if (AERON_COND_EXPECT(length > static_cast<std::size_t>(std::numeric_limits<aeron::util::index_t>::max()), true))
        {
            throw aeron::util::OutOfBoundsException(
                aeron::util::strPrintf("length out of bounds[%p]. length=%lld", this, static_cast<long long>(length)),
                SOURCEINFO);
        }
#endif

        setMemory(0, static_cast<std::size_t>(length), initialValue);
    }

    template<std::size_t N>
    explicit AgronaAtomicBuffer(std::array<std::uint8_t, N> &buffer)
    {
        wrap(buffer);
    }

    template<std::size_t N>
    AgronaAtomicBuffer(std::array<std::uint8_t, N> &buffer, std::uint8_t initialValue)
    {
        wrap(buffer);
        buffer.fill(initialValue);
    }

    /**
     * Wrap a buffer of memory for a given length.
     *
     * @param buffer to be wrapped.
     * @param length of the buffer for bounds checking.
     */
    inline void wrap(std::uint8_t *buffer, std::size_t length)
    {
#if !defined(DISABLE_BOUNDS_CHECKS)
        if (AERON_COND_EXPECT(length > static_cast<std::size_t>(std::numeric_limits<aeron::util::index_t>::max()), true))
        {
            throw aeron::util::OutOfBoundsException(
                aeron::util::strPrintf("length out of bounds[%p]: length=%lld", this, static_cast<long long>(length)),
                SOURCEINFO);
        }
#endif

        m_buffer = buffer;
        m_length = static_cast<length_t>(length);
    }

    /**
     * Wrap an existing AtomicBuffer.
     *
     * @param buffer from which the address and length are used.
     */
    inline void wrap(const AgronaAtomicBuffer &buffer)
    {
        m_buffer = buffer.m_buffer;
        m_length = buffer.m_length;
    }

    template<std::size_t N>
    inline void wrap(std::array<std::uint8_t, N> &buffer)
    {
        static_assert(
            N <= std::numeric_limits<aeron::util::index_t>::max(),
            "requires the array to have a size that fits in an index_t");

        m_buffer = buffer.data();
        m_length = static_cast<length_t>(N);
    }

    /**
     * The capacity of the underlying buffer.
     *
     * @return the capacity of the underlying buffer.
     */
    inline aeron::util::index_t capacity() const
    {
        return static_cast<aeron::util::index_t>(m_length);
    }

    /**
     * Update the capacity of the underlying buffer.
     *
     * @param length to be used for the new capacity.
     */
    inline void capacity(std::size_t length)
    {
#if !defined(DISABLE_BOUNDS_CHECKS)
        if (AERON_COND_EXPECT(length > static_cast<std::size_t>(std::numeric_limits<aeron::util::index_t>::max()), true))
        {
            throw aeron::util::OutOfBoundsException(
                aeron::util::strPrintf("length out of bounds[%p]: length=%lld", this, static_cast<long long>(length)),
                SOURCEINFO);
        }
#endif

        m_length = static_cast<length_t>(length);
    }

    /**
     * Get a pointer to the underlying buffer.
     *
     * @return a pointer to the underlying buffer
     */
    inline std::uint8_t *buffer() const
    {
        return m_buffer;
    }

    inline char *sbeData() const
    {
        return reinterpret_cast<char *>(m_buffer);
    }

    template <typename struct_t>
    struct_t &overlayStruct()
    {
        boundsCheck(0, sizeof(struct_t));
        return *reinterpret_cast<struct_t *>(m_buffer);
    }

    template <typename struct_t>
    struct_t &overlayStruct(aeron::util::index_t offset)
    {
        boundsCheck(offset, sizeof(struct_t));
        return *reinterpret_cast<struct_t *>(m_buffer + offset);
    }

    template <typename struct_t>
    const struct_t &overlayStruct(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(struct_t));
        return *reinterpret_cast<struct_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putInt64(aeron::util::index_t offset, std::int64_t v)
    {
        boundsCheck(offset, sizeof(std::int64_t));
        *reinterpret_cast<std::int64_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL void putFloat(aeron::util::index_t offset, std::float_t v)
    {
        boundsCheck(offset, sizeof(std::float_t));
        *reinterpret_cast<std::float_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL std::int64_t getInt64(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int64_t));
        return *reinterpret_cast<std::int64_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL std::uint64_t getUInt64(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::uint64_t));
        return *reinterpret_cast<std::uint64_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putUInt64(aeron::util::index_t offset, std::uint64_t v)
    {
        boundsCheck(offset, sizeof(std::uint64_t));
        *reinterpret_cast<std::uint64_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL std::uint32_t getUInt32(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::uint32_t));
        return *reinterpret_cast<std::uint32_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putUInt32(aeron::util::index_t offset, std::uint32_t v)
    {
        boundsCheck(offset, sizeof(std::uint32_t));
        *reinterpret_cast<std::uint32_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL std::float_t getFloat(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::float_t));
        return *reinterpret_cast<std::float_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL std::float_t getFloat_BE(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::float_t));

        uint8_t bytes[4];
        std::memcpy(bytes, m_buffer + offset, 4);

        uint32_t intRep =
            (static_cast<uint32_t>(bytes[0]) << 24) |
            (static_cast<uint32_t>(bytes[1]) << 16) |
            (static_cast<uint32_t>(bytes[2]) << 8) |
            (static_cast<uint32_t>(bytes[3]));

        float value;
        std::memcpy(&value, &intRep, sizeof(float));
        return value;
    }

    // Write float in big-endian order
    inline COND_MOCK_VIRTUAL void putFloat_BE(aeron::util::index_t offset, std::float_t v)
    {
        boundsCheck(offset, sizeof(std::float_t));

        uint32_t intRep;
        std::memcpy(&intRep, &v, sizeof(float));

        m_buffer[offset + 0] = static_cast<uint8_t>((intRep >> 24) & 0xFF);
        m_buffer[offset + 1] = static_cast<uint8_t>((intRep >> 16) & 0xFF);
        m_buffer[offset + 2] = static_cast<uint8_t>((intRep >> 8) & 0xFF);
        m_buffer[offset + 3] = static_cast<uint8_t>(intRep & 0xFF);
    }

    inline COND_MOCK_VIRTUAL void putInt32(aeron::util::index_t offset, std::int32_t v)
    {
        boundsCheck(offset, sizeof(std::int32_t));
        *reinterpret_cast<std::int32_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL void putInt32_BE(aeron::util::index_t offset, std::int32_t v)
    {
        boundsCheck(offset, sizeof(std::int32_t));

        uint8_t* ptr = reinterpret_cast<uint8_t*>(m_buffer + offset);
        uint32_t uv = static_cast<uint32_t>(v);

        ptr[0] = static_cast<uint8_t>((uv >> 24) & 0xFF);
        ptr[1] = static_cast<uint8_t>((uv >> 16) & 0xFF);
        ptr[2] = static_cast<uint8_t>((uv >> 8) & 0xFF);
        ptr[3] = static_cast<uint8_t>(uv & 0xFF);
    }

    inline COND_MOCK_VIRTUAL std::int32_t getInt32(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int32_t));
        return *reinterpret_cast<std::int32_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL std::int32_t getInt32_BE(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int32_t));

        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(m_buffer + offset);
        uint32_t uv = (static_cast<uint32_t>(ptr[0]) << 24) |
                      (static_cast<uint32_t>(ptr[1]) << 16) |
                      (static_cast<uint32_t>(ptr[2]) << 8)  |
                      static_cast<uint32_t>(ptr[3]);

        return static_cast<std::int32_t>(uv);
    }

    inline COND_MOCK_VIRTUAL std::int16_t getInt16(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int16_t));
        return *reinterpret_cast<std::int16_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putInt16(aeron::util::index_t offset, std::int16_t v)
    {
        boundsCheck(offset, sizeof(std::int16_t));
        *reinterpret_cast<std::int16_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL std::uint16_t getUInt16(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::uint16_t));
        return *reinterpret_cast<std::uint16_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putUInt16(aeron::util::index_t offset, std::uint16_t v)
    {
        boundsCheck(offset, sizeof(std::uint16_t));
        *reinterpret_cast<std::uint16_t *>(m_buffer + offset) = v;
    }

    inline std::int8_t getInt8(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int8_t));
        return *reinterpret_cast<std::int8_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putInt8(aeron::util::index_t offset, std::int8_t v)
    {
        boundsCheck(offset, sizeof(std::int8_t));
        *reinterpret_cast<std::int8_t *>(m_buffer + offset) = v;
    }

    inline std::uint8_t getUInt8(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::uint8_t));
        return *reinterpret_cast<std::uint8_t *>(m_buffer + offset);
    }

    inline COND_MOCK_VIRTUAL void putUInt8(aeron::util::index_t offset, std::uint8_t v)
    {
        boundsCheck(offset, sizeof(std::uint8_t));
        *reinterpret_cast<std::uint8_t *>(m_buffer + offset) = v;
    }

    inline COND_MOCK_VIRTUAL void putInt64Ordered(aeron::util::index_t offset, std::int64_t v)
    {
        boundsCheck(offset, sizeof(std::int64_t));
        aeron::concurrent::atomic::putInt64Ordered((volatile std::int64_t *)(m_buffer + offset), v);
    }

    inline COND_MOCK_VIRTUAL std::int64_t getInt64Volatile(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int64_t));
        return aeron::concurrent::atomic::getInt64Volatile((volatile std::int64_t *)(m_buffer + offset));
    }

    inline COND_MOCK_VIRTUAL void putInt32Ordered(aeron::util::index_t offset, std::int32_t v)
    {
        boundsCheck(offset, sizeof(std::int32_t));
        aeron::concurrent::atomic::putInt32Ordered((volatile std::int32_t *)(m_buffer + offset), v);
    }

    inline COND_MOCK_VIRTUAL std::int32_t getInt32Volatile(aeron::util::index_t offset) const
    {
        boundsCheck(offset, sizeof(std::int32_t));
        return aeron::concurrent::atomic::getInt32Volatile((volatile std::int32_t *)(m_buffer + offset));
    }

    inline void putInt64Atomic(aeron::util::index_t offset, std::int64_t v)
    {
        boundsCheck(offset, sizeof(std::int64_t));
        aeron::concurrent::atomic::putInt64Atomic((volatile std::int64_t *)(m_buffer + offset), v);
    }

    inline void putInt32Atomic(aeron::util::index_t offset, std::int32_t v)
    {
        boundsCheck(offset, sizeof(std::int32_t));
        aeron::concurrent::atomic::putInt32Atomic((volatile std::int32_t *)(m_buffer + offset), v);
    }

    /**
     * Single threaded increment with release semantics.
     *
     * @param offset in the buffer of the word.
     * @param delta  for to be applied to the value.
     */
    inline void addInt64Ordered(aeron::util::index_t offset, std::int64_t delta)
    {
        boundsCheck(offset, sizeof(std::int64_t));

        const std::int64_t value = getInt64(offset);
        aeron::concurrent::atomic::putInt64Ordered((volatile std::int64_t *)(m_buffer + offset), value + delta);
    }

    inline bool compareAndSetInt64(aeron::util::index_t offset, std::int64_t expectedValue, std::int64_t updateValue)
    {
        boundsCheck(offset, sizeof(std::int64_t));
        std::int64_t original = aeron::concurrent::atomic::cmpxchg((volatile std::int64_t *)(m_buffer + offset), expectedValue, updateValue);
        return original == expectedValue;
    }

    inline std::int64_t getAndSetInt64(aeron::util::index_t offset, std::int64_t value)
    {
        boundsCheck(offset, sizeof(std::int64_t));
        return aeron::concurrent::atomic::xchg((volatile std::int64_t *)(m_buffer + offset), value);
    }

    inline std::int32_t getAndSetInt32(aeron::util::index_t offset, std::int32_t value)
    {
        boundsCheck(offset, sizeof(std::int32_t));
        return aeron::concurrent::atomic::xchg((volatile std::int32_t *)(m_buffer + offset), value);
    }

    /**
     * Multi threaded increment.
     *
     * @param offset in the buffer of the word.
     * @param delta  for to be applied to the value.
     * @return the value before applying the delta.
     */
    inline COND_MOCK_VIRTUAL std::int64_t getAndAddInt64(aeron::util::index_t offset, std::int64_t delta)
    {
        boundsCheck(offset, sizeof(std::int64_t));
        return aeron::concurrent::atomic::getAndAddInt64((volatile std::int64_t *)(m_buffer + offset), delta);
    }

    /**
     * Single threaded add with release semantics.
     *
     * @param offset in the buffer of the word.
     * @param delta  for to be applied to the value.
     */
    inline void addInt32Ordered(aeron::util::index_t offset, std::int32_t delta)
    {
        boundsCheck(offset, sizeof(std::int32_t));

        const std::int32_t value = getInt32(offset);
        aeron::concurrent::atomic::putInt32Ordered((volatile std::int32_t *)(m_buffer + offset), value + delta);
    }

    inline bool compareAndSetInt32(aeron::util::index_t offset, std::int32_t expectedValue, std::int32_t updateValue)
    {
        boundsCheck(offset, sizeof(std::int32_t));
        std::int32_t original = aeron::concurrent::atomic::cmpxchg((volatile std::int32_t *)(m_buffer + offset), expectedValue, updateValue);
        return original == expectedValue;
    }

    /**
     * Multi threaded increment.
     *
     * @param offset in the buffer of the word.
     * @param delta  for to be applied to the value.
     * @return the value before applying the delta.
     */
    inline COND_MOCK_VIRTUAL std::int32_t getAndAddInt32(aeron::util::index_t offset, std::int32_t delta)
    {
        boundsCheck(offset, sizeof(std::int32_t));
        return aeron::concurrent::atomic::getAndAddInt32((volatile std::int32_t *)(m_buffer + offset), delta);
    }

    inline COND_MOCK_VIRTUAL void putBytes(
        aeron::util::index_t index, const AgronaAtomicBuffer &srcBuffer, aeron::util::index_t srcIndex, aeron::util::index_t length)
    {
        boundsCheck(index, length);
        srcBuffer.boundsCheck(srcIndex, length);
        ::memcpy(m_buffer + index, srcBuffer.m_buffer + srcIndex, static_cast<std::size_t>(length));
    }

    inline COND_MOCK_VIRTUAL void putBytes(aeron::util::index_t index, const std::uint8_t *srcBuffer, aeron::util::index_t length)
    {
        boundsCheck(index, length);
        ::memcpy(m_buffer + index, srcBuffer, static_cast<std::size_t>(length));
    }

    inline void getBytes(aeron::util::index_t index, std::uint8_t *dst, aeron::util::index_t length) const
    {
        boundsCheck(index, length);
        ::memcpy(dst, m_buffer + index, static_cast<std::size_t>(length));
    }

    inline void setMemory(aeron::util::index_t offset , std::size_t length, std::uint8_t value)
    {
        boundsCheck(offset, length);
        ::memset(m_buffer + offset, value, length);
    }

    inline std::string getString(aeron::util::index_t offset) const
    {
        std::int32_t length;

        boundsCheck(offset, sizeof(length));
        ::memcpy(reinterpret_cast<char *>(&length), m_buffer + offset, sizeof(length));

        return getStringWithoutLength(offset + sizeof(std::int32_t), static_cast<std::size_t>(length));
    }

    inline std::size_t getString(aeron::util::index_t offset, char* target) const
    {
        std::int32_t length;

        boundsCheck(offset, sizeof(length));
        ::memcpy(reinterpret_cast<char *>(&length), m_buffer + offset, sizeof(length));

        return getStringWithoutLength(offset + sizeof(std::int32_t), static_cast<std::size_t>(length), target);
    }

    inline std::string getStringWithoutLength(aeron::util::index_t offset, std::size_t length) const
    {
        boundsCheck(offset, length);
        return { m_buffer + static_cast<std::size_t>(offset), m_buffer + static_cast<std::size_t>(offset) + length };
    }

    inline std::size_t getStringWithoutLength(aeron::util::index_t offset, std::size_t length, char* target) const
    {
        boundsCheck(offset, length); // Ensure offset and length are within buffer bounds
        std::memcpy(target, m_buffer + static_cast<std::size_t>(offset), length);
        return length;
    }

    inline std::int32_t getStringLength(aeron::util::index_t offset) const
    {
        std::int32_t length;

        boundsCheck(offset, sizeof(length));
        ::memcpy(reinterpret_cast<char *>(&length), m_buffer + offset, sizeof(length));

        return length;
    }

    std::int32_t putString(aeron::util::index_t offset, const std::string &value)
    {
        auto length = static_cast<std::int32_t>(value.length());

        boundsCheck(offset, value.length() + sizeof(length));

        ::memcpy(m_buffer + offset, reinterpret_cast<const char *>(&length), sizeof(length));
        ::memcpy(m_buffer + offset + sizeof(length), value.c_str(), value.length());

        return static_cast<std::int32_t>(sizeof(std::int32_t)) + length;
    }

    std::int32_t putString(aeron::util::index_t offset, const std::string_view &value)
    {
        auto length = static_cast<std::int32_t>(value.length());

        boundsCheck(offset, value.length() + sizeof(length));

        ::memcpy(m_buffer + offset, reinterpret_cast<const char *>(&length), sizeof(length));
        ::memcpy(m_buffer + offset + sizeof(length), value.data(), value.length());

        return static_cast<std::int32_t>(sizeof(std::int32_t)) + length;
    }

    std::int32_t COND_MOCK_VIRTUAL putStringWithoutLength(aeron::util::index_t offset, const std::string &value)
    {
        boundsCheck(offset, value.length());
        ::memcpy(m_buffer + offset, value.c_str(), value.length());
        return static_cast<std::int32_t>(value.length());
    }

    static AgronaAtomicBuffer mmap(std::size_t size, const char* backingFile) {
        int fd = ::open(backingFile, O_RDWR | O_CREAT, 0666);
        if (fd < 0) {
            throw std::runtime_error(std::string("Failed to open file: ") + std::strerror(errno));
        }

        // Ensure file is correct size
        if (::ftruncate(fd, size) < 0) {
            ::close(fd);
            throw std::runtime_error(std::string("Failed to ftruncate: ") + std::strerror(errno));
        }

        void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);
        if (addr == MAP_FAILED) {
            throw std::runtime_error(std::string("Failed to mmap: ") + std::strerror(errno));
        }

        return AgronaAtomicBuffer(static_cast<std::uint8_t*>(addr), size);
    }

#if !defined(DISABLE_BOUNDS_CHECKS)
    inline void boundsCheck(aeron::util::index_t index, std::uint64_t length) const
    {
        if (AERON_COND_EXPECT(index < 0 || (static_cast<std::uint64_t>(m_length) - index) < length, false))
        {
            throw aeron::util::OutOfBoundsException(
                aeron::util::strPrintf(
                    "index out of bounds[%p]: index=%lld + %lld, capacity=%lld",
                    this, static_cast<long long>(index), length, static_cast<long long>(m_length)),
                SOURCEINFO);
        }
    }
#else
    inline void boundsCheck(util::index_t, std::uint64_t) const {}
#endif

private:
    // The internal length type used by the atomic buffer
    typedef std::uint32_t length_t;

    std::uint8_t *m_buffer = nullptr;
    length_t m_length = 0;
};

}}
