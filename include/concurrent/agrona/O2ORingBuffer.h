#pragma once

#include <functional>
#include "Aeron.h"
#include "concurrent/agrona/AgronaAtomicBuffer.h"
#include "concurrent/agrona/AgronaRingBufferDescriptor.h"
#include "concurrent/agrona/AgronaRecordDescriptor.h"
#include "flow/ScopeExit.h"

namespace agrona::concurrent::ringbuffer {
    class O2ORingBuffer {
    public:
        static constexpr aeron::util::index_t MIN_CAPACITY = AgronaRecordDescriptor::HEADER_LENGTH * 2;
        static constexpr aeron::util::index_t INSUFFICIENT_CAPACITY = -2;

        explicit O2ORingBuffer(std::uint8_t *buffer, std::size_t length) : backingBuffer_(buffer, length),
                                                                           m_buffer(backingBuffer_) {
            m_capacity = m_buffer.capacity() - AgronaRingBufferDescriptor::TRAILER_LENGTH;

            AgronaRingBufferDescriptor::checkCapacity(m_capacity);

            m_maxMsgLength = m_capacity / 8;

            m_tailPositionIndex = m_capacity + AgronaRingBufferDescriptor::TAIL_POSITION_OFFSET;
            m_headCachePositionIndex = m_capacity + AgronaRingBufferDescriptor::HEAD_CACHE_POSITION_OFFSET;
            m_headPositionIndex = m_capacity + AgronaRingBufferDescriptor::HEAD_POSITION_OFFSET;
            m_correlationIdCounterIndex =
                    m_capacity + AgronaRingBufferDescriptor::CORRELATION_COUNTER_OFFSET;
            m_consumerHeartbeatIndex = m_capacity + AgronaRingBufferDescriptor::CONSUMER_HEARTBEAT_OFFSET;
        }

        O2ORingBuffer(const O2ORingBuffer &) = delete;

        O2ORingBuffer &operator =(const O2ORingBuffer &) = delete;

        [[nodiscard]] inline aeron::util::index_t capacity() const {
            return m_capacity;
        }

        [[nodiscard]] aeron::util::index_t tryClaim(std::int32_t msgTypeId, size_t length) const {
            AgronaRecordDescriptor::checkMsgTypeId(msgTypeId);
            checkMsgLength(length);

            AgronaAtomicBuffer& buffer = m_buffer;
            const aeron::util::index_t recordLength = length + AgronaRecordDescriptor::HEADER_LENGTH;
            const aeron::util::index_t recordIndex = claimCapacity(buffer, recordLength);

            if (INSUFFICIENT_CAPACITY == recordIndex) {
                return recordIndex;
            }

            m_buffer.putInt32Ordered(AgronaRecordDescriptor::lengthOffset(recordIndex), -recordLength);
            std::atomic_thread_fence(std::memory_order_release);
            m_buffer.putInt32(AgronaRecordDescriptor::typeOffset(recordIndex), msgTypeId);

            return AgronaRecordDescriptor::encodedMsgOffset(recordIndex);
        }

        void commit(aeron::util::index_t index) const {
            const aeron::util::index_t recordIndex = computeRecordIndex(index);
            AgronaAtomicBuffer& buffer = m_buffer;
            const std::int32_t recordLength = verifyClaimedSpaceNotReleased(recordIndex);
            buffer.putInt32Ordered(AgronaRecordDescriptor::lengthOffset(recordIndex), -recordLength);
        }

        void abort(aeron::util::index_t index) const {
            const aeron::util::index_t recordIndex = computeRecordIndex(index);
            AgronaAtomicBuffer& buffer = m_buffer;
            const std::int32_t recordLength = verifyClaimedSpaceNotReleased(recordIndex);

            buffer.putInt32(AgronaRecordDescriptor::typeOffset(recordIndex),
                              AgronaRecordDescriptor::PADDING_MSG_TYPE_ID);
            buffer.putInt32Ordered(AgronaRecordDescriptor::lengthOffset(recordIndex), -recordLength);
        }

        // BUG-M23 fix: Template for inlining + zero memory on read (match M2ORingBuffer)
        template<typename Handler>
        [[nodiscard]] inline int read(Handler&& handler, int messageCountLimit) const {
            int messagesRead = 0;

            AgronaAtomicBuffer& buffer = m_buffer;
            const int headPositionIndex = m_headPositionIndex;
            const std::int64_t head = buffer.getInt64(headPositionIndex);

            int bytesRead = 0;

            const int capacity = m_capacity;
            const int headIndex = static_cast<std::int32_t>(head) & (capacity - 1);
            const int contiguousBlockLength = capacity - headIndex;

            auto onExit = make_scope_exit([&]() {
                if (bytesRead > 0) {
                    // BUG-M23: Zero consumed region to prevent stale data reads
                    buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                    buffer.putInt64Ordered(headPositionIndex, head + bytesRead);
                }
            });

            while ((bytesRead < contiguousBlockLength) && (messagesRead < messageCountLimit)) {
                const int recordIndex = headIndex + bytesRead;
                const int recordLength = buffer.getInt32Volatile(
                    AgronaRecordDescriptor::lengthOffset(recordIndex));

                if (recordLength <= 0) {
                    break;
                }

                const int alignedLength = align(
                    recordLength, AgronaRecordDescriptor::ALIGNMENT);
                bytesRead += alignedLength;

                const int msgTypeId = buffer.getInt32(
                    AgronaRecordDescriptor::typeOffset(recordIndex));
                if (AgronaRecordDescriptor::PADDING_MSG_TYPE_ID == msgTypeId) {
                    continue;
                }

                handler(
                    msgTypeId,
                    buffer,
                    AgronaRecordDescriptor::encodedMsgOffset(recordIndex),
                    recordLength - AgronaRecordDescriptor::HEADER_LENGTH
                );
                ++messagesRead;
            }

            return messagesRead;
        }

        template<typename Handler>
        [[nodiscard]] inline int controlledRead(Handler&& handler,
                                                int messageCountLimit) const {
            int messagesRead = 0;
            AgronaAtomicBuffer &buffer = m_buffer;
            aeron::util::index_t headPositionIndex = m_headPositionIndex;
            std::int64_t head = buffer.getInt64(headPositionIndex);

            int bytesRead = 0;

            const std::int32_t capacity = m_capacity;
            int headIndex = static_cast<std::int32_t>(head) & (capacity - 1);
            const std::int32_t contiguousBlockLength = capacity - headIndex;

            auto onExit = make_scope_exit([&]() {
                if (bytesRead > 0) {
                    m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
                }
            });

            while ((bytesRead < contiguousBlockLength) && (messagesRead < messageCountLimit)) {
                const std::int32_t recordIndex = headIndex + bytesRead;
                const std::int32_t recordLength = buffer.getInt32Volatile(
                    AgronaRecordDescriptor::lengthOffset(recordIndex));

                if (recordLength <= 0) {
                    break;
                }

                const std::int32_t alignedLength = align(
                    recordLength, AgronaRecordDescriptor::ALIGNMENT);
                bytesRead += alignedLength;

                const std::int32_t messageTypeId = buffer.getInt32(
                    AgronaRecordDescriptor::typeOffset(recordIndex));
                if (messageTypeId == AgronaRecordDescriptor::PADDING_MSG_TYPE_ID) {
                    continue;
                }

                const ControlledAction action = handler(
                    messageTypeId,
                    buffer,
                    AgronaRecordDescriptor::encodedMsgOffset(recordIndex),
                    recordLength - AgronaRecordDescriptor::HEADER_LENGTH);

                if (action == ControlledAction::ABORT) {
                    bytesRead -= alignedLength;
                    break;
                }

                ++messagesRead;

                if (action == ControlledAction::BREAK) {
                    break;
                }

                if (action == ControlledAction::COMMIT) {
                    buffer.putInt64Ordered(headPositionIndex, head + bytesRead);
                    headIndex += bytesRead;
                    head += bytesRead;
                    bytesRead = 0;
                }
            }


            return messagesRead;
        }

        AgronaAtomicBuffer &buffer() {
            return m_buffer;
        }

        uint8_t *innerBuffer() {
            return m_buffer.buffer();
        }

        inline std::int32_t  size() {
            AgronaAtomicBuffer &buffer = m_buffer;
            const std::int32_t headPositionIndex = m_headPositionIndex;
            const std::int32_t tailPositionIndex = m_tailPositionIndex;
            long headBefore;
            long tail;
            long headAfter = buffer.getInt64Volatile(headPositionIndex);

            do
            {
                headBefore = headAfter;
                tail = buffer.getInt64Volatile(tailPositionIndex);
                headAfter = buffer.getInt64Volatile(headPositionIndex);
            }
            while (headAfter != headBefore);

            const std::int64_t size = tail - headAfter;
            if (size < 0) {
                return 0;
            } else if (size > m_capacity) {
                return m_capacity;
            }

            return static_cast<std::int32_t>(size);
        }

    private:
        AgronaAtomicBuffer backingBuffer_;
        AgronaAtomicBuffer &m_buffer;
        aeron::util::index_t m_capacity;
        aeron::util::index_t m_maxMsgLength;
        aeron::util::index_t m_headPositionIndex;
        aeron::util::index_t m_headCachePositionIndex;
        aeron::util::index_t m_tailPositionIndex;
        aeron::util::index_t m_correlationIdCounterIndex;
        aeron::util::index_t m_consumerHeartbeatIndex;

        aeron::util::index_t claimCapacity(AgronaAtomicBuffer &buffer, std::int32_t recordLength) const {
            const std::int32_t alignedRecordLength =
                    align(recordLength, AgronaRecordDescriptor::ALIGNMENT);
            const std::int32_t requiredCapacity =
                    alignedRecordLength + AgronaRecordDescriptor::HEADER_LENGTH;
            const std::int32_t capacity = m_capacity;
            const std::int32_t tailPositionIndex = m_tailPositionIndex;
            const std::int32_t headCachePositionIndex = m_headCachePositionIndex;
            const std::int32_t mask = capacity - 1;

            std::int64_t head = buffer.getInt64(headCachePositionIndex);
            const std::int64_t tail = buffer.getInt64(tailPositionIndex);
            const std::int32_t availableCapacity = capacity - static_cast<std::int32_t>(tail - head);

            if (requiredCapacity > availableCapacity) {
                head = buffer.getInt64Volatile(m_headPositionIndex);

                if (requiredCapacity > (capacity - static_cast<std::int32_t>(tail - head))) {
                    return INSUFFICIENT_CAPACITY;
                }

                buffer.putInt64(headCachePositionIndex, head);
            }

            std::int32_t padding = 0;
            const std::int32_t recordIndex = static_cast<std::int32_t>(tail) & mask;
            const std::int32_t toBufferEndLength = capacity - recordIndex;
            std::int32_t writeIndex = recordIndex;
            std::int64_t nextTail = tail + alignedRecordLength;

            if (alignedRecordLength == toBufferEndLength) {
                buffer.putInt64Ordered(tailPositionIndex, nextTail);
                buffer.putInt64(0, 0L); // pre-zero next message header
                return recordIndex;
            } else if (requiredCapacity > toBufferEndLength) {
                writeIndex = 0;
                std::int32_t headIndex = static_cast<std::int32_t>(head) & mask;

                if (requiredCapacity > headIndex) {
                    head = buffer.getInt64Volatile(m_headPositionIndex);
                    headIndex = static_cast<std::int32_t>(head) & mask;
                    if (requiredCapacity > headIndex) {
                        writeIndex = INSUFFICIENT_CAPACITY;
                        nextTail = tail;
                    }

                    buffer.putInt64(headCachePositionIndex, head);
                }

                padding = toBufferEndLength;
                nextTail += padding;
            }

            buffer.putInt64Ordered(tailPositionIndex, nextTail);

            if (0 != padding) {
                buffer.putInt64(0, 0L);
                buffer.putInt32Ordered(AgronaRecordDescriptor::lengthOffset(recordIndex), -padding);
                std::atomic_thread_fence(std::memory_order_release);
                buffer.putInt32(AgronaRecordDescriptor::typeOffset(recordIndex),
                                AgronaRecordDescriptor::PADDING_MSG_TYPE_ID);
                buffer.putInt32Ordered(AgronaRecordDescriptor::lengthOffset(recordIndex), padding);
            }

            if (INSUFFICIENT_CAPACITY != writeIndex) {
                buffer.putInt64(writeIndex + alignedRecordLength, 0L); // pre-zero next message header
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

        [[nodiscard]] inline aeron::util::index_t computeRecordIndex(aeron::util::index_t index) const {
            const aeron::util::index_t recordIndex = index - AgronaRecordDescriptor::HEADER_LENGTH;
            if (recordIndex < 0 || recordIndex > (m_capacity - AgronaRecordDescriptor::HEADER_LENGTH)) {
                throw aeron::util::IllegalArgumentException("Invalid message index: " + std::to_string(index),
                                                            SOURCEINFO);
            }
            return recordIndex;
        }

        [[nodiscard]] inline std::int32_t verifyClaimedSpaceNotReleased(aeron::util::index_t recordIndex) const {
            const std::int32_t recordLength = m_buffer.getInt32(
                AgronaRecordDescriptor::lengthOffset(recordIndex));
            if (recordLength < 0) {
                return recordLength;
            }

            const std::int32_t typeId = m_buffer.getInt32(AgronaRecordDescriptor::typeOffset(recordIndex));
            throw aeron::util::IllegalStateException(
                "claimed space previously " + std::string(
                    typeId == AgronaRecordDescriptor::PADDING_MSG_TYPE_ID
                        ? "aborted"
                        : "committed"),
                SOURCEINFO);
        }
    };
}
