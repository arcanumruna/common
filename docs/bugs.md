# Bug Report

All bugs listed below have been **fixed**. The table summarises where each fix was applied.

| ID | Severity | Status | File(s) changed |
|----|----------|--------|-----------------|
| BUG-1 | High | Fixed | `include/network/SnapshotStore.h` |
| BUG-2 | High | Fixed | `include/text/StringBuilder.h` |
| BUG-3 | Medium | Fixed | `include/time/Time.h` |
| BUG-4 | Medium | Fixed | `include/network/FrameSyncIdleStrategy.h`, `include/network/FrameSyncBackoffIdleStrategy.h` |
| BUG-5 | Low | Fixed | `include/network/FastRNG.h` |
| BUG-6 | Low | Fixed | `include/network/FastRNG.h` |
| BUG-7 | Low | Fixed | `include/time/Time.h` |
| BUG-8 | Low | Fixed | `include/text/StringBuilder.h` |
| BUG-9 | Low | Fixed | `include/numeric/NumberUtils.h` |
| BUG-10 | Low | Fixed | `include/configuration/Configuration.h` |
| BUG-11 | Low | Fixed | `include/network/Network.h` |
| BUG-12 | Low | Fixed | `include/network/RateLimiter.h` |
| BUG-13 | Low | Fixed | `include/network/detail/RunningAvg.h` (new), `FrameSyncIdleStrategy.h`, `FrameSyncBackoffIdleStrategy.h` |

---

## BUG-1 — `SnapshotStore`: `const` correctness violation ✓ Fixed

**File:** `include/network/SnapshotStore.h:25, 36`
**Severity:** High

`floor()` and `ceil()` are `const` methods. `std::array::operator[]` returns `const_reference` in a const context, but the original code bound the result to a non-const `Entry&`, which is ill-formed and causes a compilation error.

**Fix:** Changed `Entry& e` to `const Entry& e` in both methods.

---

## BUG-2 — `StringBuilder::append(uint64_t)`: buffer overflow before capacity check ✓ Fixed

**File:** `include/text/StringBuilder.h:37-44`
**Severity:** High

`uint64_to_chars_unsafe` wrote digits directly into the destination buffer before checking the available capacity. If the number required more digits than available space, bytes were written past the end of the buffer, causing undefined behaviour.

**Fix:** The number is now written into a local `char tmp[20]` first; only the clamped portion is then copied into the buffer with `memcpy`.

---

## BUG-3 — `Time.h`: `system_clock` / `steady_clock` mismatch ✓ Fixed

**File:** `include/time/Time.h:18-22`
**Severity:** Medium

`getCurrentTimeSeconds()` and `getCurrentTimeMillis()` used `steady_clock` (monotonic, epoch = boot), while `getCurrentTimeMicros()` used `system_clock` (wall-clock, epoch = Unix epoch). Mixing values from these functions in the same calculation produced incorrect results.

**Fix:** `getCurrentTimeMicros()` now also uses `steady_clock`. A comment notes that callers needing wall-clock timestamps should use `std::chrono::system_clock` directly.

---

## BUG-4 — `FrameSyncIdleStrategy` / `FrameSyncBackoffIdleStrategy`: broken Windows timer-resolution workaround ✓ Fixed

**File:** `include/network/FrameSyncIdleStrategy.h:118-122`
**File:** `include/network/FrameSyncBackoffIdleStrategy.h:147-151`
**Severity:** Medium (Windows only)

The original code used `static std::thread(lambda).detach()` — applying `static` to an unnamed temporary is ill-formed in standard C++. Even if accepted by a compiler, calling `.detach()` on a temporary that is immediately destroyed is unreliable.

**Fix:** The thread is now stored in a named `static std::thread s_timerResolutionThread(...)` variable. A second `static bool s_detached` guards the single call to `.detach()` via the comma-operator initialiser, ensuring the thread is detached exactly once regardless of how many `FrameSync*` objects are created.

---

## BUG-5 — `FastRNG`: unused `seed` field ✓ Fixed

**File:** `include/network/FastRNG.h:5`
**Severity:** Low

The `seed` field was declared and initialised to `0xdeadbeef` but never referenced in any method, increasing struct size and misleading callers.

**Fix:** Field removed.

---

## BUG-6 — `FastRNG`: missing `#include <chrono>` and `#include <cstdint>` ✓ Fixed

**File:** `include/network/FastRNG.h`
**Severity:** Low

`next()` used `std::chrono::steady_clock` and `uint32_t` without including their respective headers.

**Fix:** Added `#include <chrono>` and `#include <cstdint>`. Also added an explicit `ull` suffix to the multiplier constant (`0x2545F4914F6CDD1Dull`) to make the intended 64-bit arithmetic unambiguous.

---

## BUG-7 — `Time.h`: missing standard includes ✓ Fixed

**File:** `include/time/Time.h`
**Severity:** Low

The file used `std::chrono`, `uint32_t`, and `uint64_t` without any `#include` directives.

**Fix:** Added `#include <chrono>` and `#include <cstdint>`. (Fixed together with BUG-3.)

---

## BUG-8 — `StringBuilder.h`: missing `#include <cstring>` ✓ Fixed

**File:** `include/text/StringBuilder.h`
**Severity:** Low

`memcpy` was called without including `<cstring>`.

**Fix:** Added `#include <cstring>`. (Fixed together with BUG-2.)

---

## BUG-9 — `NumberUtils.h`: missing `#include <cstring>` ✓ Fixed

**File:** `include/numeric/NumberUtils.h`
**Severity:** Low

`std::memcpy` was used in `fast_inv_sqrt` (non-SSE path) and `is_valid_float` without including `<cstring>`.

**Fix:** Added `#include <cstring>`.

---

## BUG-10 — `Configuration.h`: missing `#include <stdexcept>` ✓ Fixed

**File:** `include/configuration/Configuration.h`
**Severity:** Low

`std::runtime_error` was thrown without including `<stdexcept>`.

**Fix:** Added `#include <stdexcept>`.

---

## BUG-11 — `Network.h`: unused `#include <vector>` ✓ Fixed

**File:** `include/network/Network.h`
**Severity:** Low

`<vector>` was included but `std::vector` was not used anywhere in the file.

**Fix:** Removed the include.

---

## BUG-12 — `RateLimiter`: whitelisted messages bypass the activity timer ✓ Fixed

**File:** `include/network/RateLimiter.h:36-40`
**Severity:** Low

Whitelisted messages (e.g. `KEEPALIVE`) returned `0` immediately without updating `lastActivityTime`. A client sending only whitelisted messages would be marked inactive and evicted after 60 seconds despite actively communicating.

**Fix:** `lastActivityTime` is now updated for whitelisted messages before returning `0`. Token consumption and violation tracking remain bypassed.

---

## BUG-13 — `RunningAvg` duplicated across two headers ✓ Fixed

**File:** `include/network/FrameSyncIdleStrategy.h`
**File:** `include/network/FrameSyncBackoffIdleStrategy.h`
**Severity:** Low

`RunningAvg` was a private inner class copied verbatim into both strategy headers. Any change had to be made twice.

**Fix:** Extracted into `include/network/detail/RunningAvg.h` under namespace `aeron::concurrent::detail`. Both strategy headers now include it. The `add()` method was also simplified from the redundant double-modulo form to the cleaner `values_[index_] = duration; index_ = (index_ + 1) % SLOT_COUNT;`.
