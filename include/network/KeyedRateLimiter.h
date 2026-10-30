#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>

#include "network/RateLimiter.h"

// Per-key (per-IP / per-name / ...) rate limiter. Each key lazily gets its own
// RateLimiter::overWindow(maxEvents, window) bucket, all sharing one clock.
//
// NOT THREAD-SAFE (like RateLimiter): serialise access externally.
//
// Memory is bounded: idle keys (bucket full, not locked, untouched for >= idleEvict)
// are pruned automatically about every idleEvict/4, and the map never exceeds
// maxKeys. At the cap a prune is attempted first; if still full, NEW keys are
// rejected (fail closed: tryAcquire returns false) and rejectedNewKeys() counts them.
// Existing keys keep working.
//
// Keys: memory is only bounded if Key is; callers must cap key length (IP string, name <= N).
//
// Lock API: lockFor(key, d) blocks a key for exactly d; isLocked is true while
// now < lockUntil. tryAcquire on a locked key returns false and consumes nothing.
template <class Key, class Hash = std::hash<Key>, class Eq = std::equal_to<Key>>
class KeyedRateLimiter {
public:
    using Clock = RateLimiter::Clock;
    using TimePoint = RateLimiter::TimePoint;
    using ms = std::chrono::milliseconds;
    using NowFn = RateLimiter::NowFn;

    KeyedRateLimiter(std::uint32_t maxEvents,
                     ms window,
                     std::size_t maxKeys = 65536,
                     ms idleEvict = std::chrono::seconds(60),
                     NowFn now = &RateLimiter::steadyNow)
        : maxEvents_(maxEvents), window_(window), maxKeys_(maxKeys), idleEvict_(idleEvict),
          now_(now ? std::move(now) : NowFn(&RateLimiter::steadyNow)),
          lastPrune_(now_()), nextPrune_(lastPrune_ + pruneInterval()) {}

    // Check and take in one step. False if locked, out of tokens, or (new key at cap) rejected.
    [[nodiscard]] bool tryAcquire(const Key& key) {
        Entry* e = entryFor(key);
        if (!e) return false;
        if (isLockedAt(*e, e->touched)) return false;
        if (!e->bucket.hasToken()) return false;
        e->bucket.take();
        return true;
    }

    // Would tryAcquire succeed? Never inserts and never consumes. Fail closed: an unknown key
    // while the map is at maxKeys reports false, UNLESS the throttled cap-time prune is due
    // (this const query cannot prune itself). In that case it reports true and the following
    // take()/tryAcquire() runs the prune and still fails closed if the map is full, so the
    // hasToken -> take pattern makes progress after a flood. Always check take()'s result.
    [[nodiscard]] bool hasToken(const Key& key) const {
        auto it = map_.find(key);
        if (it == map_.end()) {
            if (map_.size() < maxKeys_) return true;
            return maxKeys_ > 0 && (now_() - lastPrune_) >= capPruneInterval();
        }
        if (isLockedAt(it->second, now_())) return false;
        return it->second.bucket.hasToken();
    }

    // Consume one token (creating the key if absent). Returns false, and counts the key in
    // rejectedNewKeys(), if the key is new and the map is at the cap (nothing was consumed).
    [[nodiscard]] bool take(const Key& key) {
        Entry* e = entryFor(key);
        if (!e) return false;
        e->bucket.take();
        return true;
    }

    // 0 if a token is available (or key unknown); otherwise ms until one is, or until
    // the lock ends if that is later.
    [[nodiscard]] long long millisUntilNextToken(const Key& key) const {
        auto it = map_.find(key);
        if (it == map_.end()) return 0;
        const TimePoint t = now_();
        long long wait = it->second.bucket.millisUntilNextToken();
        if (isLockedAt(it->second, t)) {
            const long long lockLeft =
                std::chrono::ceil<ms>(it->second.lockUntil - t).count();
            if (lockLeft > wait) wait = lockLeft;
        }
        return wait;
    }

    // Lock `key` for `duration` (never shortens an existing longer lock; duration <= 0 is a
    // no-op). A lock is security-critical and is NEVER dropped: at the cap a victim is evicted
    // to make room (idle keys first, then the cheapest unlocked entry, finally the lock that
    // expires soonest), so the map stays bounded.
    void lockFor(const Key& key, ms duration) {
        if (duration.count() <= 0) return;
        const TimePoint t = now_();
        maybePrune(t);
        auto it = map_.find(key);
        if (it == map_.end()) {
            if (map_.size() >= maxKeys_) {
                if (maxKeys_ == 0) return;   // degenerate: nothing can be stored
                evictOne(t);
            }
            it = map_.try_emplace(key, maxEvents_, window_, now_, t).first;
        }
        Entry& e = it->second;
        e.touched = t;
        const TimePoint until = (duration >= std::chrono::duration_cast<ms>(TimePoint::max() - t)) ? TimePoint::max() : t + duration;
        if (until > e.lockUntil) e.lockUntil = until;
    }

    [[nodiscard]] bool isLocked(const Key& key) const {
        auto it = map_.find(key);
        return it != map_.end() && isLockedAt(it->second, now_());
    }

    // Evict keys that are idle: bucket full, not locked, untouched for >= idleEvict.
    void prune() {
        const TimePoint t = now_();
        for (auto it = map_.begin(); it != map_.end();) {
            Entry& e = it->second;
            if (!isLockedAt(e, t) && (t - e.touched) >= idleEvict_ && e.bucket.isFull()) {
                it = map_.erase(it);
            } else {
                ++it;
            }
        }
        lastPrune_ = t;
        nextPrune_ = t + pruneInterval();
        ++scans_;
    }

    [[nodiscard]] std::size_t size() const { return map_.size(); }
    [[nodiscard]] std::size_t rejectedNewKeys() const { return rejected_; }
    // Number of full prune scans performed (diagnostics / tests).
    [[nodiscard]] std::size_t pruneScans() const { return scans_; }

private:
    struct Entry {
        Entry(std::uint32_t maxEvents, ms window, const NowFn& now, TimePoint t)
            : bucket(RateLimiter::overWindow(maxEvents, window, now)), touched(t), lockUntil(t) {}
        // mutable-by-design: refill is a pure function of time, so const queries may refill.
        mutable RateLimiter bucket;
        TimePoint touched;
        TimePoint lockUntil;
    };

    ms pruneInterval() const {
        const ms q = idleEvict_ / 4;
        return q.count() > 0 ? q : ms(1);
    }

    static bool isLockedAt(const Entry& e, TimePoint t) { return t < e.lockUntil; }

    // Cap-time prune is throttled so a distinct-key flood costs O(1) per rejected key.
    ms capPruneInterval() const {
        const ms q = pruneInterval();
        return q < ms(1000) ? q : ms(1000);
    }

    void maybePrune(TimePoint t) {
        if (t >= nextPrune_) prune();
    }

    // Make room for one entry. Prefers (throttled) prune; otherwise evicts a victim.
    void evictOne(TimePoint t) {
        if ((t - lastPrune_) >= capPruneInterval()) {
            prune();
            if (map_.size() < maxKeys_) return;
        }
        // Bounded sample: prefer an unlocked entry with a full bucket, else the oldest unlocked.
        constexpr int kSample = 16;
        auto victim = map_.end();
        bool victimFull = false;
        int seen = 0;
        for (auto it = map_.begin(); it != map_.end(); ++it) {
            if (isLockedAt(it->second, t)) continue;
            const bool full = it->second.bucket.isFull();
            if (victim == map_.end() || (full && !victimFull) ||
                (full == victimFull && it->second.touched < victim->second.touched)) {
                victim = it;
                victimFull = full;
            }
            if (++seen >= kSample) break;
        }
        if (victim == map_.end()) {
            // Everything is locked: drop the lock that expires soonest.
            for (auto it = map_.begin(); it != map_.end(); ++it) {
                if (victim == map_.end() || it->second.lockUntil < victim->second.lockUntil) victim = it;
            }
        }
        if (victim != map_.end()) map_.erase(victim);
    }

    // Finds or creates the entry (touching it). nullptr => rejected at cap (counted).
    Entry* entryFor(const Key& key) {
        const TimePoint t = now_();
        maybePrune(t);
        auto it = map_.find(key);
        if (it == map_.end()) {
            if (map_.size() >= maxKeys_ && (t - lastPrune_) >= capPruneInterval()) prune();
            if (map_.size() >= maxKeys_) {
                ++rejected_;
                return nullptr;
            }
            it = map_.try_emplace(key, maxEvents_, window_, now_, t).first;
        }
        it->second.touched = t;
        return &it->second;
    }

    std::uint32_t maxEvents_;
    ms window_;
    std::size_t maxKeys_;
    ms idleEvict_;
    NowFn now_;
    TimePoint lastPrune_;
    TimePoint nextPrune_;
    std::size_t scans_ = 0;
    std::size_t rejected_ = 0;
    std::unordered_map<Key, Entry, Hash, Eq> map_;
};
