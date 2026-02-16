# Collection

**Header:** `include/collection/ChunkedList.h`

## `ChunkedList<T>`

A singly-linked list of fixed-size chunks. Elements are stored contiguously within each chunk, enabling efficient iteration while avoiding large contiguous allocations.

```cpp
template<typename T>
class ChunkedList;
```

### Constructor

```cpp
explicit ChunkedList(size_t chunkSize = 64, size_t preallocateChunks = 1);
```

- `chunkSize` — Number of elements per chunk.
- `preallocateChunks` — Number of chunks allocated upfront (reduces early reallocation).

### Member Functions

#### `push_back`

```cpp
void push_back(const T& value);
```

Appends `value` to the end. Allocates a new chunk automatically when the current tail chunk is full.

#### `clear`

```cpp
void clear();
```

Resets the element count to zero. Allocated chunks are retained for reuse; no memory is freed.

#### `size`

```cpp
size_t size() const;
```

Returns the total number of elements currently stored.

#### `forEach`

```cpp
template<typename Fn>
void forEach(Fn&& fn) const;
```

Calls `fn(element)` for every element in insertion order. Equivalent to a range-for loop.

#### `begin` / `end`

```cpp
const_iterator begin() const;
const_iterator end() const;
```

Returns forward-only const iterators. Compatible with range-for and standard algorithms.

### Iterator

`const_iterator` satisfies `std::forward_iterator_tag`. It skips over preallocated but empty chunks automatically. Dereference returns `const T&`.

### Design Notes

- Chunks are heap-allocated and owned via `std::unique_ptr`; no manual memory management is required.
- `clear()` does not release chunk memory — call the destructor or let the object go out of scope to free all memory.
- No mutable iterator is provided; to modify elements in-place, store pointers or use a different container.
- Not thread-safe.

### Example

```cpp
ChunkedList<int> list(/*chunkSize=*/32, /*preallocate=*/2);
list.push_back(1);
list.push_back(2);

for (int v : list) {
    // process v
}

list.clear(); // reuse chunk memory
```
