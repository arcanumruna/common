# Network

**Headers:** `include/network/`

Utilities for game-loop timing, ID generation, rate limiting, message parsing, and snapshotting.

---

## `FastRNG`

**Header:** `include/network/FastRNG.h`

Lightweight XORShift pseudo-random number generator designed for Entity Component Systems.

```cpp
struct FastRNG {
    uint32_t state = 0;
    uint32_t seed  = 0xdeadbeef; // currently unused; reserved for future seeding
};
```

### Methods

```cpp
uint32_t next();
```

Returns the next pseudo-random `uint32_t`. Uses a three-round XORShift. If the internal state becomes 0 after XOR operations (e.g. on first call), re-seeds from `std::chrono::steady_clock`.

```cpp
uint32_t next(uint32_t min, uint32_t max);
```

Returns a value in `[min, max)`. Returns `min` if `min >= max`.

```cpp
float nextFloat();
```

Returns a value in `[0.0, 1.0)` using the upper 24 bits of `next()`.

```cpp
uint32_t rollD6();
```

Returns a value in `[1, 6]`.

### Notes

- Not thread-safe; use one instance per thread.
- `state = 0` on construction causes the first call to always re-seed from the clock.
- The `seed` field is currently unused.

---

## `FrameSyncIdleStrategy`

**Header:** `include/network/FrameSyncIdleStrategy.h`
**Namespace:** `aeron::concurrent`

Idle strategy that paces a loop to a fixed frames-per-second target using adaptive sleep and yield phases.

```cpp
explicit FrameSyncIdleStrategy(int fps);
```

### Methods

```cpp
void idle();               // Block until the next frame boundary
void idle(int workCount);  // Same as idle() (workCount is ignored)
void reset();              // No-op
```

### Adaptive Algorithm

1. **Sleep phase:** Sleeps in increments of `remaining_time / 8` (min 1 µs) until the remaining time to the next frame is smaller than the running average sleep overhead.
2. **Yield phase:** Yields until the next frame boundary.
3. **Dampening:** If the average sleep duration exceeds 10 ms, all stored durations are multiplied by 0.9 to improve accuracy on low-resolution timers.
4. On first call, the frame clock is anchored to `Clock::now()`.

### Windows Note

On Windows, a long-sleeping background thread is spawned on first use to improve the OS timer resolution. See [bugs.md](bugs.md) for a known issue with this implementation.

---

## `FrameSyncBackoffIdleStrategy`

**Header:** `include/network/FrameSyncBackoffIdleStrategy.h`
**Namespace:** `aeron::concurrent`

Extends the frame-sync idle strategy with dynamic FPS backoff for adaptive workload handling.

```cpp
explicit FrameSyncBackoffIdleStrategy(int fps);
```

### Methods

```cpp
void idle();                   // Pace to the current FPS target
void idle(int workCount);      // Halve FPS on zero work; reset to default on work
void reset();                  // No-op
void setFps(int newFps);       // Change the FPS target at runtime
```

### Backoff Behaviour

- `idle(0)` (no work): `currentFps = max(1, currentFps / 2)`. This exponentially backs off toward 1 FPS.
- `idle(n > 0)` (work done): `currentFps` is immediately restored to the default FPS.
- The frame interval is updated atomically (`std::atomic<std::chrono::nanoseconds>`), so `setFps` is safe to call from another thread.

---

## `IdGenerator`

**Header:** `include/network/IdGenerator.h`

Monotonically-increasing ID counter with independent 32-bit and 16-bit streams.

```cpp
struct IdGenerator {
    std::uint32_t currentId32 = 1;
    std::uint16_t currentId16 = 1;
};
```

### Methods

```cpp
[[nodiscard]] std::uint32_t next()   noexcept;  // Post-increment 32-bit counter
[[nodiscard]] std::uint16_t next16() noexcept;  // Post-increment 16-bit counter

void reset  (std::uint32_t start32 = 1) noexcept;
void reset16(std::uint16_t start16 = 1) noexcept;
void resetAll(std::uint32_t start32 = 1, std::uint16_t start16 = 1) noexcept;

[[nodiscard]] std::uint32_t current()   const noexcept;  // Peek next 32-bit ID
[[nodiscard]] std::uint16_t current16() const noexcept;  // Peek next 16-bit ID
```

### Notes

- Both streams start at `1` (ID `0` is reserved as "none" / invalid by convention).
- Wraps silently on overflow (`UINT32_MAX + 1 → 0`, `UINT16_MAX + 1 → 0`).
- Not thread-safe.

---

## `MessageBatchView`

**Header:** `include/network/MessageBatchView.h`

Zero-copy, forward-iterator view over a tightly-packed batch of encoded messages.

### Wire Format

```
[length : 1 byte][typeId : 1 byte][payload : (length-1) bytes] ...
```

`length` = 1 + `payloadSize` (the `typeId` byte is counted in `length`).

### `BatchedMessage`

```cpp
struct BatchedMessage {
    uint8_t      typeId;
    const char*  payload;
    size_t       payloadSize;
};
```

### `MessageBatchView`

```cpp
explicit MessageBatchView(std::string_view data);

Iterator begin() const;
Iterator end()   const;
```

The iterator decodes one message per increment. On a malformed message (e.g. `length == 0` or truncated payload), the iterator sets its internal pointer to `nullptr` and compares equal to `end()`.

### Example

```cpp
std::string_view raw = getNetworkPayload();
for (const BatchedMessage& msg : MessageBatchView(raw)) {
    dispatch(msg.typeId, msg.payload, msg.payloadSize);
}
```

---

## `RateLimiter`

**Header:** `include/network/RateLimiter.h`

Token-bucket rate limiter with a violation window and optional per-message-type whitelist.

```cpp
explicit RateLimiter(std::uint32_t ratePerSecond,
                     ms            violationWindow,
                     std::unordered_set<std::uint8_t> whitelist = {});
```

- `ratePerSecond` — Maximum non-whitelisted messages per second (token bucket capacity and refill amount).
- `violationWindow` — Duration during which consecutive violations are tracked.
- `whitelist` — Set of message type bytes that bypass all rate limiting and violation tracking.

### `allow`

```cpp
int allow(std::uint8_t messageType);
```

| Return value | Meaning |
|:---:|--------|
| `0` | Message allowed |
| `-1` | First violation in the current window |
| `-2` | Additional violation (still within window) |
| `-3` | Third or later violation — caller should disconnect the client |

**Whitelist bypass:** Whitelisted messages always return `0` immediately and do **not** update the activity timer or consume tokens.

### `isInactive`

```cpp
[[nodiscard]] bool isInactive() const;
```

Returns `true` if no non-whitelisted message has been seen for at least 60 seconds. Use this to evict idle clients.

### Token Refill

Tokens are refilled to `ratePerSecond` once per second (`refillInterval = 1000 ms`). Partial-second accumulation is not supported.

### Violation Window

- The window starts on the **first** violation.
- Subsequent violations within `violationWindow` increment the counter.
- When `violationCount >= 3` the return value is `-3`.
- If a violation occurs **after** the window has expired, the window resets and `-1` is returned.
- Successful messages reset the violation counter.

### Example

```cpp
using ms = std::chrono::milliseconds;
RateLimiter limiter(20, ms(2000), { PONG, KEEPALIVE });

int result = limiter.allow(messageType);
if (result == -3) {
    kickClient(clientId);
} else if (result < 0) {
    warnClient(clientId);
}
```

---

## `SnapshotStore<T, Capacity>`

**Header:** `include/network/SnapshotStore.h`

Fixed-size circular buffer of timestamped snapshots, useful for interpolation and extrapolation.

```cpp
template<typename T, size_t Capacity>
class SnapshotStore;
// Capacity must be a power of two (enforced with static_assert).
```

### Methods

```cpp
void addSnapshot(int64_t ts, const T& val);
```

Stores a new snapshot. Overwrites the oldest entry when full.

```cpp
bool floor(int64_t ts, const T*& out) const;
```

Sets `out` to the most-recent snapshot with `timestamp <= ts`. Returns `false` if no such snapshot exists.

```cpp
bool ceil(int64_t ts, const T*& out) const;
```

Sets `out` to the oldest snapshot with `timestamp >= ts`. Returns `false` if no such snapshot exists.

```cpp
bool getLastSnapshot(T& out);
bool getSecondLastSnapshot(T& out);
```

Copy the most-recent or second-most-recent snapshot into `out`. Return `false` if insufficient entries.

```cpp
size_t getSize() const;
```

Returns the number of snapshots currently stored (capped at `Capacity`).

### Notes

- `floor` searches backwards from the newest entry; `ceil` searches forward from the oldest. Both are O(Capacity) worst-case.
- Not thread-safe.
- The timestamp type is `int64_t`; use a consistent unit (e.g. milliseconds from `getCurrentTimeMillis()`).

---

## `getHostname`

**Header:** `include/network/Network.h`

```cpp
inline std::string getHostname();
```

Returns the hostname of the current machine via `gethostname()`. Returns `"unknown-host"` on failure.
