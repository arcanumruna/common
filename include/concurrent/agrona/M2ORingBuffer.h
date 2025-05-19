#pragma once

#include <climits>
#include <functional>
#include "util/LangUtil.h"
#include "concurrent/agrona/AgronaRingBufferDescriptor.h"
#include "concurrent/ringbuffer/RecordDescriptor.h"
#include "AgronaAtomicBuffer.h"


namespace agrona::concurrent::ringbuffer {

            class M2ORingBuffer {
            public:
                explicit M2ORingBuffer(AgronaAtomicBuffer &buffer) : m_buffer(buffer) {
                    m_capacity = buffer.capacity() - AgronaRingBufferDescriptor::TRAILER_LENGTH;

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

                bool write(std::int32_t msgTypeId, const AgronaAtomicBuffer &srcBuffer, aeron::util::index_t srcIndex,
                           aeron::util::index_t length) {
                    bool isSuccessful = false;

                    aeron::concurrent::ringbuffer::RecordDescriptor::checkMsgTypeId(msgTypeId);
                    checkMsgLength(length);

                    const aeron::util::index_t recordLength = length + aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH;
                    const aeron::util::index_t requiredCapacity = aeron::util::BitUtil::align(
                        recordLength, aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT);
                    const aeron::util::index_t recordIndex = claimCapacity(requiredCapacity);

                    if (INSUFFICIENT_CAPACITY != recordIndex) {
                        m_buffer.putInt64Ordered(recordIndex, aeron::concurrent::ringbuffer::RecordDescriptor::makeHeader(-recordLength, msgTypeId));
                        m_buffer.putBytes(aeron::concurrent::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex), srcBuffer, srcIndex, length);
                        m_buffer.putInt32Ordered(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), recordLength);

                        isSuccessful = true;
                    }

                    return isSuccessful;
                }

                inline aeron::util::index_t tryClaim(std::int32_t msgTypeId, size_t length)
                {
                    aeron::concurrent::ringbuffer::RecordDescriptor::checkMsgTypeId(msgTypeId);
                    checkMsgLength(length);

                    const aeron::util::index_t recordLength = length + aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH;
                    aeron::util::index_t recordIndex = claimCapacity(recordLength);

                    if (INSUFFICIENT_CAPACITY == recordIndex)
                    {
                        return recordIndex;
                    }

                    m_buffer.putInt32Ordered(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
                    std::atomic_thread_fence(std::memory_order_release);
                    m_buffer.putInt32(aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex), msgTypeId);

                    return aeron::concurrent::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex);
                }

                inline void commit(aeron::util::index_t index)
                {
                    const aeron::util::index_t recordIndex = computeRecordIndex(index);
                    const std::int32_t recordLength = verifyClaimedSpaceNotReleased(recordIndex);

                    m_buffer.putInt32Ordered(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
                }

                inline void abort(aeron::util::index_t index)
                {
                    const aeron::util::index_t recordIndex = computeRecordIndex(index);
                    const std::int32_t recordLength = verifyClaimedSpaceNotReleased(recordIndex);

                    m_buffer.putInt32(aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex), aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID);
                    m_buffer.putInt32Ordered(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex), -recordLength);
                }

                int read(const agrona_handler_t &handler, int messageCountLimit) {
                    const std::int64_t head = m_buffer.getInt64(m_headPositionIndex);
                    const auto headIndex = static_cast<std::int32_t>(head & (m_capacity - 1));
                    const std::int32_t contiguousBlockLength = m_capacity - headIndex;
                    int messagesRead = 0;
                    int bytesRead = 0;

                    auto cleanup = aeron::util::InvokeOnScopeExit{
                        [&]() {
                            if (bytesRead != 0) {
                                m_buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                                m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
                            }
                        }
                    };

                    while ((bytesRead < contiguousBlockLength) && (messagesRead < messageCountLimit)) {
                        const std::int32_t recordIndex = headIndex + bytesRead;
                        const std::int64_t header = m_buffer.getInt64Volatile(recordIndex);
                        const std::int32_t recordLength = aeron::concurrent::ringbuffer::RecordDescriptor::recordLength(header);

                        if (recordLength <= 0) {
                            break;
                        }

                        bytesRead += aeron::util::BitUtil::align(recordLength, aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT);

                        const std::int32_t msgTypeId = aeron::concurrent::ringbuffer::RecordDescriptor::messageTypeId(header);
                        if (aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID == msgTypeId) {
                            continue;
                        }

                        ++messagesRead;
                        handler(
                            msgTypeId,
                            m_buffer,
                            aeron::concurrent::ringbuffer::RecordDescriptor::encodedMsgOffset(recordIndex),
                            recordLength - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH);
                    }

                    return messagesRead;
                }

                int controlledRead(const agrona_controlled_handler_t &handler, int messageCountLimit = INT_MAX) {
                    const std::int64_t initialHead = m_buffer.getInt64(m_headPositionIndex);
                    std::int32_t headIndex = static_cast<std::int32_t>(initialHead & (m_capacity - 1));
                    const std::int32_t maxBlockLength = m_capacity - headIndex;

                    int messagesRead = 0;
                    int bytesRead = 0;
                    std::int64_t head = initialHead;

                    while (bytesRead < maxBlockLength && messagesRead < messageCountLimit) {
                        const std::int32_t recordIndex = headIndex + bytesRead;
                        const std::int64_t header = m_buffer.getInt64Volatile(recordIndex);
                        const std::int32_t recordLength = aeron::concurrent::ringbuffer::RecordDescriptor::recordLength(header);

                        if (recordLength <= 0) {
                            break;
                        }

                        const std::int32_t alignedLength = aeron::util::BitUtil::align(
                            recordLength, aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT);
                        bytesRead += alignedLength;
                        const std::int32_t msgTypeId = aeron::concurrent::ringbuffer::RecordDescriptor::messageTypeId(header);

                        if (msgTypeId == aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID) {
                            continue;
                        }

                        ControlledAction action = handler(
                            msgTypeId,
                            m_buffer,
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
                            m_buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                            m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
                            head += bytesRead;
                            headIndex = static_cast<std::int32_t>(head & (m_capacity - 1));
                            bytesRead = 0;
                        }
                    }

                    if (bytesRead > 0) {
                        m_buffer.setMemory(headIndex, static_cast<std::size_t>(bytesRead), 0);
                        m_buffer.putInt64Ordered(m_headPositionIndex, head + bytesRead);
                    }

                    return messagesRead;
                }


                inline int read(const agrona_handler_t &handler) {
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
                    std::int64_t headBefore;
                    std::int64_t tail;
                    std::int64_t headAfter = m_buffer.getInt64Volatile(m_headPositionIndex);

                    do {
                        headBefore = headAfter;
                        tail = m_buffer.getInt64Volatile(m_tailPositionIndex);
                        headAfter = m_buffer.getInt64Volatile(m_headPositionIndex);
                    } while (headAfter != headBefore);

                    std::int64_t size = tail - headAfter;
                    if (size < 0) {
                        return 0;
                    } else if (size > m_capacity) {
                        return static_cast<std::int32_t>(m_capacity);
                    }

                    return static_cast<std::int32_t>(size);
                }

                bool unblock() {
                    const std::int64_t headPosition = m_buffer.getInt64Volatile(m_headPositionIndex);
                    const std::int64_t tailPosition = m_buffer.getInt64Volatile(m_tailPositionIndex);

                    if (tailPosition == headPosition) {
                        return false;
                    }

                    const aeron::util::index_t mask = m_capacity - 1;
                    const auto consumerIndex = static_cast<std::int32_t>(headPosition & mask);
                    const auto producerIndex = static_cast<std::int32_t>(tailPosition & mask);

                    bool unblocked = false;
                    std::int32_t length = m_buffer.getInt32Volatile(consumerIndex);
                    if (length < 0) {
                        m_buffer.putInt64Ordered(
                            consumerIndex,
                            aeron::concurrent::ringbuffer::RecordDescriptor::makeHeader(-length, aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID));
                        unblocked = true;
                    } else if (0 == length) {
                        const std::int32_t limit = producerIndex > consumerIndex ? producerIndex : m_buffer.capacity();
                        std::int32_t i = consumerIndex + aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT;

                        do {
                            length = m_buffer.getInt32Volatile(i);
                            if (0 != length) {
                                if (scanBackToConfirmStillZeroed(m_buffer, i, consumerIndex)) {
                                    m_buffer.putInt64Ordered(
                                        consumerIndex,
                                        aeron::concurrent::ringbuffer::RecordDescriptor::makeHeader(i - consumerIndex,
                                                                     aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID));
                                    unblocked = true;
                                }

                                break;
                            }

                            i += aeron::concurrent::ringbuffer::RecordDescriptor::ALIGNMENT;
                        } while (i < limit);
                    }

                    return unblocked;
                }

                AgronaAtomicBuffer & buffer() {
                    return m_buffer;
                }

            private:
                static constexpr aeron::util::index_t INSUFFICIENT_CAPACITY = -2;

                AgronaAtomicBuffer &m_buffer;
                aeron::util::index_t m_capacity;
                aeron::util::index_t m_maxMsgLength;
                aeron::util::index_t m_headPositionIndex;
                aeron::util::index_t m_headCachePositionIndex;
                aeron::util::index_t m_tailPositionIndex;
                aeron::util::index_t m_correlationIdCounterIndex;
                aeron::util::index_t m_consumerHeartbeatIndex;

                [[nodiscard]] inline aeron::util::index_t computeRecordIndex(aeron::util::index_t index) const
                {
                    aeron::util::index_t recordIndex = index - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH;
                    if (recordIndex < 0 || recordIndex > (m_capacity - aeron::concurrent::ringbuffer::RecordDescriptor::HEADER_LENGTH))
                    {
                        throw aeron::util::IllegalArgumentException("Invalid message index: " + std::to_string(index), SOURCEINFO);
                    }

                    return recordIndex;
                }

                [[nodiscard]] inline std::int32_t verifyClaimedSpaceNotReleased(aeron::util::index_t recordIndex) const
                {
                    const std::int32_t recordLength = m_buffer.getInt32(aeron::concurrent::ringbuffer::RecordDescriptor::lengthOffset(recordIndex));

                    if (recordLength < 0)
                    {
                        return -recordLength;
                    }

                    const std::int32_t typeId = m_buffer.getInt32(aeron::concurrent::ringbuffer::RecordDescriptor::typeOffset(recordIndex));
                    throw aeron::util::IllegalStateException(
                        "Claimed space previously " + std::string(typeId == aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID ? "aborted" : "committed"),
                        SOURCEINFO);
                }

                aeron::util::index_t claimCapacity(aeron::util::index_t requiredCapacity) {
                    const aeron::util::index_t mask = m_capacity - 1;
                    std::int64_t head = m_buffer.getInt64Volatile(m_headCachePositionIndex);

                    std::int64_t tail;
                    aeron::util::index_t tailIndex;
                    aeron::util::index_t padding;
                    do {
                        tail = m_buffer.getInt64Volatile(m_tailPositionIndex);
                        const aeron::util::index_t availableCapacity = m_capacity - static_cast<aeron::util::index_t>(tail - head);

                        if (requiredCapacity > availableCapacity) {
                            head = m_buffer.getInt64Volatile(m_headPositionIndex);

                            if (requiredCapacity > (m_capacity - static_cast<aeron::util::index_t>(tail - head))) {
                                return INSUFFICIENT_CAPACITY;
                            }

                            m_buffer.putInt64Ordered(m_headCachePositionIndex, head);
                        }

                        padding = 0;
                        tailIndex = static_cast<aeron::util::index_t>(tail & mask);
                        const aeron::util::index_t toBufferEndLength = m_capacity - tailIndex;

                        if (requiredCapacity > toBufferEndLength) {
                            auto headIndex = static_cast<std::int32_t>(head & mask);

                            if (requiredCapacity > headIndex) {
                                head = m_buffer.getInt64Volatile(m_headPositionIndex);
                                headIndex = static_cast<std::int32_t>(head & mask);

                                if (requiredCapacity > headIndex) {
                                    return INSUFFICIENT_CAPACITY;
                                }

                                m_buffer.putInt64Ordered(m_headCachePositionIndex, head);
                            }

                            padding = toBufferEndLength;
                        }
                    } while (!m_buffer.compareAndSetInt64(m_tailPositionIndex, tail, tail + requiredCapacity + padding))
                    ;

                    if (0 != padding) {
                        m_buffer.putInt64Ordered(
                            tailIndex, aeron::concurrent::ringbuffer::RecordDescriptor::makeHeader(padding, aeron::concurrent::ringbuffer::RecordDescriptor::PADDING_MSG_TYPE_ID));
                        tailIndex = 0;
                    }

                    return tailIndex;
                }

                inline void checkMsgLength(aeron::util::index_t length) const {
                    if (length > m_maxMsgLength) {
                        throw aeron::util::IllegalArgumentException(
                            "encoded message exceeds maxMsgLength of " + std::to_string(m_maxMsgLength) +
                            " length=" + std::to_string(length),
                            SOURCEINFO);
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

