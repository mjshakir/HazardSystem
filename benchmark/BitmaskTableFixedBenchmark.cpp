#include <benchmark/benchmark.h>
#include <array>
#include <atomic>
#include <iostream>
#include <memory>
#include <vector>

#include "BitmaskTable.hpp"

using namespace HazardSystem;

// Shared payload type exercised by all benchmarks
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

using FixedTable = BitmaskTable<BenchmarkTestData, 64>;

class BitmaskFixedFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override {
        table.clear();
    }

    void TearDown(const ::benchmark::State&) override {
        table.clear();
    }

    FixedTable table;
};

// Acquire + set + release cycle to probe bitmask allocation cost
BENCHMARK_DEFINE_F(BitmaskFixedFixture, AcquireRelease)(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));
    auto payload = std::make_shared<BenchmarkTestData>(42);
    std::vector<FixedTable::IndexType> held;
    held.reserve(table.capacity());

    for (auto _ : state) {
        auto idx = table.acquire();
        benchmark::DoNotOptimize(idx);
        if (idx) {
            table.set(idx.value(), payload);
            held.push_back(idx.value());
        }

        if (held.size() >= std::min(batch, static_cast<size_t>(table.capacity()))) {
            for (auto id : held) {
                table.release(id);
            }
            held.clear();
        }
    }

    for (auto id : held) {
        table.release(id);
    }

    state.SetComplexityN(batch);
    state.SetItemsProcessed(state.iterations());
}

// Iterate over active hazards and touch payloads to simulate reads
BENCHMARK_DEFINE_F(BitmaskFixedFixture, IterateActive)(benchmark::State& state) {
    const size_t to_fill = static_cast<size_t>(std::min<int64_t>(state.range(0), table.capacity()));

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        for (size_t i = 0; i < to_fill; ++i) {
            auto idx = table.emplace(static_cast<int>(i));
            if (!idx) {
                break;
            }
        }
        state.ResumeTiming();

        size_t visited = 0;
        table.for_each_fast([&](FixedTable::IndexType, std::shared_ptr<BenchmarkTestData>& ptr) {
            benchmark::DoNotOptimize(ptr);
            ptr->work();
            ++visited;
        });
        benchmark::DoNotOptimize(visited);
    }

    state.SetComplexityN(to_fill);
    state.SetItemsProcessed(state.iterations() * to_fill);
}

// Clear all hazard slots and bitmask state
BENCHMARK_DEFINE_F(BitmaskFixedFixture, Clear)(benchmark::State& state) {
    auto payload = std::make_shared<BenchmarkTestData>(7);

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        for (size_t i = 0; i < table.capacity(); ++i) {
            auto idx = table.acquire();
            if (!idx) {
                break;
            }
            table.set(idx.value(), payload);
        }
        state.ResumeTiming();

        table.clear();
        benchmark::DoNotOptimize(table.size());
    }

    state.SetComplexityN(table.capacity());
    state.SetItemsProcessed(state.iterations() * table.capacity());
}

// Exercise acquire_iterator + set(iterator) path
BENCHMARK_DEFINE_F(BitmaskFixedFixture, AcquireIteratorSet)(benchmark::State& state) {
    auto payload = std::make_shared<BenchmarkTestData>(3);

    for (auto _ : state) {
        auto it = table.acquire_iterator();
        benchmark::DoNotOptimize(it);
        if (!it) {
            continue;
        }
        table.set(it.value(), payload);
        const auto index = static_cast<FixedTable::IndexType>(it.value() - table.begin());
        table.release(index);
    }

    state.SetComplexityN(table.capacity());
    state.SetItemsProcessed(state.iterations());
}

// Active/at checks on a prefilled table
BENCHMARK_DEFINE_F(BitmaskFixedFixture, ActiveChecks)(benchmark::State& state) {
    const size_t fill_count = static_cast<size_t>(std::min<int64_t>(state.range(0), table.capacity()));
    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        for (size_t i = 0; i < fill_count; ++i) {
            auto idx = table.emplace(static_cast<int>(i));
            if (!idx) {
                break;
            }
        }
        state.ResumeTiming();

        size_t hits = 0;
        for (size_t i = 0; i < table.capacity(); ++i) {
            const bool active = table.active(static_cast<FixedTable::IndexType>(i));
            if (active) {
                auto sp = table.at(static_cast<FixedTable::IndexType>(i));
                benchmark::DoNotOptimize(sp);
                ++hits;
            }
        }
        benchmark::DoNotOptimize(hits);
    }

    state.SetComplexityN(fill_count);
    state.SetItemsProcessed(state.iterations() * fill_count);
}

// Find with predicate scanning through active slots
BENCHMARK_DEFINE_F(BitmaskFixedFixture, FindPredicate)(benchmark::State& state) {
    constexpr int target_seed = 777;

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        // Fill all slots; place a unique seed in the last slot to force scanning
        for (size_t i = 0; i < table.capacity(); ++i) {
            auto idx = table.emplace(static_cast<int>(i == table.capacity() - 1 ? target_seed : static_cast<int>(i)));
            if (!idx) {
                break;
            }
        }
        state.ResumeTiming();

        const bool found = table.find([&](const std::shared_ptr<BenchmarkTestData>& ptr) {
            return ptr && ptr->data.front() == target_seed;
        });
        benchmark::DoNotOptimize(found);
    }

    state.SetComplexityN(table.capacity());
    state.SetItemsProcessed(state.iterations());
}

// Emplace with return pair then release to keep table available
BENCHMARK_DEFINE_F(BitmaskFixedFixture, EmplaceReturn)(benchmark::State& state) {
    for (auto _ : state) {
        auto res = table.emplace_return(9);
        benchmark::DoNotOptimize(res);
        if (res) {
            table.release(res->first);
        }
    }

    state.SetComplexityN(table.capacity());
    state.SetItemsProcessed(state.iterations());
}

// Benchmark registration
BENCHMARK_REGISTER_F(BitmaskFixedFixture, AcquireRelease)
    ->Range(8, 64)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(BitmaskFixedFixture, IterateActive)
    ->Range(8, 64)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(BitmaskFixedFixture, Clear)
    ->Range(8, 64)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(BitmaskFixedFixture, AcquireIteratorSet)
    ->Range(8, 64)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(BitmaskFixedFixture, ActiveChecks)
    ->Range(8, 64)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(BitmaskFixedFixture, FindPredicate)
    ->Range(8, 64)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(BitmaskFixedFixture, EmplaceReturn)
    ->Range(8, 64)
    ->Complexity(benchmark::o1);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== BitmaskTable Fixed Benchmark ===\n";
    std::cout << "Capacity fixed at 64 hazard slots, expect O(1) allocation/clear.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
