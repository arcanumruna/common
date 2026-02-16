# Flow

**Header:** `include/flow/ScopeExit.h`

RAII utilities for guaranteed cleanup on scope exit.

## `ScopeExit<F>`

Executes a callable `F` when the `ScopeExit` object is destroyed (i.e., when control leaves the enclosing scope).

```cpp
template<typename F>
class ScopeExit;
```

### Constructor

```cpp
explicit ScopeExit(F&& f) noexcept;
```

Stores `f` and marks the guard as active.

### Destructor

```cpp
~ScopeExit() noexcept;
```

If the guard is active, calls `func()`. Exceptions thrown by `func` are silently swallowed (destructor is `noexcept`).

### `dismiss`

```cpp
void dismiss() noexcept;
```

Deactivates the guard. The stored callable will not be invoked on destruction. Use this when the cleanup is no longer needed (e.g., after a successful operation).

---

## `make_scope_exit`

```cpp
template<typename F>
ScopeExit<F> make_scope_exit(F&& f);
```

Factory function that constructs a `ScopeExit<F>` with perfect forwarding.

### Example

```cpp
int fd = open(...);
auto guard = make_scope_exit([&]{ close(fd); });

// ... work that may throw ...

guard.dismiss(); // don't close fd if we want to keep it open
```

### Notes

- `ScopeExit` is not copyable or movable (the callable is stored by value, and the guard is tied to its original scope).
- Prefer `make_scope_exit` over direct construction to benefit from template argument deduction.
