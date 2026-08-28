#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_set>

class RateLimiter {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using ms = std::chrono::milliseconds;

    static constexpr ms MIN_IDLE_TIME = ms(60'000); // 1 minute

    // Token bucket with CONTINUOUS refill.
    //  - ratePerSecond : sustained refill rate (tokens added per second) = the long-run cap.
    //  - burstCapacity : bucket size = how many messages may be sent in a short burst ABOVE the
    //                    sustained rate before the bucket runs dry. 0 => capacity == ratePerSecond
    //                    (no extra burst headroom). With capacity > rate a client can briefly
    //                    exceed the rate — e.g. flushing a backlog after a network stall — which
    //                    is absorbed by the bucket, but SUSTAINING over the rate drains it and
    //                    then trips violations. This avoids kicking legit clients on a momentary
    //                    burst while still cutting a real flood.
    //
    // Whitelisted message types bypass the limiter entirely (e.g. PONG/KEEPALIVE).
    //
    // Example: RateLimiter(30, ms(2000), { PONG }, 60)  // 30/s sustained, tolerate a 60-msg burst
    explicit RateLimiter(std::uint32_t ratePerSecond,
                         ms violationWindow,
                         std::unordered_set<std::uint8_t> whitelist = {},
                         std::uint32_t burstCapacity = 0)
        : ratePerSecond(ratePerSecond),
          capacity(burstCapacity ? burstCapacity : ratePerSecond),
          tokens(burstCapacity ? burstCapacity : ratePerSecond),
          lastRefillTime(now()),
          violationWindow(violationWindow),
          violationCount(0),
          lastActivityTime(lastRefillTime),
          whitelist(std::move(whitelist))
    {}

    // Returns:
    //  0 = allowed
    // -1 = first violation
    // -2 = additional violations in window
    // -3 = final violation (kick)
    int allow(std::uint8_t messageType) {
        // Whitelisted messages bypass the limiter but still count as activity so heartbeat/
        // keepalive traffic keeps the client from being evicted by isInactive().
        if (whitelist.find(messageType) != whitelist.end()) {
            lastActivityTime = now();
            return 0;
        }

        const auto currentTime = now();
        lastActivityTime = currentTime;
        refill(currentTime);

        if (tokens > 0) {
            --tokens;
            resetViolations();
            return 0;
        }

        return trackViolation(currentTime);
    }

    [[nodiscard]] bool isInactive() const {
        return (now() - lastActivityTime) >= MIN_IDLE_TIME;
    }

private:
    const std::uint32_t ratePerSecond;   // sustained refill rate (tokens/sec)
    const std::uint32_t capacity;        // bucket size (burst budget)
    std::uint32_t tokens;

    TimePoint lastRefillTime;
    std::int64_t refillCarryMs = 0;      // fractional-token carry (rate*ms not yet a whole token)

    const ms violationWindow;
    std::uint32_t violationCount;
    TimePoint firstViolationTime{};
    TimePoint lastActivityTime;

    const std::unordered_set<std::uint8_t> whitelist;

    static inline TimePoint now() {
        return Clock::now();
    }

    // Continuous refill: add ratePerSecond tokens/sec, saturating at capacity. Fractional
    // tokens are carried across calls so many sub-second allow()s still refill at the right
    // rate (a hard per-second reset let a client send up to 2x capacity across a window edge).
    void refill(TimePoint currentTime) {
        const std::int64_t elapsedMs =
            std::chrono::duration_cast<ms>(currentTime - lastRefillTime).count();
        if (elapsedMs <= 0) {
            return;
        }
        lastRefillTime = currentTime;
        refillCarryMs += elapsedMs * static_cast<std::int64_t>(ratePerSecond);
        const std::int64_t add = refillCarryMs / 1000;
        if (add > 0) {
            refillCarryMs -= add * 1000;
            const std::int64_t newTokens = static_cast<std::int64_t>(tokens) + add;
            tokens = static_cast<std::uint32_t>(
                std::min<std::int64_t>(newTokens, static_cast<std::int64_t>(capacity)));
        }
    }

    void resetViolations() {
        violationCount = 0;
        firstViolationTime = TimePoint{};
    }

    int trackViolation(TimePoint currentTime) {
        if (violationCount == 0) {
            firstViolationTime = currentTime;
            violationCount = 1;
            return -1;
        }

        if ((currentTime - firstViolationTime) <= violationWindow) {
            ++violationCount;
            return (violationCount >= 3) ? -3 : -2;
        } else {
            // Reset violation window
            firstViolationTime = currentTime;
            violationCount = 1;
            return -1;
        }
    }
};
