# Time

**Header:** `include/time/Time.h`

Lightweight wrappers around `std::chrono` for common time queries.

## Functions

### `getCurrentTimeSeconds`

```cpp
inline uint32_t getCurrentTimeSeconds();
```

Returns the current time in whole seconds since the `std::chrono::steady_clock` epoch (typically system boot). The value is cast to `uint32_t`, which wraps around after ~136 years of uptime.

**Clock:** `std::chrono::steady_clock` (monotonic, never adjusted).

### `getCurrentTimeMillis`

```cpp
inline uint64_t getCurrentTimeMillis();
```

Returns the current time in milliseconds since the `std::chrono::steady_clock` epoch.

**Clock:** `std::chrono::steady_clock` (monotonic).

### `getCurrentTimeMicros`

```cpp
inline uint64_t getCurrentTimeMicros();
```

Returns the current time in microseconds since the Unix epoch (`1970-01-01T00:00:00Z`).

**Clock:** `std::chrono::system_clock` (wall-clock time, may be adjusted by NTP).

## Important: Clock Mismatch

`getCurrentTimeSeconds` and `getCurrentTimeMillis` use `steady_clock`, while `getCurrentTimeMicros` uses `system_clock`. Their return values have **different epoch references** and cannot be meaningfully compared or mixed in the same calculation. See [bugs.md](bugs.md).

## Usage Guide

| Use case | Function |
|----------|----------|
| Timeout / elapsed time measurement | `getCurrentTimeMillis()` |
| Rate limiting with short windows | `getCurrentTimeMillis()` |
| Wall-clock timestamp (for logs, events) | `getCurrentTimeMicros()` |
| Coarse uptime counter | `getCurrentTimeSeconds()` |
