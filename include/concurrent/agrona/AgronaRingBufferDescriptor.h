#pragma once


#include <functional>

#include "AgronaAtomicBuffer.h"
#include "aeron.h"
#include "util/Index.h"
#include "util/BitUtil.h"

namespace agrona { namespace concurrent { namespace ringbuffer {

    /** The read handler function signature */
    typedef std::function<void(std::int32_t, AgronaAtomicBuffer &, aeron::util::index_t, aeron::util::index_t)> agrona_handler_t;

    enum class ControlledAction {
        ABORT,
        BREAK,
        COMMIT,
        CONTINUE
    };

    using agrona_controlled_handler_t = std::function<ControlledAction(
        std::int32_t msgTypeId,
        const AgronaAtomicBuffer &buffer,
        std::int32_t index,
        std::int32_t length
    )>;

    using namespace aeron::util::BitUtil;

    namespace AgronaRingBufferDescriptor {

        static const aeron::util::index_t TAIL_POSITION_OFFSET = CACHE_LINE_LENGTH * 2;
        static const aeron::util::index_t HEAD_CACHE_POSITION_OFFSET = CACHE_LINE_LENGTH * 4;
        static const aeron::util::index_t HEAD_POSITION_OFFSET = CACHE_LINE_LENGTH * 6;
        static const aeron::util::index_t CORRELATION_COUNTER_OFFSET = CACHE_LINE_LENGTH * 8;
        static const aeron::util::index_t CONSUMER_HEARTBEAT_OFFSET = CACHE_LINE_LENGTH * 10;

        /** Total length of the trailer in bytes. */
        static const aeron::util::index_t TRAILER_LENGTH = CACHE_LINE_LENGTH * 12;

        inline static void checkCapacity(aeron::util::index_t capacity)
        {
            if (!aeron::util::BitUtil::isPowerOfTwo(capacity))
            {
                throw aeron::util::IllegalArgumentException(
                    "Capacity must be a positive power of 2 + TRAILER_LENGTH: capacity=" + std::to_string(capacity),
                    SOURCEINFO);
            }
        }
    }

}}}