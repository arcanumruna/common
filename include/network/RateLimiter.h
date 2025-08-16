#pragma once

#include <chrono>
#include <unordered_set>

class RateLimiter {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using ms = std::chrono::milliseconds;

    static constexpr ms MIN_IDLE_TIME = ms(60'000); // 1 minute

    // Constructor with optional whitelist of message types that bypass the limiter.
    // Example:
    //   RateLimiter rl(20, ms(2000), { game::NetworkMessage::PONG, game::NetworkMessage::KEEPALIVE });
    explicit RateLimiter(std::uint32_t ratePerSecond,
                         ms violationWindow,
                         std::unordered_set<std::uint8_t> whitelist = {})
        : maxTokens(ratePerSecond),
          tokens(ratePerSecond),
          refillInterval(ms(1000)), // refill every second
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
        // Allow whitelisted messages without touching tokens/violations/activity time.
        if (whitelist.find(messageType) != whitelist.end()) {
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
        const auto currentTime = now();
        return (currentTime - lastActivityTime) >= MIN_IDLE_TIME;
    }

private:
    const std::uint32_t maxTokens;
    std::uint32_t tokens;
    const ms refillInterval;

    TimePoint lastRefillTime;
    const ms violationWindow;

    std::uint32_t violationCount;
    TimePoint firstViolationTime{};
    TimePoint lastActivityTime;

    const std::unordered_set<std::uint8_t> whitelist;

    static inline TimePoint now() {
        return Clock::now();
    }

    void refill(TimePoint currentTime) {
        auto elapsed = std::chrono::duration_cast<ms>(currentTime - lastRefillTime);
        if (elapsed >= refillInterval) {
            tokens = maxTokens;
            lastRefillTime = currentTime;
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
