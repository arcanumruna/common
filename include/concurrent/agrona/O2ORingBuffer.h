#pragma once

#include <functional>
#include "aeron.h"
#include "concurrent/agrona/AgronaAtomicBuffer.h"
#include "concurrent/ringbuffer/RingBufferDescriptor.h"
#include "concurrent/ringbuffer/RecordDescriptor.h"

namespace agrona::concurrent::ringbuffer {

    class O2ORingBuffer {
    public:
        static constexpr aeron::util::index_t MIN_CAPACITY = aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH * 2;
        static constexpr aeron::util::index_t INSUFFICIENT_CAPACITY = -2;

        explicit O2ORingBuffer(AgronaAtomicBuffer &buffer) : m_buffer(buffer) {
            m_capacity = buffer.capacity() - aeron::ringbuffer::RingBufferDescriptor::TRAILER_LENGTH;

            aeron::ringbuffer::RingBufferDescriptor::checkCapacity(m_capacity);

            m_maxMsgLength = m_capacity / 8;

            m_tailPositionIndex = m_capacity + aeron::ringbuffer::RingBufferDescriptor::TAIL_POSITION_OFFSET;
            m_headCachePositionIndex = m_capacity + aeron::ringbuffer::RingBufferDescriptor::HEAD_CACHE_POSITION_OFFSET;
            m_headPositionIndex = m_capacity + aeron::ringbuffer::RingBufferDescriptor::HEAD_POSITION_OFFSET;
            m_correlationIdCounterIndex = m_capacity + aeron::ringbuffer::RingBufferDescriptor::CORRELATION_COUNTER_OFFSET;
            m_consumerHeartbeatIndex = m_capacity + aeron::ringbuffer::RingBufferDescriptor::CONSUMER_HEARTBEAT_OFFSET;
        }

        O2ORingBuffer(const O2ORingBuffer &) = delete;

        O2ORingBuffer &operator =(const O2ORingBuffer &) = delete;

        [[nodiscard]] inline aeron::util::index_t capacity() const {
            return m_capacity;
        }

        [[nodiscard]] bool write(std::int32_t msgTypeId, const AgronaAtomicBuffer &srcBuffer,
                                 aeron::util::index_t srcIndex, aeron::util::index_t length) const {
            aeron::ringbuffer::RecordDescriptor::checkMsgTypeId(msgTypeId);
            checkMsgLength(length);

            const aeron::util::index_t recordLength = length + aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH;
            const aeron::util::index_t alignedRecordLength = aeron::util::BitUtil::align(recordLength, aeron::ringbuffer::RecordDescriptor::ALIGNMENT);
            const aeron::util::index_t recordIndex = claimCapacity(alignedRecordLength);

            if (INSUFFICIENT_CAPACITY == recordIndex) {
                return false;
            }

            m_buffer.putInt32Ordered(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
            std::atomic_thread_fence(std::memory_order_release);

            m_buffer.putBytes(aeron::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex), srcBuffer, srcIndex, length);
            m_buffer.putInt32(aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex), msgTypeId);
            m_buffer.putInt32Ordered(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), recordLength);

            return true;
        }

        [[nodiscard]] aeron::util::index_t tryClaim(std::int32_t msgTypeId, size_t length) const {
            aeron::ringbuffer::RecordDescriptor::checkMsgTypeId(msgTypeId);
            checkMsgLength(length);

            const aeron::util::index_t recordLength = length + aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH;
            const aeron::util::index_t alignedRecordLength = aeron::util::BitUtil::align(recordLength, aeron::ringbuffer::RecordDescriptor::ALIGNMENT);
            const aeron::util::index_t recordIndex = claimCapacity(alignedRecordLength);

            if (INSUFFICIENT_CAPACITY == recordIndex) {
                return recordIndex;
            }

            m_buffer.putInt32Ordered(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
            std::atomic_thread_fence(std::memory_order_release);
            m_buffer.putInt32(aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex), msgTypeId);

            return aeron::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex);
        }

        void commit(aeron::util::index_t index) const {
            const aeron::util::index_t recordIndex = computeRecordIndex(index);
            const std::int32_t recordLength = verifyClaimedSpaceNotReleased(recordIndex);
            m_buffer.putInt32Ordered(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
        }

        void abort(aeron::util::index_t index) const {
            const aeron::util::index_t recordIndex = computeRecordIndex(index);
            const std::int32_t recordLength = verifyClaimedSpaceNotReleased(recordIndex);

            m_buffer.putInt32(aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex), aeron::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID);
            m_buffer.putInt32Ordered(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
        }

        [[nodiscard]] inline int read(const agrona_handler_t &handler, int messageCountLimit) const {
            const std::int64_t head = m_buffer.getInt64(m_headPositionIndex);
            const auto headIndex = static_cast<std::int32_t>(head & (m_capacity - 1));
            const std::int32_t contiguousBlockLength = m_capacity - headIndex;
            int messagesRead = 0;
            int bytesRead = 0;

            while ((bytesRead < contiguousBlockLength) && (messagesRead < messageCountLimit)) {
                const std::int32_t recordIndex = headIndex + bytesRead;
                const std::int32_t recordLength = m_buffer.
                        getInt32Volatile(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex));

                if (recordLength <= 0) {
                    break;
                }

                const std::int32_t alignedLength = aeron::util::BitUtil::align(recordLength, aeron::ringbuffer::RecordDescriptor::ALIGNMENT);
                bytesRead += alignedLength;

                const std::int32_t msgTypeId = m_buffer.getInt32(aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex));
                if (aeron::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID == msgTypeId) {
                    continue;
                }

                handler(msgTypeId, m_buffer, aeron::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex),
                        recordLength - aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH);
                ++messagesRead;
            }

            if (bytesRead > 0) {
                m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
            }

            return messagesRead;
        }

        [[nodiscard]] inline int controlledRead(const agrona_controlled_handler_t &handler, int messageCountLimit) const {
            std::int64_t head = m_buffer.getInt64(m_headPositionIndex);
            auto headIndex = static_cast<std::int32_t>(head & (m_capacity - 1));
            const std::int32_t maxBlockLength = m_capacity - headIndex;
            int messagesRead = 0;
            int bytesRead = 0;

            while ((bytesRead < maxBlockLength) && (messagesRead < messageCountLimit)) {
                const std::int32_t recordIndex = headIndex + bytesRead;
                const std::int32_t recordLength = m_buffer.
                        getInt32Volatile(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex));

                if (recordLength <= 0) {
                    break;
                }

                const std::int32_t alignedLength = aeron::util::BitUtil::align(recordLength, aeron::ringbuffer::RecordDescriptor::ALIGNMENT);
                bytesRead += alignedLength;
                const std::int32_t msgTypeId = m_buffer.getInt32(aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex));

                if (msgTypeId == aeron::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID) {
                    continue;
                }

                ControlledAction action = handler(msgTypeId, m_buffer, aeron::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex),
                                                  recordLength - aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH);

                if (action == ControlledAction::ABORT) {
                    bytesRead -= alignedLength;
                    break;
                }

                ++messagesRead;

                if (action == ControlledAction::BREAK) {
                    break;
                }

                if (action == ControlledAction::COMMIT) {
                    m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
                    headIndex += bytesRead;
                    head += bytesRead;
                    bytesRead = 0;
                }
            }

            if (bytesRead > 0) {
                m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
            }

            return messagesRead;
        }

        AgronaAtomicBuffer & buffer() {
            return m_buffer;
        }

    private:
        AgronaAtomicBuffer &m_buffer;
        aeron::util::index_t m_capacity;
        aeron::util::index_t m_maxMsgLength;
        aeron::util::index_t m_headPositionIndex;
        aeron::util::index_t m_headCachePositionIndex;
        aeron::util::index_t m_tailPositionIndex;
        aeron::util::index_t m_correlationIdCounterIndex;
        aeron::util::index_t m_consumerHeartbeatIndex;

        [[nodiscard]] aeron::util::index_t claimCapacity(aeron::util::index_t alignedRecordLength) const {
            const aeron::util::index_t mask = m_capacity - 1;
            std::int64_t head = m_buffer.getInt64(m_headCachePositionIndex);
            const std::int64_t tail = m_buffer.getInt64(m_tailPositionIndex);
            const auto availableCapacity = static_cast<std::int32_t>(m_capacity - (tail - head));

            if (alignedRecordLength > availableCapacity) {
                head = m_buffer.getInt64Volatile(m_headPositionIndex);

                if (alignedRecordLength > static_cast<std::int32_t>(m_capacity - (tail - head))) {
                    return INSUFFICIENT_CAPACITY;
                }

                m_buffer.putInt64(m_headCachePositionIndex, head);
            }

            const auto recordIndex = static_cast<aeron::util::index_t>(tail & mask);
            m_buffer.putInt64Ordered(m_tailPositionIndex, tail + alignedRecordLength);

            return recordIndex;
        }

        inline void checkMsgLength(aeron::util::index_t length) const {
            if (length > m_maxMsgLength) {
                throw aeron::util::IllegalArgumentException(
                    "encoded message exceeds maxMsgLength of " + std::to_string(m_maxMsgLength) +
                    " length=" + std::to_string(length), SOURCEINFO);
            }
        }

        [[nodiscard]] inline aeron::util::index_t computeRecordIndex(aeron::util::index_t index) const {
            const aeron::util::index_t recordIndex = index - aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH;
            if (recordIndex > m_capacity - aeron::ringbuffer::RecordDescriptor::HEADER_LENGTH) {
                throw aeron::util::IllegalArgumentException("Invalid message index: " + std::to_string(index), SOURCEINFO);
            }
            return recordIndex;
        }

        [[nodiscard]] inline std::int32_t verifyClaimedSpaceNotReleased(aeron::util::index_t recordIndex) const {
            const std::int32_t recordLength = m_buffer.getInt32(aeron::ringbuffer::RecordDescriptor::lengthOffset(recordIndex));
            if (recordLength < 0) {
                return -recordLength;
            }

            const std::int32_t typeId = m_buffer.getInt32(aeron::ringbuffer::RecordDescriptor::typeOffset(recordIndex));
            throw aeron::util::IllegalStateException(
                "claimed space previously " + std::string(typeId == aeron::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID
                                                              ? "aborted"
                                                              : "committed"),
                SOURCEINFO);
        }
    };
}
