#pragma once

#include <climits>
#include <functional>
#include <flow/ScopeExit.h>

#include "util/LangUtil.h"
#include "concurrent/agrona/AgronaRingBufferDescriptor.h"
#include "concurrent/agrona/AgronaAtomicBuffer.h"
#include "concurrent/ringbuffer/RecordDescriptor.h"


namespace agrona::concurrent::ringbuffer {
    class M2ORingBuffer {
    public:
        explicit M2ORingBuffer(std::uint8_t *buffer, std::size_t length) : backingBuffer_(buffer, length),
                                                                           m_buffer(backingBuffer_) {
            m_capacity = m_buffer.capacity() - AgronaRingBufferDescriptor::TRAILER_LENGTH;

            AgronaRingBufferDescriptor::checkCapacity(m_capacity);

            m_maxMsgLength = m_capacity / 8;

            m_tailPositionIndex = m_capacity + AgronaRingBufferDescriptor::TAIL_POSITION_OFFSET;
            m_headCachePositionIndex = m_capacity + AgronaRingBufferDescriptor::HEAD_CACHE_POSITION_OFFSET;
            m_headPositionIndex = m_capacity + AgronaRingBufferDescriptor::HEAD_POSITION_OFFSET;
            m_correlationIdCounterIndex = m_capacity + AgronaRingBufferDescriptor::CORRELATION_COUNTER_OFFSET;
            m_consumerHeartbeatIndex = m_capacity + AgronaRingBufferDescriptor::CONSUMER_HEARTBEAT_OFFSET;
        }

        M2ORingBuffer(const M2ORingBuffer &) = delete;

        M2ORingBuffer &operator =(const M2ORingBuffer &) = delete;

        inline aeron::util::index_t capacity() const {
            return m_capacity;
        }

        inline aeron::util::index_t tryClaim(std::int32_t msgTypeId, size_t length) {
            aeron::concurrent::ringbuffer::RecordDescriptor::checkMsgTypeId(msgTypeId);
            checkMsgLength(length);

            AgronaAtomicBuffer& buffer = m_buffer;
            const aeron::util::index_t recordLength =
                length + aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH;
            aeron::util::index_t recordIndex = claimCapacity(buffer, recordLength);

            if (INSUFFICIENT_CAPACITY == recordIndex) {
                return recordIndex;
            }

            buffer.putInt32Ordered(
                aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
            std::atomic_thread_fence(std::memory_order_release);
            buffer.putInt32(
                aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex),
                msgTypeId);

            return aeron::concurrent::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex);
        }

        inline void commit(aeron::util::index_t index) {
            const aeron::util::index_t recordIndex = computeRecordIndex(index);
            AgronaAtomicBuffer& buffer = m_buffer;
            const std::int32_t recordLength = verifyClaimedSpaceNotReleased(buffer, recordIndex);

            buffer.putInt32Ordered(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex),
                                     -recordLength);
        }

        inline void abort(aeron::util::index_t index) {
            const aeron::util::index_t recordIndex = computeRecordIndex(index);
            AgronaAtomicBuffer& buffer = m_buffer;
            const std::int32_t recordLength = verifyClaimedSpaceNotReleased(buffer, recordIndex);

            buffer.putInt32(aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex),
                              aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID);
            buffer.putInt32Ordered(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex),
                                     -recordLength);
        }

        // PERF-P25: Templated version for inlining (avoids std::function overhead)
        template<typename Handler>
        int read(Handler&& handler, int messageCountLimit) {
            int messagesRead = 0;

            AgronaAtomicBuffer& buffer = m_buffer;
            const int headPositionIndex = m_headPositionIndex;
            const std::int64_t head = buffer.getInt64(headPositionIndex);

            const int capacity = m_capacity;
            const int headIndex = static_cast<std::int32_t>(head) & (capacity - 1);
            const int maxBlockLength = capacity - headIndex;
            int bytesRead = 0;

            auto cleanup = aeron::util::InvokeOnScopeExit{
                [&]() {
                    if (bytesRead != 0) {
                        buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                        buffer.putInt64Ordered(headPositionIndex, head + bytesRead);
                    }
                }
            };

            while ((bytesRead < maxBlockLength) && (messagesRead < messageCountLimit)) {
                const int recordIndex = headIndex + bytesRead;
                const int recordLength = buffer.getInt32Volatile(
                    aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex));

                if (recordLength <= 0) {
                    break;
                }

                bytesRead += align(
                    recordLength, aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT);

                const int msgTypeId = buffer.getInt32(
                    aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex));
                if (aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID == msgTypeId) {
                    continue;
                }

                handler(
                    msgTypeId,
                    buffer,
                    aeron::concurrent::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex),
                    recordLength - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH);
                ++messagesRead;
            }

            return messagesRead;
        }

        // PERF-P25: Templated version for inlining (avoids std::function overhead)
        template<typename Handler>
        int controlledRead(Handler&& handler, int messageCountLimit = INT_MAX) {
            int messagesRead = 0;

            AgronaAtomicBuffer &buffer = m_buffer;
            const int headPositionIndex = m_headPositionIndex;
            std::int64_t head = buffer.getInt64(headPositionIndex);

            const int capacity = m_capacity;
            int headIndex = static_cast<std::int32_t>(head) & (capacity - 1);
            const int maxBlockLength = capacity - headIndex;
            int bytesRead = 0;

            // Simulate Java's `finally` block
            auto onExit = make_scope_exit([&]() {
                if (bytesRead > 0) {
                    buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                    buffer.putInt64Ordered(headPositionIndex, head + bytesRead);
                }
            });

            while ((bytesRead < maxBlockLength) && (messagesRead < messageCountLimit)) {
                const int recordIndex = headIndex + bytesRead;
                const std::int64_t header = buffer.getInt64Volatile(recordIndex);
                const int recordLength = aeron::concurrent::ringbuffer::RecordDescriptor::recordLength(header);

                if (recordLength <= 0) {
                    break;
                }

                const int alignedLength = aeron::util::BitUtil::align(
                    recordLength, aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT);
                bytesRead += alignedLength;

                const std::int32_t messageTypeId = buffer.getInt32(
                    aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex));
                if (messageTypeId == aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID) {
                    continue;
                }

                ControlledAction action = handler(
                    messageTypeId,
                    buffer,
                    aeron::concurrent::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex),
                    recordLength - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH);

                if (action == ControlledAction::ABORT) {
                    bytesRead -= alignedLength;
                    break;
                }

                ++messagesRead;

                if (action == ControlledAction::BREAK) {
                    break;
                }

                if (action == ControlledAction::COMMIT) {
                    buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                    buffer.putInt64Ordered(headPositionIndex, head + bytesRead);
                    headIndex += bytesRead;
                    head += bytesRead;
                    bytesRead = 0;
                }
            }

            return messagesRead;
        }


        template<typename Handler>
        inline int read(Handler&& handler) {
            return read(handler, INT_MAX);
        }

        inline aeron::util::index_t maxMsgLength() const {
            return m_maxMsgLength;
        }

        inline std::int64_t nextCorrelationId() {
            return m_buffer.getAndAddInt64(m_correlationIdCounterIndex, 1);
        }

        inline void consumerHeartbeatTime(std::int64_t time) {
            m_buffer.putInt64Ordered(m_consumerHeartbeatIndex, time);
        }

        inline std::int64_t consumerHeartbeatTime() const {
            return m_buffer.getInt64Volatile(m_consumerHeartbeatIndex);
        }

        inline std::int64_t producerPosition() const {
            return m_buffer.getInt64Volatile(m_tailPositionIndex);
        }

        inline std::int64_t consumerPosition() const {
            return m_buffer.getInt64Volatile(m_headPositionIndex);
        }

        inline std::int32_t size() const {
            AgronaAtomicBuffer &buffer = m_buffer;
            const std::int32_t headPositionIndex = m_headPositionIndex;
            const std::int32_t tailPositionIndex = m_tailPositionIndex;
            std::int64_t headBefore;
            std::int64_t tail;
            std::int64_t headAfter = buffer.getInt64Volatile(headPositionIndex);

            do {
                headBefore = headAfter;
                tail = buffer.getInt64Volatile(tailPositionIndex);
                headAfter = buffer.getInt64Volatile(headPositionIndex);
            } while (headAfter != headBefore);

            const std::int64_t size = tail - headAfter;
            if (size < 0) {
                return 0;
            } else if (size > m_capacity) {
                return m_capacity;
            }

            return static_cast<std::int32_t>(size);
        }

        AgronaAtomicBuffer &buffer() {
            return m_buffer;
        }

        uint8_t *innerBuffer() {
            return m_buffer.buffer();
        }

    private:
        static constexpr aeron::util::index_t INSUFFICIENT_CAPACITY = -2;

        AgronaAtomicBuffer backingBuffer_;
        AgronaAtomicBuffer &m_buffer;
        aeron::util::index_t m_capacity;
        aeron::util::index_t m_maxMsgLength;
        aeron::util::index_t m_headPositionIndex;
        aeron::util::index_t m_headCachePositionIndex;
        aeron::util::index_t m_tailPositionIndex;
        aeron::util::index_t m_correlationIdCounterIndex;
        aeron::util::index_t m_consumerHeartbeatIndex;

        [[nodiscard]] inline aeron::util::index_t computeRecordIndex(aeron::util::index_t index) const {
            aeron::util::index_t recordIndex = index - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH;
            if (recordIndex < 0 || recordIndex > (
                    m_capacity - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH)) {
                throw aeron::util::IllegalArgumentException("Invalid message index: " + std::to_string(index),
                                                            SOURCEINFO);
            }

            return recordIndex;
        }

        [[nodiscard]] inline std::int32_t verifyClaimedSpaceNotReleased(AgronaAtomicBuffer& buffer, aeron::util::index_t recordIndex) const {
            const std::int32_t recordLength = buffer.getInt32(
                aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex));

            if (recordLength < 0) {
                return recordLength;
            }

            const std::int32_t typeId = buffer.getInt32(
                aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex));
            throw aeron::util::IllegalStateException(
                "Claimed space previously " + std::string(
                    typeId == aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID
                        ? "aborted"
                        : "committed"),
                SOURCEINFO);
        }

        aeron::util::index_t claimCapacity(AgronaAtomicBuffer &buffer, aeron::util::index_t recordLength) {
            const aeron::util::index_t requiredCapacity = align(
                recordLength, aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT);
            const aeron::util::index_t capacity = m_capacity;
            const aeron::util::index_t tailPositionIndex = m_tailPositionIndex;
            const aeron::util::index_t headCachePositionIndex = m_headCachePositionIndex;
            const aeron::util::index_t mask = capacity - 1;

            std::int64_t head = buffer.getInt64Volatile(headCachePositionIndex);

            std::int64_t tail;
            std::int64_t newTail;
            aeron::util::index_t tailIndex;
            aeron::util::index_t padding;
            aeron::util::index_t writeIndex;

            do {
                tail = buffer.getInt64Volatile(tailPositionIndex);
                const aeron::util::index_t availableCapacity =
                        capacity - static_cast<aeron::util::index_t>(tail - head);

                if (requiredCapacity > availableCapacity) {
                    head = buffer.getInt64Volatile(m_headPositionIndex);

                    if (requiredCapacity > (capacity - static_cast<aeron::util::index_t>(tail - head))) {
                        return INSUFFICIENT_CAPACITY;
                    }

                    buffer.putInt64Ordered(headCachePositionIndex, head);
                }

                newTail = tail + requiredCapacity;

                padding = 0;
                tailIndex = static_cast<aeron::util::index_t>(tail) & mask;
                writeIndex = tailIndex;
                const aeron::util::index_t toBufferEndLength = capacity - tailIndex;

                if (requiredCapacity > toBufferEndLength) {
                    aeron::util::index_t headIndex = static_cast<aeron::util::index_t>(head) & mask;
                    writeIndex = 0;

                    if (requiredCapacity > headIndex) {
                        head = buffer.getInt64Volatile(m_headPositionIndex);
                        headIndex = static_cast<aeron::util::index_t>(head) & mask;

                        if (requiredCapacity > headIndex) {
                            writeIndex = INSUFFICIENT_CAPACITY;
                            newTail = tail; // Do not claim any actual space, only pad to the buffer end
                        }

                        buffer.putInt64Ordered(headCachePositionIndex, head);
                    }

                    padding = toBufferEndLength;
                    newTail += padding;
                }
            } while (!buffer.compareAndSetInt64(tailPositionIndex, tail, newTail));

            if (0 != padding) {
                buffer.putInt32Ordered(
                    aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(tailIndex), -padding);
                std::atomic_thread_fence(std::memory_order_release);
                buffer.putInt32(
                    aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(tailIndex),
                    aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID);
                buffer.putInt32Ordered(
                    aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(tailIndex), padding);
            }

            return writeIndex;
        }

        inline void checkMsgLength(aeron::util::index_t length) const {
            if (length < 0) {
                throw aeron::util::IllegalArgumentException("invalid message length= " +  std::to_string(length), SOURCEINFO);
            }
            if (length > m_maxMsgLength) {
                throw aeron::util::IllegalArgumentException(
                    "encoded message exceeds maxMsgLength of " + std::to_string(m_maxMsgLength) +
                    " length=" + std::to_string(length), SOURCEINFO);
            }
        }

        inline static bool scanBackToConfirmStillZeroed(const AgronaAtomicBuffer &buffer, std::int32_t from,
                                                        std::int32_t limit) {
            std::int32_t i = from - aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT;
            bool allZeroes = true;

            while (i >= limit) {
                if (0 != buffer.getInt32Volatile(i)) {
                    allZeroes = false;
                    break;
                }

                i -= aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT;
            }

            return allZeroes;
        }
    };
}
