#include <benchmark/benchmark.h>
#include <iostream>
#include <memory>
#include <vector>

#include "HashSet.hpp"

using namespace HazardSystem;

class HashSetFixedFixture : public benchmark::Fixture {
public:
    static constexpr size_t kFixedCapacity = 8192;

    void SetUp(const ::benchmark::State&) override {
        set = std::make_unique<HashSet<int, kFixedCapacity>>();
    }

    void TearDown(const ::benchmark::State&) override {
        set.reset();
    }

    std::unique_ptr<HashSet<int, kFixedCapacity>> set;
};

// Insert a batch of unique keys
BENCHMARK_DEFINE_F(HashSetFixedFixture, Insert)(benchmark::State& state) {
    const size_t workload = static_cast<size_t>(state.range(0));
    const size_t baseline = workload / 2;
    set->clear();
    for (size_t i = 0; i < baseline; ++i) {
        set->insert(static_cast<int>(i));
    }
    int next_key = static_cast<int>(workload);

    for (auto _ : state) {
        bool ok = set->insert(next_key);
        benchmark::DoNotOptimize(ok);
        if (!ok) {
            state.SkipWithError("insert failed (load cap reached)");
            break;
        }
        state.PauseTiming();
        set->remove(next_key);
        ++next_key;
        if (static_cast<size_t>(next_key) >= workload * 4) {
            next_key = static_cast<int>(workload);
        }
        state.ResumeTiming();
    }

    state.SetComplexityN(static_cast<int64_t>(workload));
    state.SetItemsProcessed(state.iterations());
}

// Lookup all keys after a full insert
BENCHMARK_DEFINE_F(HashSetFixedFixture, Contains)(benchmark::State& state) {
    const size_t workload = static_cast<size_t>(state.range(0));
    set->clear();
    std::vector<int> queries;
    queries.reserve(workload * 2);
    for (size_t i = 0; i < workload; ++i) {
        set->insert(static_cast<int>(i));
        queries.push_back(static_cast<int>(i)); // present
    }
    for (size_t i = 0; i < workload; ++i) {
        queries.push_back(static_cast<int>(workload * 2 + i)); // miss
    }
    size_t idx = 0;

    for (auto _ : state) {
        benchmark::DoNotOptimize(set->contains(queries[idx]));
        idx = (idx + 1) % queries.size();
    }

    state.SetComplexityN(static_cast<int64_t>(workload));
    state.SetItemsProcessed(state.iterations());
}

// Remove all keys after a full insert
BENCHMARK_DEFINE_F(HashSetFixedFixture, Remove)(benchmark::State& state) {
    const size_t workload = static_cast<size_t>(state.range(0));
    set->clear();
    std::vector<int> keys(workload);
    for (size_t i = 0; i < workload; ++i) {
        keys[i] = static_cast<int>(i);
        set->insert(keys[i]);
    }
    size_t idx = 0;

    for (auto _ : state) {
        const int key = keys[idx];
        benchmark::DoNotOptimize(set->remove(key));
        state.PauseTiming();
        set->insert(key);
        state.ResumeTiming();
        idx = (idx + 1) % keys.size();
    }

    state.SetComplexityN(static_cast<int64_t>(workload));
    state.SetItemsProcessed(state.iterations());
}

// Traverse active buckets via for_each_fast
BENCHMARK_DEFINE_F(HashSetFixedFixture, Iterate)(benchmark::State& state) {
    const size_t workload = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        for (size_t i = 0; i < workload; ++i) {
            set->insert(static_cast<int>(i));
        }
        state.ResumeTiming();

        size_t visited = 0;
        set->for_each_fast([&](int value) {
            benchmark::DoNotOptimize(value);
            ++visited;
        });
        benchmark::DoNotOptimize(visited);
    }

    state.SetComplexityN(static_cast<int64_t>(workload));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(workload));
}

// Reclaim removes non-hazard values based on predicate
BENCHMARK_DEFINE_F(HashSetFixedFixture, Reclaim)(benchmark::State& state) {
    const size_t workload = static_cast<size_t>(state.range(0));
    const size_t hazard_stride = std::max<size_t>(1, workload / 8);

    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        for (size_t i = 0; i < workload; ++i) {
            set->insert(static_cast<int>(i));
        }
        state.ResumeTiming();

        set->reclaim([&](const int value) {
            return (static_cast<size_t>(value) % hazard_stride) == 0;
        });

        benchmark::DoNotOptimize(set->size());
    }

    state.SetComplexityN(static_cast<int64_t>(workload));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(workload));
}

BENCHMARK_REGISTER_F(HashSetFixedFixture, Insert)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(HashSetFixedFixture, Contains)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(HashSetFixedFixture, Remove)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(HashSetFixedFixture, Iterate)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(HashSetFixedFixture, Reclaim)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::oN);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== HashSet Fixed Benchmark ===\n";
    std::cout << "Exercises insert/lookup/remove/traversal/reclaim on fixed-capacity table.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
