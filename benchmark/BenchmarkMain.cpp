// Single translation unit that provides BENCHMARK_MAIN() for the entire
// common_benchmark executable. All other benchmark TUs register their
// benchmarks via the BENCHMARK() macro without defining main().
#include <benchmark/benchmark.h>

BENCHMARK_MAIN();
