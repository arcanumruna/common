# Test Coverage Plan — target ≥ 90%

## Coverage gap analysis

| Component | Estimated coverage before | Gap |
|-----------|--------------------------|-----|
| `RateLimiter` | ~85% | Whitelist activity-timer fix; violation count > 3 |
| `MessageBatchView` | ~80% | Malformed input (zero-length, truncated) |
| `FrameSyncIdleStrategy` | ~70% | `idle(workCount)`, `reset()` |
| `FrameSyncBackoffIdleStrategy` | 0% | Entirely untested |
| `RunningAvg` | 0% | Entirely untested |
| `ChunkedList` | 0% | Entirely untested |
| `Configuration` | 0% | Entirely untested |
| `Files` | 0% | Entirely untested |
| `ScopeExit` | 0% | Entirely untested |
| `NumberUtils` | 0% | Entirely untested |
| `StringBuilder` | 0% | Entirely untested |
| `Time` | 0% | Entirely untested |
| `IdGenerator` | 0% | Entirely untested |
| `FastRNG` | 0% | Entirely untested |
| `SnapshotStore` | 0% | Entirely untested |
| `Network` (`getHostname`) | 0% | Entirely untested |
| Agrona concurrent structs | 0% | Out of scope (require full Aeron infrastructure) |

---

## New test files (13 files)

### `tests/network/RunningAvgTest.cpp`
| Test | Description |
|------|-------------|
| `DefaultAvgIsOneMilli` | freshly-constructed `avg()` returns 1 ms |
| `AddAndAvgReflectsValues` | add 10 equal durations, `avg()` matches |
| `CircularOverwriteWorks` | add > 10 samples, oldest overwritten correctly |
| `DampenAppliedWhenAboveThreshold` | fill with 15 ms samples, call `dampenForLowResTicker`, avg drops by factor 0.9 |
| `DampenNotAppliedWhenBelowThreshold` | fill with 1 ms samples, dampen is a no-op |

### `tests/network/FrameSyncBackoffIdleStrategyTest.cpp`
| Test | Description |
|------|-------------|
| `BackoffReducesFpsOnNoWork` | `idle(0)` repeatedly halves FPS; verify interval grows |
| `BackoffRestoresOnWork` | back off, then `idle(1)` restores default FPS |
| `SetFpsUpdatesInterval` | `setFps(20)` changes target |
| `SetFpsIgnoresNonPositive` | `setFps(0)` / `setFps(-1)` leave FPS unchanged |
| `ResetIsNoOp` | `reset()` does not crash or alter state |
| `IdleNoArgsPacesCorrectly` | basic frame pacing (low FPS, short run) |

### `tests/network/SnapshotStoreTest.cpp`
| Test | Description |
|------|-------------|
| `EmptyStoreHasSizeZero` | `getSize() == 0` on construction |
| `AddIncreasesSize` | each `addSnapshot` increments size up to Capacity |
| `FloorReturnsMostRecentAtOrBefore` | multiple entries; correct one returned |
| `FloorReturnsFalseWhenAllAfter` | all timestamps > query |
| `CeilReturnsOldestAtOrAfter` | correct entry returned |
| `CeilReturnsFalseWhenAllBefore` | all timestamps < query |
| `GetLastSnapshotReturnsMostRecent` | matches last `addSnapshot` value |
| `GetLastSnapshotReturnsFalseWhenEmpty` | empty store |
| `GetSecondLastSnapshotWorks` | size ≥ 2 case |
| `GetSecondLastReturnsFalseWhenFewerThan2` | size 0 and size 1 |
| `CircularOverwriteWrapsCorrectly` | add Capacity+1 entries; oldest overwritten |

### `tests/network/IdGeneratorTest.cpp`
| Test | Description |
|------|-------------|
| `Next32StartsAt1AndIncrements` | 1, 2, 3, … |
| `Next16StartsAt1AndIncrements` | 1, 2, 3, … |
| `CurrentPeeksWithoutAdvancing` | `current()` same value twice |
| `Current16PeeksWithoutAdvancing` | `current16()` same value twice |
| `ResetRestores32Stream` | reset to 5, next() == 5 |
| `Reset16Restores16Stream` | reset16 to 10, next16() == 10 |
| `ResetAllRestoresBothStreams` | both streams reset atomically |
| `StreamsAreIndependent` | advancing 32-bit does not affect 16-bit |

### `tests/network/FastRNGTest.cpp`
| Test | Description |
|------|-------------|
| `NextProducesNonZeroValues` | 100 calls, at least one non-zero |
| `StateChangesOnEachCall` | consecutive results differ (statistically) |
| `NextRangeRespectsBounds` | 1000 calls to `next(5,10)`, all in [5,10) |
| `NextRangeMinEqualsMaxReturnsMin` | `next(7,7) == 7` |
| `NextFloatInZeroToOne` | 1000 calls to `nextFloat()`, all in [0,1) |
| `RollD6InRange1To6` | 1000 calls to `rollD6()`, all in [1,6] |
| `ZeroStateTriggersClockreSeed` | `state=0` on construction; first `next()` succeeds and returns non-zero |

### `tests/network/NetworkTest.cpp`
| Test | Description |
|------|-------------|
| `GetHostnameReturnsNonEmptyString` | result is not empty and not `"unknown-host"` on a normal host |

### `tests/collection/ChunkedListTest.cpp`
| Test | Description |
|------|-------------|
| `EmptyListHasSizeZero` | `size() == 0` |
| `PushBackIncreasesSize` | push N elements, `size() == N` |
| `IterationInInsertionOrder` | range-for yields elements in insertion order |
| `ForEachVisitsAllElements` | count via `forEach` matches `size()` |
| `ClearResetsSize` | `clear()` → `size() == 0` |
| `ClearAllowsReuse` | push after clear produces correct values |
| `MultipleChunksAllocatedAutomatically` | push > chunkSize items; all readable |
| `PreallocatedChunksUsedFirst` | preallocate 3, push items spanning all chunks |
| `IteratorSkipsEmptyPreallocatedChunks` | preallocate 3, push only in first chunk; iterator visits correct elements |

### `tests/configuration/ConfigurationTest.cpp`
| Test | Description |
|------|-------------|
| `GetEnvReturnsValueWhenSet` | `setenv`, then `getEnv` returns it |
| `GetEnvReturnsDefaultWhenAbsent` | unset var + default string |
| `GetEnvThrowsWhenAbsentAndNoDefault` | unset var, no default → `std::runtime_error` |
| `GetEnvNumIntReturnsValue` | `setenv("X","42",1)`, `getEnvNum<int>("X",0) == 42` |
| `GetEnvNumIntReturnsDefaultWhenAbsent` | unset → default returned |
| `GetEnvNumIntThrowsOnInvalidInput` | `setenv` to `"abc"` → throws |
| `GetEnvNumFloatReturnsValue` | `"3.14"` → `≈ 3.14f` |
| `GetEnvNumFloatThrowsOnInvalidInput` | `"xyz"` → throws |

### `tests/file/FilesTest.cpp`
| Test | Description |
|------|-------------|
| `CreatesNewFileZeroInitialized` | create in `/tmp`, verify mmap returns zeroed memory, cleanup |
| `OpensExistingFilePreservesContent` | write known bytes, close, reopen via `createMmapBuffer`, verify bytes |
| `ThrowsOnInvalidDirectory` | pass path `/nonexistent_dir/x.bin` → `std::runtime_error` |

### `tests/flow/ScopeExitTest.cpp`
| Test | Description |
|------|-------------|
| `CallableInvokedOnScopeExit` | counter incremented when guard goes out of scope |
| `DismissPreventsInvocation` | `dismiss()` → counter NOT incremented |
| `MakeScopeExitFactory` | factory helper produces working guard |
| `NestedGuardsFireInReverseOrder` | LIFO destruction order |

### `tests/numeric/NumberUtilsTest.cpp`
| Test | Description |
|------|-------------|
| `FastAtoiNormal` | `"123"` → `123` |
| `FastAtoiStopsAtNonDigit` | `"12x4"` → `12` |
| `FastAtoiEmptyString` | `""` → `0` |
| `FastAtoi16CastsCorrectly` | `"100"` → `int16_t(100)` |
| `StringViewToUINT64Valid` | `"18446744073709551615"` → max |
| `StringViewToUINT64InvalidArgument` | `"abc"` → throws |
| `StringViewToUINT64OutOfRange` | `"99999999999999999999"` → throws |
| `StringViewToUINT64TrailingChars` | `"42abc"` → throws |
| `FloatToStringNormal` | `1.5f` → `"1.5"` |
| `FloatToStringZero` | `0.0f` → `"0"` |
| `FloatToStringNegative` | `-1.0f` → `"-1"` |
| `FloatToStringViewNormal` | stack buffer variant |
| `Uint32ToStringNormal` | `42` → `"42"` |
| `Uint32ToStringZero` | `0` → `"0"` |
| `Uint32ToStringMax` | `4294967295` → `"4294967295"` |
| `FastInvSqrtApproximate` | `|fast_inv_sqrt(4.0f) - 0.5f| < 0.002f` |
| `IsValidFloatNormal` | `1.0f` → true |
| `IsValidFloatNaN` | `NaN` → false |
| `IsValidFloatInf` | `Inf` and `-Inf` → false |

### `tests/text/StringBuilderTest.cpp`
| Test | Description |
|------|-------------|
| `AppendStringViewNormal` | produces correct string |
| `AppendStringViewTruncatesWhenFull` | string_view larger than remaining space |
| `AppendCharNormal` | single character appended |
| `AppendCharNoOpWhenFull` | no write past end |
| `AppendUint64Zero` | `0` → `"0"` |
| `AppendUint64SingleDigit` | `7` → `"7"` |
| `AppendUint64MultiDigit` | `1234567890` → correct string |
| `AppendUint64MaxValue` | `UINT64_MAX` → correct 20-digit string |
| `AppendUint64TruncatesWhenInsufficient` | only `avail` digits written, no overflow |
| `CStrNullTerminates` | `c_str()[len] == '\0'` |
| `CStrTruncatesLastCharWhenFull` | `len == capacity` case |
| `ViewReturnsCurrentContent` | `view()` matches pushed content |
| `ClearResetsLength` | `size() == 0` after `clear()` |
| `SizeReflectsLength` | matches manually counted chars |

### `tests/time/TimeTest.cpp`
| Test | Description |
|------|-------------|
| `GetCurrentTimeSecondsReturnsNonZero` | > 0 |
| `GetCurrentTimeMillisReturnsNonZero` | > 0 |
| `GetCurrentTimeMicrosReturnsNonZero` | > 0 |
| `TimeMillisIsMonotonic` | two calls: second ≥ first |
| `TimeMicrosIsMonotonic` | two calls: second ≥ first |
| `MicrosResolutionFinerThanMillis` | micros value > millis value (same steady_clock epoch) |

---

## Existing test files to extend

### `tests/network/RateLimiterTest.cpp` — add 2 tests
| Test | Description |
|------|-------------|
| `WhitelistUpdatesActivityTime` | send whitelisted messages only; `isInactive()` remains false immediately after (verifies BUG-12 fix) |
| `ViolationCountContinuesBeyond3` | 5 violations in window; 4th and 5th also return `-3` |

### `tests/network/MessageBatchViewTest.cpp` — add 2 tests
| Test | Description |
|------|-------------|
| `MalformedZeroLengthStopsIteration` | inject message with `length == 0`; iterator stops immediately |
| `TruncatedPayloadStopsIteration` | buffer advertises 10 bytes payload but only 3 bytes present |

### `tests/network/FrameSyncIdleStrategyTest.cpp` — add 2 tests
| Test | Description |
|------|-------------|
| `IdleWithWorkCountDelegatesToIdle` | `idle(0)` and `idle(5)` both complete without hanging |
| `ResetIsNoOp` | `reset()` does not crash |

---

## `CMakeLists.txt` changes

Add new source files to the `common_tests` target:

```
tests/network/RunningAvgTest.cpp
tests/network/FrameSyncBackoffIdleStrategyTest.cpp
tests/network/SnapshotStoreTest.cpp
tests/network/IdGeneratorTest.cpp
tests/network/FastRNGTest.cpp
tests/network/NetworkTest.cpp
tests/collection/ChunkedListTest.cpp
tests/configuration/ConfigurationTest.cpp
tests/file/FilesTest.cpp
tests/flow/ScopeExitTest.cpp
tests/numeric/NumberUtilsTest.cpp
tests/text/StringBuilderTest.cpp
tests/time/TimeTest.cpp
```

---

## Out of scope
- `AgronaAtomicBuffer`, `M2ORingBuffer`, `O2ORingBuffer`, `M2OConcurrentArrayQueue` — require a running Aeron media driver and substantial infra; excluded from this plan.
