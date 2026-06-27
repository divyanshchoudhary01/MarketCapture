#include <benchmark/benchmark.h>

static void BM_BasicMath(benchmark::State& state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(2 + 2);
    }
}

BENCHMARK(BM_BasicMath);

BENCHMARK_MAIN();
