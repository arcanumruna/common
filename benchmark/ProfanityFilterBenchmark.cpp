// ProfanityFilter benchmark.
//
// Measures containsProfanity() and sanitizeInPlace() throughput for
// typical chat message lengths. These run on the hot path: every chat
// message goes through sanitizeInPlace before ring buffer commit.

#include <benchmark/benchmark.h>

#include <cstring>
#include <string>
#include <vector>

#include "filter/ProfanityFilter.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string makeCleanMessage(size_t length) {
    // Realistic chat: repeating "hello world " to fill length
    const std::string base = "hello world ";
    std::string msg;
    msg.reserve(length);
    while (msg.size() < length) {
        msg += base;
    }
    msg.resize(length);
    return msg;
}

static std::string makeDirtyMessage(size_t length) {
    // Message with profanity embedded in the middle
    const std::string prefix = "hey guys check this ";
    const std::string profane = "fuck";
    const std::string suffix = " out it is really cool and fun to play";
    std::string msg = prefix + profane + suffix;
    if (msg.size() < length) {
        msg.resize(length, ' ');
    } else {
        msg.resize(length);
    }
    return msg;
}

static std::string makeLeetMessage(size_t length) {
    // Leetspeak bypass attempt
    const std::string prefix = "hey wh4t th3 ";
    const std::string profane = "$h1t";
    const std::string suffix = " is going on in this game right now";
    std::string msg = prefix + profane + suffix;
    if (msg.size() < length) {
        msg.resize(length, ' ');
    } else {
        msg.resize(length);
    }
    return msg;
}

// Shared filter instance (constructed once, used by all benchmarks)
static ProfanityFilter& getFilter() {
    static ProfanityFilter filter(PROFANITY_WORD_LIST_PATH);
    return filter;
}

// ---------------------------------------------------------------------------
// BM_ContainsProfanity_Clean
//
// Worst case for containsProfanity: full scan, no match.
// ---------------------------------------------------------------------------
static void BM_ContainsProfanity_Clean(benchmark::State& state) {
    const auto len = static_cast<size_t>(state.range(0));
    const auto& filter = getFilter();
    std::string msg = makeCleanMessage(len);

    for (auto _ : state) {
        bool result = filter.containsProfanity(msg);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.counters["msg_len"] = benchmark::Counter(static_cast<double>(len));
}

BENCHMARK(BM_ContainsProfanity_Clean)
    ->Arg(16)->Arg(64)->Arg(128)->Arg(256)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// BM_ContainsProfanity_Dirty
//
// Best case: early match on embedded profanity.
// ---------------------------------------------------------------------------
static void BM_ContainsProfanity_Dirty(benchmark::State& state) {
    const auto len = static_cast<size_t>(state.range(0));
    const auto& filter = getFilter();
    std::string msg = makeDirtyMessage(len);

    for (auto _ : state) {
        bool result = filter.containsProfanity(msg);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.counters["msg_len"] = benchmark::Counter(static_cast<double>(len));
}

BENCHMARK(BM_ContainsProfanity_Dirty)
    ->Arg(16)->Arg(64)->Arg(128)->Arg(256)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// BM_ContainsProfanity_Leet
//
// Leetspeak normalization + match.
// ---------------------------------------------------------------------------
static void BM_ContainsProfanity_Leet(benchmark::State& state) {
    const auto len = static_cast<size_t>(state.range(0));
    const auto& filter = getFilter();
    std::string msg = makeLeetMessage(len);

    for (auto _ : state) {
        bool result = filter.containsProfanity(msg);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.counters["msg_len"] = benchmark::Counter(static_cast<double>(len));
}

BENCHMARK(BM_ContainsProfanity_Leet)
    ->Arg(16)->Arg(64)->Arg(128)->Arg(256)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// BM_SanitizeInPlace_Clean
//
// sanitizeInPlace on clean text — full scan, no masking.
// This is the common case for chat messages.
// ---------------------------------------------------------------------------
static void BM_SanitizeInPlace_Clean(benchmark::State& state) {
    const auto len = static_cast<size_t>(state.range(0));
    const auto& filter = getFilter();
    std::string original = makeCleanMessage(len);
    std::string msg(len, '\0');

    for (auto _ : state) {
        std::memcpy(msg.data(), original.data(), len);
        bool result = filter.sanitizeInPlace(msg.data(), msg.size());
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.counters["msg_len"] = benchmark::Counter(static_cast<double>(len));
}

BENCHMARK(BM_SanitizeInPlace_Clean)
    ->Arg(16)->Arg(64)->Arg(128)->Arg(256)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// BM_SanitizeInPlace_Dirty
//
// sanitizeInPlace on text with profanity — scan + mask.
// ---------------------------------------------------------------------------
static void BM_SanitizeInPlace_Dirty(benchmark::State& state) {
    const auto len = static_cast<size_t>(state.range(0));
    const auto& filter = getFilter();
    std::string original = makeDirtyMessage(len);
    std::string msg(len, '\0');

    for (auto _ : state) {
        std::memcpy(msg.data(), original.data(), len);
        bool result = filter.sanitizeInPlace(msg.data(), msg.size());
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
    state.counters["msg_len"] = benchmark::Counter(static_cast<double>(len));
}

BENCHMARK(BM_SanitizeInPlace_Dirty)
    ->Arg(16)->Arg(64)->Arg(128)->Arg(256)
    ->Unit(benchmark::kNanosecond);

// ---------------------------------------------------------------------------
// BM_SanitizeInPlace_BatchMessages
//
// Simulates a burst of N chat messages being sanitized sequentially,
// as ChatProcessor would process per tick.
// ---------------------------------------------------------------------------
static void BM_SanitizeInPlace_BatchMessages(benchmark::State& state) {
    const auto n = static_cast<size_t>(state.range(0));
    const auto& filter = getFilter();

    // Mix of clean and dirty messages (90% clean, 10% dirty — realistic ratio)
    std::vector<std::string> originals;
    originals.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (i % 10 == 0) {
            originals.push_back(makeDirtyMessage(80));
        } else {
            originals.push_back(makeCleanMessage(80));
        }
    }

    std::vector<std::string> messages(n);

    for (auto _ : state) {
        state.PauseTiming();
        for (size_t i = 0; i < n; ++i) {
            messages[i] = originals[i];
        }
        state.ResumeTiming();

        for (size_t i = 0; i < n; ++i) {
            filter.sanitizeInPlace(messages[i].data(), messages[i].size());
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
    state.counters["messages"] = benchmark::Counter(static_cast<double>(n));
}

BENCHMARK(BM_SanitizeInPlace_BatchMessages)
    ->Arg(10)->Arg(50)->Arg(100)->Arg(500)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_ContainsProfanity_CharacterName
//
// Character name validation: short strings (max 16 chars).
// Used at character creation time — not hot path but should be fast.
// ---------------------------------------------------------------------------
static void BM_ContainsProfanity_CharacterName(benchmark::State& state) {
    const auto& filter = getFilter();

    const std::string names[] = {
        "DarkKnight",
        "xXShadowXx",
        "NormalPlayer",
        "f4ckY0u",
        "CoolDude123",
        "sh1thead",
        "LegitName",
        "AssassinX"
    };
    const size_t nameCount = sizeof(names) / sizeof(names[0]);
    size_t idx = 0;

    for (auto _ : state) {
        bool result = filter.containsProfanity(names[idx % nameCount]);
        benchmark::DoNotOptimize(result);
        ++idx;
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ContainsProfanity_CharacterName)
    ->Unit(benchmark::kNanosecond);
