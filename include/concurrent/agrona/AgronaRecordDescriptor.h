#pragma once

// Ring buffer record header layout, from Aeron's C++ client
// (concurrent/ringbuffer/RecordDescriptor.h, Copyright 2014-2025 Real Logic Limited, Apache License 2.0).
// Kept here because Aeron packages that ship only the C client and the C++ wrapper do not carry that header.
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |R|                       Record Length                         |
//  +-+-------------------------------------------------------------+
//  |                              Type                             |
//  +---------------------------------------------------------------+
//  |                       Encoded Message                        ...
//
// Writing the record length signals that the record is complete.

#include <cstdint>
#include <string>

#include "util/Index.h"
#include "util/Exceptions.h"

namespace agrona::concurrent::ringbuffer::AgronaRecordDescriptor {

    inline constexpr aeron::util::index_t HEADER_LENGTH = sizeof(std::int32_t) * 2;
    inline constexpr aeron::util::index_t ALIGNMENT = HEADER_LENGTH;
    inline constexpr std::int32_t PADDING_MSG_TYPE_ID = -1;

    constexpr aeron::util::index_t lengthOffset(aeron::util::index_t recordOffset) noexcept {
        return recordOffset;
    }

    constexpr aeron::util::index_t typeOffset(aeron::util::index_t recordOffset) noexcept {
        return recordOffset + static_cast<aeron::util::index_t>(sizeof(std::int32_t));
    }

    constexpr aeron::util::index_t encodedMsgOffset(aeron::util::index_t recordOffset) noexcept {
        return recordOffset + HEADER_LENGTH;
    }

    constexpr std::int64_t makeHeader(std::int32_t length, std::int32_t msgTypeId) noexcept {
        return ((static_cast<std::int64_t>(msgTypeId) & 0xFFFFFFFF) << 32) | (length & 0xFFFFFFFF);
    }

    constexpr std::int32_t recordLength(std::int64_t header) noexcept {
        return static_cast<std::int32_t>(header);
    }

    constexpr std::int32_t messageTypeId(std::int64_t header) noexcept {
        return static_cast<std::int32_t>(header >> 32);
    }

    inline void checkMsgTypeId(std::int32_t msgTypeId) {
        if (msgTypeId < 1) {
            throw aeron::util::IllegalArgumentException(
                "Message type id must be greater than zero, msgTypeId=" + std::to_string(msgTypeId), SOURCEINFO);
        }
    }
}
