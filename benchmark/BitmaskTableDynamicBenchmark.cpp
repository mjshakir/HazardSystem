#include <benchmark/benchmark.h>
#include <array>
#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>

#include "BitmaskTable.hpp"

using namespace HazardSystem;

struct BenchmarkTestData {
    std::array<int, 16> data{};
    std::atomic<int> counter{0};

    explicit BenchmarkTestData(int seed = 0) {
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<int>(seed + static_cast<int>(i));
        }
    }

    void work() {
        counter.fetch_add(1, std::memory_order_relaxed);
        int sum = 0;
        for (auto v : data) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }
};

using DynamicTable = BitmaskTable<BenchmarkTestData, 0>;

class BitmaskDynamicFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        capacity = static_cast<size_t>(state.range(0));
        table = std::make_unique<DynamicTable>(capacity);
    }

    void TearDown(const ::benchmark::State&) override {
        table.reset();
    }

    size_t capacity{};
    std::unique_ptr<DynamicTable> table;
};

// Acquire + release against a growing bitmask vector
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, AcquireRelease)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(11);

    for (auto _ : state) {
        auto idx = table->acquire();
        benchmark::DoNotOptimize(idx);
        if (idx) {
            table->set(idx.value(), payload.get());
            table->release(idx.value());
        }
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

// Worst-case: fill the table completely; acquire should scan and fail.
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, AcquireFailWhenFull)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(19);
    const size_t cap = table->capacity();

    table->clear();
    for (size_t i = 0; i < cap; ++i) {
        table->set(i, payload.get());
    }

    for (auto _ : state) {
        constexpr size_t kBatch = 64;
        for (size_t i = 0; i < kBatch; ++i) {
            auto idx = table->acquire();
            benchmark::DoNotOptimize(idx);
        }
    }

    state.SetComplexityN(cap);
    state.SetItemsProcessed(state.iterations() * 64);
}

// Worst-case: only one free slot in the last mask word; acquire scans all previous words.
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, AcquireWorstCaseNearFull)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(23);
    const size_t cap = table->capacity();

    table->clear();
    for (size_t i = 0; i < cap; ++i) {
        table->set(i, payload.get());
    }

    const size_t last = cap - 1;
    table->set(last, nullptr);
    table->set(static_cast<size_t>(0), nullptr);
    table->set(static_cast<size_t>(0), payload.get());

    for (auto _ : state) {
        constexpr size_t kBatch = 64;
        for (size_t i = 0; i < kBatch; ++i) {
            auto idx = table->acquire();
            benchmark::DoNotOptimize(idx);
            if (idx) {
                table->release(idx.value());
                // Reset hint to part 0 without changing the single free-slot location.
                table->set(static_cast<size_t>(0), nullptr);
                table->set(static_cast<size_t>(0), payload.get());
            }
        }
    }

    state.SetComplexityN(cap);
    state.SetItemsProcessed(state.iterations() * 64);
}

// Iterate across active entries to exercise bitmask scanning logic
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, IterateActive)(benchmark::State& state) {
    const size_t fill_target = std::min<size_t>(capacity, 512);
    std::vector<std::unique_ptr<BenchmarkTestData>> owned;
    owned.reserve(fill_target);

    for (auto _ : state) {
        state.PauseTiming();
        table->clear();
        owned.clear();
        for (size_t i = 0; i < fill_target; ++i) {
            auto idx = table->acquire();
            if (!idx) break;
            owned.emplace_back(std::make_unique<BenchmarkTestData>(static_cast<int>(i)));
            table->set(idx.value(), owned.back().get());
        }
        state.ResumeTiming();

        size_t visited = 0;
        table->for_each_fast([&](DynamicTable::IndexType, BenchmarkTestData* ptr) {
            benchmark::DoNotOptimize(ptr);
            ptr->work();
            ++visited;
        });
        benchmark::DoNotOptimize(visited);
    }

    state.SetComplexityN(fill_target);
    state.SetItemsProcessed(state.iterations() * fill_target);
}

// Clear the dynamically sized table to capture cleanup overhead
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, Clear)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(5);

    for (auto _ : state) {
        state.PauseTiming();
        table->clear();
        for (size_t i = 0; i < capacity; ++i) {
            auto idx = table->acquire();
            if (!idx) {
                break;
            }
            table->set(idx.value(), payload.get());
        }
        state.ResumeTiming();

        table->clear();
        benchmark::DoNotOptimize(table->size());
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations() * capacity);
}

// Iterator-based acquisition + set + release
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, AcquireIteratorSet)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(13);

    for (auto _ : state) {
        auto it = table->acquire_iterator();
        benchmark::DoNotOptimize(it);
        if (!it) {
            continue;
        }
        table->set(it.value(), payload.get());
        const auto index = static_cast<DynamicTable::IndexType>(it.value() - table->begin());
        table->release(index);
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

// Active/at scanning on a prefilled dynamic table
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, ActiveChecks)(benchmark::State& state) {
    const size_t fill_count = std::min<size_t>(capacity, static_cast<size_t>(state.range(0)));
    std::vector<std::unique_ptr<BenchmarkTestData>> owned;
    owned.reserve(fill_count);

    for (auto _ : state) {
        state.PauseTiming();
        table->clear();
        owned.clear();
        for (size_t i = 0; i < fill_count; ++i) {
            auto idx = table->acquire();
            if (!idx) break;
            owned.emplace_back(std::make_unique<BenchmarkTestData>(static_cast<int>(i)));
            table->set(idx.value(), owned.back().get());
        }
        state.ResumeTiming();

        size_t hits = 0;
        for (size_t i = 0; i < capacity; ++i) {
            const bool active = table->active(static_cast<DynamicTable::IndexType>(i));
            if (active) {
                auto ptr = table->at(static_cast<DynamicTable::IndexType>(i));
                benchmark::DoNotOptimize(ptr);
                ++hits;
            }
        }
        benchmark::DoNotOptimize(hits);
    }

    state.SetComplexityN(fill_count);
    state.SetItemsProcessed(state.iterations() * fill_count);
}

// Predicate-based find across dynamic masks
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, FindPredicate)(benchmark::State& state) {
    constexpr int target_seed = 909;
    std::vector<std::unique_ptr<BenchmarkTestData>> owned;
    owned.reserve(capacity);

    for (auto _ : state) {
        state.PauseTiming();
        table->clear();
        const size_t fill_count = std::min<size_t>(capacity, 512);
        owned.clear();
        for (size_t i = 0; i < fill_count; ++i) {
            auto idx = table->acquire();
            if (!idx) break;
            const int seed = static_cast<int>(i == fill_count - 1 ? target_seed : static_cast<int>(i));
            owned.emplace_back(std::make_unique<BenchmarkTestData>(seed));
            table->set(idx.value(), owned.back().get());
        }
        state.ResumeTiming();

        const bool found = table->find([&](const BenchmarkTestData* ptr) {
            return ptr && ptr->data.front() == target_seed;
        });
        benchmark::DoNotOptimize(found);
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

// Emplace-return pair path with release
BENCHMARK_DEFINE_F(BitmaskDynamicFixture, EmplaceReturn)(benchmark::State& state) {
    BenchmarkTestData payload(17);
    for (auto _ : state) {
        auto idx = table->acquire();
        benchmark::DoNotOptimize(idx);
        if (idx) {
            table->set(idx.value(), &payload);
            table->release(idx.value());
        }
    }

    state.SetComplexityN(capacity);
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, AcquireRelease)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, AcquireFailWhenFull)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, AcquireWorstCaseNearFull)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, IterateActive)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, Clear)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, AcquireIteratorSet)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, ActiveChecks)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, FindPredicate)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

BENCHMARK_REGISTER_F(BitmaskDynamicFixture, EmplaceReturn)
    ->RangeMultiplier(2)
    ->Range(64, 4096)
    ->Complexity(benchmark::oAuto);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== BitmaskTable Dynamic Benchmark ===\n";
    std::cout << "Capacity varies per run; includes worst-case near-full and full-table scan benchmarks.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
