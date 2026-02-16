# File

**Header:** `include/file/Files.h`

Utilities for creating memory-mapped file buffers (POSIX only).

## `createMmapBuffer`

```cpp
uint8_t* createMmapBuffer(const char* filepath, size_t size, int& outFd);
```

Opens (or creates) a file at `filepath`, maps `size` bytes into process memory, and returns a pointer to that region.

**Parameters:**
- `filepath` — Path to the file. Created if it does not exist.
- `size` — Number of bytes to map. The file is truncated to this size when newly created.
- `outFd` — Out-parameter; set to the file descriptor of the opened file.

**Returns:** Pointer to the mapped memory region (`PROT_READ | PROT_WRITE`, `MAP_SHARED`).

**Throws:** `std::runtime_error` — on any OS error (`stat`, `open`, `ftruncate`, `mmap`).

**Caller responsibilities:**
- Call `munmap(ptr, size)` when the mapping is no longer needed.
- Call `close(outFd)` when the file descriptor is no longer needed.
- The caller is responsible for managing the lifetime of both the file descriptor and the mapped region.

**Behaviour details:**
- If the file already exists, it is opened and mapped as-is (no resize, no zeroing).
- If the file is newly created, it is truncated to `size` and zero-initialised with `memset`.

**Example:**
```cpp
int fd = -1;
uint8_t* mem = createMmapBuffer("/tmp/shared.bin", 4096, fd);

// use mem ...

munmap(mem, 4096);
close(fd);
```
