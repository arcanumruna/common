# Text

**Header:** `include/text/StringBuilder.h`

## `StringBuilder`

High-performance string builder backed by a caller-provided stack or heap buffer. Never allocates memory internally.

```cpp
struct StringBuilder {
    char*  buf;
    size_t capacity;
    size_t len;

    StringBuilder(char* buf_, size_t cap);
};
```

### Constructor

```cpp
StringBuilder(char* buf_, size_t cap);
```

Wraps the caller-supplied buffer `buf_` of `cap` bytes. `len` starts at `0`.

### Append Methods

```cpp
void append(std::string_view sv);
```
Copies up to `capacity - len` bytes from `sv`. Silently truncates if the remaining space is insufficient.

```cpp
void append(char c);
```
Appends a single character. No-op if the buffer is full.

```cpp
void append(uint64_t value);
```
Appends the decimal representation of `value`. Uses a pre-computed digit-pair table for efficiency.

**Warning:** When the number's digit count exceeds the available buffer space, digits are written beyond the valid region before the length is clamped. See [bugs.md](bugs.md).

### Inspection Methods

```cpp
const char* c_str() const;
```
Null-terminates the buffer and returns a pointer to it. If the buffer is full (`len == capacity`), the last byte is overwritten with `'\0'`, truncating the last character.

```cpp
size_t size() const;
```
Returns the current number of characters (not counting any null terminator).

```cpp
std::string_view view() const;
```
Returns a `std::string_view` over the current content without null-terminating.

```cpp
void clear();
```
Resets `len` to `0`. Buffer contents are not zeroed.

### Performance Notes

- `kDigitPairs[200]` is a `static constexpr` table of all two-digit pairs `"00"` through `"99"`.
- `uint64_to_chars_unsafe` writes digits two-at-a-time from the end of the number.
- `fast_num_digits` computes the digit count in O(1) via a chain of comparisons.

### Example

```cpp
char storage[64];
StringBuilder sb(storage, sizeof(storage));

sb.append("Player ");
sb.append(uint64_t(42));
sb.append(" scored");

// Use as C-string:
printf("%s\n", sb.c_str());

// Or as string_view (no null-termination overhead):
log(sb.view());
```
