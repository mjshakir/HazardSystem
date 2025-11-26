#include <benchmark/benchmark.h>
#include <iostream>
#include <memory>
#include <vector>
#include <atomic>

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
    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        state.ResumeTiming();

        for (size_t i = 0; i < capacity; ++i) {
            set->insert(static_cast<int>(i));
        }
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations() * capacity);
}

// Lookup all keys after a full insert
BENCHMARK_DEFINE_F(HashSetFixture, Contains)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        for (size_t i = 0; i < capacity; ++i) {
            set->insert(static_cast<int>(i));
        }
        state.ResumeTiming();

        for (size_t i = 0; i < capacity; ++i) {
            benchmark::DoNotOptimize(set->contains(static_cast<int>(i)));
        }
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations() * capacity);
}

// Remove all keys after a full insert
BENCHMARK_DEFINE_F(HashSetFixture, Remove)(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        set->clear();
        for (size_t i = 0; i < capacity; ++i) {
            set->insert(static_cast<int>(i));
        }
        state.ResumeTiming();

        for (size_t i = 0; i < capacity; ++i) {
            set->remove(static_cast<int>(i));
        }
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations() * capacity);
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
        set->for_each_fast([&](const std::shared_ptr<int>& value) {
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
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(HashSetFixture, Contains)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(HashSetFixture, Remove)
    ->RangeMultiplier(2)
    ->Range(128, 4096)
    ->Complexity(benchmark::oN);

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
