#include <benchmark/benchmark.h>
#include <vector>
#include "HazardRegistry.hpp"

using namespace HazardSystem;

static void BM_HazardRegistry_AddRemove(benchmark::State& state) {
    const size_t hazards = static_cast<size_t>(state.range(0));
    HazardRegistry<int> registry(hazards);
    std::vector<int> items(hazards);
    std::vector<int*> ptrs(hazards);
    for (size_t i = 0; i < hazards; ++i) {
        items[i] = static_cast<int>(i);
        ptrs[i]  = &items[i];
    }

    for (auto _ : state) {
        for (auto* p : ptrs) {
            benchmark::DoNotOptimize(registry.add(p));
        }
        for (auto* p : ptrs) {
            benchmark::DoNotOptimize(registry.remove(p));
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(hazards) * 2);
    state.SetComplexityN(static_cast<benchmark::ComplexityN>(hazards));
}

static void BM_HazardRegistry_Contains(benchmark::State& state) {
    const size_t hazards = static_cast<size_t>(state.range(0));
    HazardRegistry<int> registry(hazards);
    std::vector<int> items(hazards);
    std::vector<int*> ptrs(hazards);
    for (size_t i = 0; i < hazards; ++i) {
        items[i] = static_cast<int>(i);
        ptrs[i]  = &items[i];
        registry.add(ptrs[i]);
    }

    for (auto _ : state) {
        for (auto* p : ptrs) {
            benchmark::DoNotOptimize(registry.contains(p));
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(hazards));
    state.SetComplexityN(static_cast<benchmark::ComplexityN>(hazards));
}

static void BM_HazardRegistry_Contains_Contended(benchmark::State& state) {
    const size_t hazards = static_cast<size_t>(state.range(0));
    HazardRegistry<int> registry(hazards);
    std::vector<int> items(hazards);
    std::vector<int*> ptrs(hazards);
    for (size_t i = 0; i < hazards; ++i) {
        items[i] = static_cast<int>(i);
        ptrs[i]  = &items[i];
        registry.add(ptrs[i]);
    }

    for (auto _ : state) {
        size_t idx = 0;
        for (auto* p : ptrs) {
            // mix add/remove to create tombstones during contains
            registry.remove(ptrs[idx]);
            registry.add(ptrs[idx]);
            benchmark::DoNotOptimize(registry.contains(p));
            ++idx;
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(hazards));
    state.SetComplexityN(static_cast<benchmark::ComplexityN>(hazards));
}

BENCHMARK(BM_HazardRegistry_AddRemove)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oN);

BENCHMARK(BM_HazardRegistry_Contains)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oN);

BENCHMARK(BM_HazardRegistry_Contains_Contended)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oN);

BENCHMARK_MAIN();
