# common — C++ Utility Library

Header-only C++23 utility library for high-performance, real-time applications (game servers, low-latency networking, message passing systems).

## Modules

| Module | Header | Description |
|--------|--------|-------------|
| [Configuration](docs/configuration.md) | `include/configuration/Configuration.h` | Environment variable parsing |
| [Collection](docs/collection.md) | `include/collection/ChunkedList.h` | Memory-efficient chunked list |
| [Concurrent](docs/concurrent.md) | `include/concurrent/agrona/` | Lock-free queues and ring buffers |
| [File](docs/file.md) | `include/file/Files.h` | Memory-mapped file utilities |
| [Flow](docs/flow.md) | `include/flow/ScopeExit.h` | RAII scope-exit guard |
| [Network](docs/network.md) | `include/network/` | Networking and game-loop utilities |
| [Numeric](docs/numeric.md) | `include/numeric/NumberUtils.h` | Fast number conversion and math |
| [Text](docs/text.md) | `include/text/StringBuilder.h` | High-performance string builder |
| [Time](docs/time.md) | `include/time/Time.h` | Lightweight time utilities |

## Build

The library is header-only and distributed as a Conan package (`common/1.0.0`).

```bash
# Install dependencies and build
./build.sh

# CMake (after conan install)
cmake --preset conan-release
cmake --build build/Release
```

Requires:
- C++23 compiler
- [Aeron](https://github.com/real-logic/aeron) C++ client library
- CMake 3.10+
- Conan 2.x

## Testing

Tests use GoogleTest and are built when `-DBUILD_TESTING=ON` is passed to CMake:

```bash
cmake -DBUILD_TESTING=ON ...
ctest --test-dir build
```

Test coverage: `FrameSyncIdleStrategy`, `MessageBatchView`, `RateLimiter`.
