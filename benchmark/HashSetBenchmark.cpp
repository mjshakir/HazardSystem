#include <benchmark/benchmark.h>
#include <iostream>
#include <memory>
#include <vector>

#include "HashSet.hpp"

using namespace HazardSystem;

class HashSetFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        capacity = static_cast<size_t>(state.range(0));
        set = std::make_unique<HashSet<int>>(capacity);
    }

    void TearDown(const ::benchmark::State&) override {
        set.reset();
    }

    size_t capacity{};
    std::unique_ptr<HashSet<int>> set;
};

// Insert a batch of unique keys
BENCHMARK_DEFINE_F(HashSetFixture, Insert)(benchmark::State& state) {
    const size_t baseline = capacity / 2;
    set->clear();
    for (size_t i = 0; i < baseline; ++i) {
        set->insert(static_cast<int>(i));
    }
    int next_key = static_cast<int>(capacity);

    for (auto _ : state) {
        const bool ok = set->insert(next_key);
        benchmark::DoNotOptimize(ok);
        if (!ok) {
            state.SkipWithError("insert failed (load cap reached)");
            break;
        }
        state.PauseTiming();
        set->remove(next_key);
        ++next_key;
        if (static_cast<size_t>(next_key) >= capacity * 4) {
            next_key = static_cast<int>(capacity);
        }
        state.ResumeTiming();
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

// Lookup all keys after a full insert
BENCHMARK_DEFINE_F(HashSetFixture, Contains)(benchmark::State& state) {
    set->clear();
    std::vector<int> queries;
    queries.reserve(capacity * 2);
    for (size_t i = 0; i < capacity; ++i) {
        set->insert(static_cast<int>(i));
        queries.push_back(static_cast<int>(i)); // present
    }
    for (size_t i = 0; i < capacity; ++i) {
        queries.push_back(static_cast<int>(capacity * 2 + i)); // miss
    }
    size_t idx = 0;

    for (auto _ : state) {
        benchmark::DoNotOptimize(set->contains(queries[idx]));
        idx = (idx + 1) % queries.size();
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

// Remove all keys after a full insert
BENCHMARK_DEFINE_F(HashSetFixture, Remove)(benchmark::State& state) {
    set->clear();
    std::vector<int> keys(capacity);
    for (size_t i = 0; i < capacity; ++i) {
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

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

// Traverse active buckets via for_each_fast
BENCHMARK_DEFINE_F(HashSetFixture, Iterate)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        for (size_t i = 0; i < capacity; ++i) {
            set->insert(static_cast<int>(i));
        }
        state.ResumeTiming();

        size_t visited = 0;
        set->for_each_fast([&](const int value) {
            benchmark::DoNotOptimize(value);
            ++visited;
        });
        benchmark::DoNotOptimize(visited);
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations() * capacity);
}

// Reclaim removes non-hazard values based on predicate
BENCHMARK_DEFINE_F(HashSetFixture, Reclaim)(benchmark::State& state) {
    const size_t hazard_stride = std::max<size_t>(1, capacity / 8);

    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        for (size_t i = 0; i < capacity; ++i) {
            set->insert(static_cast<int>(i));
        }
        state.ResumeTiming();

        set->reclaim([&](const int value) {
            return (static_cast<size_t>(value) % hazard_stride) == 0;
        });

        benchmark::DoNotOptimize(set->size());
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations() * capacity);
}

BENCHMARK_REGISTER_F(HashSetFixture, Insert)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(HashSetFixture, Contains)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(HashSetFixture, Remove)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(HashSetFixture, Iterate)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(HashSetFixture, Reclaim)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::oN);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== HashSet Benchmark ===\n";
    std::cout << "Exercises insert/lookup/remove/traversal/reclaim across capacities.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
