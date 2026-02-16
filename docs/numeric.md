# Numeric

**Header:** `include/numeric/NumberUtils.h`

Fast number conversion utilities and math helpers.

---

## String-to-Number

### `fast_atoi`

```cpp
inline int fast_atoi(const char* s);
```

Converts a null-terminated ASCII decimal string to `int`. Stops at the first non-digit character.

- No sign handling (negative numbers are not supported).
- No overflow detection — undefined behaviour if the result exceeds `INT_MAX`.
- Faster than `std::atoi` because it skips locale handling.

### `fast_atoi16`

```cpp
inline int16_t fast_atoi16(const char* s);
```

Same as `fast_atoi` but casts the result to `int16_t`. No overflow detection.

### `stringViewToUINT64`

```cpp
inline uint64_t stringViewToUINT64(std::string_view sv);
```

Parses `sv` as an unsigned 64-bit integer using `std::from_chars`.

- Throws `std::runtime_error("Invalid input: not a valid number")` on non-numeric input.
- Throws `std::runtime_error("Number out of range for uint64_t")` on overflow.
- Throws `std::runtime_error("Invalid input: trailing characters")` if `sv` contains characters after the number.

---

## Number-to-String

### `floatToString`

```cpp
inline std::string floatToString(float f);
```

Converts `f` to a `std::string` using `std::to_chars` with `std::chars_format::general`.

Throws `std::runtime_error` if conversion fails (should not happen for finite values).

### `floatToStringView`

```cpp
inline std::string_view floatToStringView(float value, char* buffer, size_t bufferSize);
```

Converts `value` into the caller-provided `buffer` and returns a `std::string_view` over the result. No heap allocation.

- `buffer` must remain valid for the lifetime of the returned view.
- `bufferSize` should be at least 16 bytes for safety.
- Throws `std::runtime_error` if `bufferSize` is too small.

### `uint32ToString`

```cpp
inline std::string uint32ToString(uint32_t value);
```

Converts a `uint32_t` to a `std::string`. Uses a 10-byte stack buffer (sufficient for the maximum 10-digit value `4294967295`).

---

## Math

### `fast_inv_sqrt`

```cpp
inline float fast_inv_sqrt(float x);
```

Computes an approximate reciprocal square root of `x` (i.e. `1 / sqrt(x)`).

- On x86/x64 (`__SSE__` or `_M_X64`): uses `_mm_rsqrt_ss` — single instruction, ~1.5 ulp error.
- Otherwise: uses the Quake 3 magic-number algorithm with one Newton–Raphson iteration.

The result is an approximation; do not use where exact results are required.

### `is_valid_float`

```cpp
inline bool is_valid_float(float x);
```

Returns `true` if `x` is neither NaN nor infinity (i.e. the exponent bits are not all set). Uses a bitwise check via `std::memcpy` — avoids FPU exceptions.
