# Concurrent (Agrona)

**Headers:** `include/concurrent/agrona/`

Aeron-compatible lock-free data structures for inter-thread message passing.

---

## `AgronaAtomicBuffer`

**Header:** `include/concurrent/agrona/AgronaAtomicBuffer.h`

A bounds-checked wrapper around a raw byte buffer with atomic read/write operations. Compatible with the Aeron messaging framework.

### Constructors

```cpp
AgronaAtomicBuffer();
AgronaAtomicBuffer(std::uint8_t* buffer, std::size_t length);
AgronaAtomicBuffer(std::uint8_t* buffer, std::size_t length, std::uint8_t initialValue);
template<std::size_t N>
AgronaAtomicBuffer(std::array<std::uint8_t, N>& buffer);
```

The default constructor creates an empty (null) buffer; call `wrap()` before use.

### Key Methods

#### Introspection

```cpp
std::size_t capacity() const;
std::uint8_t* buffer() const;
```

#### Non-atomic Primitive Access

```cpp
std::int8_t   getInt8(std::size_t index) const;
std::int16_t  getInt16(std::size_t index) const;
std::int32_t  getInt32(std::size_t index) const;
std::int64_t  getInt64(std::size_t index) const;
float         getFloat(std::size_t index) const;

void putInt8 (std::size_t index, std::int8_t  value);
void putInt16(std::size_t index, std::int16_t value);
void putInt32(std::size_t index, std::int32_t value);
void putInt64(std::size_t index, std::int64_t value);
void putFloat(std::size_t index, float value);
```

Big-endian variants: `getInt32_BE`, `putInt32_BE`, `getFloat_BE`, `putFloat_BE`.

#### Atomic Operations

```cpp
// Ordered (release/acquire) reads and writes
std::int64_t getInt64Volatile(std::size_t index) const;
void         putInt64Ordered(std::size_t index, std::int64_t value);
std::int32_t getInt32Volatile(std::size_t index) const;
void         putInt32Ordered(std::size_t index, std::int32_t value);

// Full sequentially-consistent atomics
void putInt64Atomic(std::size_t index, std::int64_t value);
void putInt32Atomic(std::size_t index, std::int32_t value);

// Compare-and-set
bool compareAndSetInt64(std::size_t index, std::int64_t expected, std::int64_t desired);
bool compareAndSetInt32(std::size_t index, std::int32_t expected, std::int32_t desired);

// Atomic swap and fetch-add
std::int64_t getAndSetInt64(std::size_t index, std::int64_t value);
std::int32_t getAndSetInt32(std::size_t index, std::int32_t value);
std::int64_t getAndAddInt64(std::size_t index, std::int64_t delta);
std::int32_t getAndAddInt32(std::size_t index, std::int32_t delta);
```

#### Bulk Operations

```cpp
void putBytes(std::size_t index, const std::uint8_t* src, std::size_t length);
void getBytes(std::size_t index, std::uint8_t* dst, std::size_t length) const;
void setMemory(std::size_t index, std::size_t length, std::uint8_t value);
```

#### String Operations

```cpp
std::string getString(std::size_t index) const;           // length-prefixed
std::string getStringWithoutLength(std::size_t index, std::size_t length) const;
std::size_t putString(std::size_t index, const std::string& value);
std::size_t putStringWithoutLength(std::size_t index, const std::string& value);
```

#### Struct Overlay

```cpp
template<typename T>
T& overlayStruct(std::size_t offset = 0);
```

Reinterpret the buffer at `offset` as a reference to `T`. Useful for zero-copy message decoding.

#### Static Factory

```cpp
static AgronaAtomicBuffer mmap(const char* path, std::size_t length);
```

Creates a buffer backed by a memory-mapped file.

### Bounds Checking

Bounds checks are enabled by default. Define `DISABLE_BOUNDS_CHECKS` to remove them for maximum performance.

---

## `M2ORingBuffer`

**Header:** `include/concurrent/agrona/M2ORingBuffer.h`

Many-to-One (multiple producers, single consumer) lock-free ring buffer.

```cpp
explicit M2ORingBuffer(std::uint8_t* buffer, std::size_t length);
```

`length` must be a power of two plus `TRAILER_LENGTH` (from `AgronaRingBufferDescriptor`).

### Write (Producer) Side

```cpp
// Reserve space; returns write index on success, INSUFFICIENT_CAPACITY on failure.
aeron::util::index_t tryClaim(std::int32_t msgTypeId, size_t length);

// Publish the previously claimed slot.
void commit(aeron::util::index_t index);

// Discard the previously claimed slot.
void abort(aeron::util::index_t index);
```

Typical producer pattern:
```cpp
auto idx = rb.tryClaim(MY_MSG_TYPE, payloadSize);
if (idx != aeron::concurrent::ringbuffer::RecordDescriptor::INSUFFICIENT_CAPACITY) {
    // write payload into buffer at idx
    rb.commit(idx);
}
```

### Read (Consumer) Side

```cpp
// Read up to messageCountLimit messages, invoking handler for each.
int read(const agrona_handler_t& handler, int messageCountLimit);

// Controlled read; handler returns ControlledAction (ABORT/BREAK/COMMIT/CONTINUE).
int controlledRead(const agrona_controlled_handler_t& handler, int messageCountLimit);

// Read all available messages.
int read(const agrona_handler_t& handler);
```

Handler signature:
```cpp
// agrona_handler_t
void handler(std::int32_t msgTypeId, AgronaAtomicBuffer& buffer,
             aeron::util::index_t offset, aeron::util::index_t length);
```

### Metadata

```cpp
std::int64_t nextCorrelationId();       // Unique correlation ID
std::int64_t consumerHeartbeatTime();   // Last consumer activity timestamp
void consumerHeartbeatTime(std::int64_t time);
std::int64_t producerPosition();        // Current tail position
std::int64_t consumerPosition();        // Current head position
int size();                             // Approximate message count in flight
std::size_t maxMsgLength();             // Maximum single-message payload size
```

---

## `O2ORingBuffer`

**Header:** `include/concurrent/agrona/O2ORingBuffer.h`

One-to-One (single producer, single consumer) ring buffer. Identical API to `M2ORingBuffer` but optimised: the producer path is CAS-free, making it faster when only one thread writes.

Use `O2ORingBuffer` instead of `M2ORingBuffer` whenever there is exactly one producer thread.

---

## `M2OConcurrentArrayQueue<E>`

**Header:** `include/concurrent/agrona/M2OConcurrentArrayQueue.h`

Bounded, lock-free, many-producer / single-consumer FIFO queue backed by a circular array.

```cpp
template<typename E>
class M2OConcurrentArrayQueue;

explicit M2OConcurrentArrayQueue(std::size_t requested_capacity);
```

Actual capacity is rounded up to the next power of two.

### Methods

```cpp
bool offer(const E& element);                                    // Enqueue; returns false if full
E*   poll();                                                     // Dequeue; returns nullptr if empty
int  drain(std::function<void(E*)> consumer, int limit);         // Batch dequeue up to limit
bool is_empty() const;
int  size() const;                                               // Approximate count
std::size_t capacity() const;
```

### Implementation Notes

- Producer `tail_` and consumer `head_` are on separate cache lines to prevent false sharing.
- `shared_head_cache_` reduces head reads by producers.
- Memory ordering: `relaxed` for non-synchronising loads, `acquire`/`release` for ordering guarantees.

---

## `AgronaRingBufferDescriptor`

**Header:** `include/concurrent/agrona/AgronaRingBufferDescriptor.h`

Constants and validation for ring buffer metadata layout.

```cpp
// Byte offsets within the trailer region at the end of the buffer
TAIL_POSITION_OFFSET          // = CACHE_LINE_LENGTH * 2
HEAD_CACHE_POSITION_OFFSET    // = CACHE_LINE_LENGTH * 4
HEAD_POSITION_OFFSET          // = CACHE_LINE_LENGTH * 6
CORRELATION_COUNTER_OFFSET    // = CACHE_LINE_LENGTH * 8
CONSUMER_HEARTBEAT_OFFSET     // = CACHE_LINE_LENGTH * 10
TRAILER_LENGTH                // = CACHE_LINE_LENGTH * 12

static void checkCapacity(aeron::util::index_t capacity);
// Throws aeron::util::IllegalArgumentException if capacity is not a power of two.
```

Type aliases:
```cpp
using agrona_handler_t           = std::function<void(int32_t, AgronaAtomicBuffer&, index_t, index_t)>;
using agrona_controlled_handler_t = std::function<ControlledAction(int32_t, AgronaAtomicBuffer&, index_t, index_t)>;
enum class ControlledAction { ABORT, BREAK, COMMIT, CONTINUE };
```
