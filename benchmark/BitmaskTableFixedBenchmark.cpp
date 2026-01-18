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
using FixedLargeTable = BitmaskTable<BenchmarkTestData, 1024>;

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

class BitmaskFixedLargeFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override {
        table.clear();
    }

    void TearDown(const ::benchmark::State&) override {
        table.clear();
    }

    FixedLargeTable table;
};

// Acquire + set + release cycle to probe bitmask allocation cost
BENCHMARK_DEFINE_F(BitmaskFixedFixture, AcquireRelease)(benchmark::State& state) {
    const size_t batch = static_cast<size_t>(state.range(0));
    auto payload = std::make_unique<BenchmarkTestData>(42);
    std::vector<FixedTable::IndexType> held;
    held.reserve(table.capacity());

    for (auto _ : state) {
        auto idx = table.acquire();
        benchmark::DoNotOptimize(idx);
        if (idx) {
            table.set(idx.value(), payload.get());
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

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(batch));
    state.SetItemsProcessed(state.iterations());
}

// Iterate over active hazards and touch payloads to simulate reads
BENCHMARK_DEFINE_F(BitmaskFixedFixture, IterateActive)(benchmark::State& state) {
    const size_t to_fill = static_cast<size_t>(std::min<int64_t>(state.range(0), table.capacity()));
    std::vector<std::unique_ptr<BenchmarkTestData>> owned;
    owned.reserve(table.capacity());

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        owned.clear();
        for (size_t i = 0; i < to_fill; ++i) {
            auto idx = table.acquire();
            if (!idx) break;
            owned.emplace_back(std::make_unique<BenchmarkTestData>(static_cast<int>(i)));
            table.set(idx.value(), owned.back().get());
        }
        state.ResumeTiming();

        size_t visited = 0;
        table.for_each_fast([&](FixedTable::IndexType, BenchmarkTestData* ptr) {
            benchmark::DoNotOptimize(ptr);
            ptr->work();
            ++visited;
        });
        benchmark::DoNotOptimize(visited);
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(to_fill));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(to_fill));
}

// Clear all hazard slots and bitmask state
BENCHMARK_DEFINE_F(BitmaskFixedFixture, Clear)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(7);

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        for (size_t i = 0; i < table.capacity(); ++i) {
            auto idx = table.acquire();
            if (!idx) {
                break;
            }
            table.set(idx.value(), payload.get());
        }
        state.ResumeTiming();

        table.clear();
        benchmark::DoNotOptimize(table.size());
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(table.capacity()));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(table.capacity()));
}

// Exercise acquire_iterator + set(iterator) path
BENCHMARK_DEFINE_F(BitmaskFixedFixture, AcquireIteratorSet)(benchmark::State& state) {
    auto payload = std::make_unique<BenchmarkTestData>(3);

    for (auto _ : state) {
        auto it = table.acquire_iterator();
        benchmark::DoNotOptimize(it);
        if (!it) {
            continue;
        }
        table.set(it.value(), payload.get());
        const auto index = static_cast<FixedTable::IndexType>(it.value() - table.begin());
        table.release(index);
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(table.capacity()));
    state.SetItemsProcessed(state.iterations());
}

// Active/at checks on a prefilled table
BENCHMARK_DEFINE_F(BitmaskFixedFixture, ActiveChecks)(benchmark::State& state) {
    const size_t fill_count = static_cast<size_t>(std::min<int64_t>(state.range(0), table.capacity()));
    std::vector<std::unique_ptr<BenchmarkTestData>> owned;
    owned.reserve(table.capacity());
    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        owned.clear();
        for (size_t i = 0; i < fill_count; ++i) {
            auto idx = table.acquire();
            if (!idx) break;
            owned.emplace_back(std::make_unique<BenchmarkTestData>(static_cast<int>(i)));
            table.set(idx.value(), owned.back().get());
        }
        state.ResumeTiming();

        size_t hits = 0;
        for (size_t i = 0; i < table.capacity(); ++i) {
            const bool active = table.active(static_cast<FixedTable::IndexType>(i));
            if (active) {
                auto ptr = table.at(static_cast<FixedTable::IndexType>(i));
                benchmark::DoNotOptimize(ptr);
                ++hits;
            }
        }
        benchmark::DoNotOptimize(hits);
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(fill_count));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(fill_count));
}

// Find with predicate scanning through active slots
BENCHMARK_DEFINE_F(BitmaskFixedFixture, FindPredicate)(benchmark::State& state) {
    constexpr int target_seed = 777;
    std::vector<std::unique_ptr<BenchmarkTestData>> owned;
    owned.reserve(table.capacity());

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        owned.clear();
        // Fill all slots; place a unique seed in the last slot to force scanning
        const size_t capacity = static_cast<size_t>(table.capacity());
        for (size_t i = 0; i < capacity; ++i) {
            auto idx = table.acquire();
            if (!idx) break;
            const int seed = static_cast<int>(i == capacity - 1 ? target_seed : static_cast<int>(i));
            owned.emplace_back(std::make_unique<BenchmarkTestData>(seed));
            table.set(idx.value(), owned.back().get());
        }
        state.ResumeTiming();

        bool found = table.find([&](const BenchmarkTestData* ptr) {
            return ptr && ptr->data.front() == target_seed;
        });
        benchmark::DoNotOptimize(found);
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(table.capacity()));
    state.SetItemsProcessed(state.iterations());
}

// Emplace with return pair then release to keep table available
BENCHMARK_DEFINE_F(BitmaskFixedFixture, EmplaceReturn)(benchmark::State& state) {
    BenchmarkTestData payload(9);
    for (auto _ : state) {
        auto idx = table.acquire();
        benchmark::DoNotOptimize(idx);
        if (idx) {
            table.set(idx.value(), &payload);
            table.release(idx.value());
        }
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(table.capacity()));
    state.SetItemsProcessed(state.iterations());
}

// Worst-case: full table, acquire should scan and fail.
BENCHMARK_DEFINE_F(BitmaskFixedLargeFixture, AcquireFailWhenFull)(benchmark::State& state) {
    BenchmarkTestData payload(1);
    for (size_t i = 0; i < table.capacity(); ++i) {
        table.set(static_cast<FixedLargeTable::IndexType>(i), &payload);
    }

    for (auto _ : state) {
        constexpr size_t kBatch = 64;
        for (size_t i = 0; i < kBatch; ++i) {
            auto idx = table.acquire();
            benchmark::DoNotOptimize(idx);
        }
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(table.capacity()));
    state.SetItemsProcessed(state.iterations() * 64);
}

// Worst-case: only one free slot in the last mask word; acquire scans all previous words.
BENCHMARK_DEFINE_F(BitmaskFixedLargeFixture, AcquireWorstCaseNearFull)(benchmark::State& state) {
    BenchmarkTestData payload(2);
    const auto cap = table.capacity();
    for (size_t i = 0; i < cap; ++i) {
        table.set(static_cast<FixedLargeTable::IndexType>(i), &payload);
    }

    const auto last = static_cast<FixedLargeTable::IndexType>(cap - 1);
    table.set(last, nullptr);
    table.set(static_cast<FixedLargeTable::IndexType>(0), nullptr);
    table.set(static_cast<FixedLargeTable::IndexType>(0), &payload);

    for (auto _ : state) {
        constexpr size_t kBatch = 64;
        for (size_t i = 0; i < kBatch; ++i) {
            auto idx = table.acquire();
            benchmark::DoNotOptimize(idx);
            if (idx) {
                table.release(idx.value());
                // Reset hint to part 0 without changing the single free-slot location.
                table.set(static_cast<FixedLargeTable::IndexType>(0), nullptr);
                table.set(static_cast<FixedLargeTable::IndexType>(0), &payload);
            }
        }
    }

    state.SetComplexityN(static_cast<benchmark::ComplexityN>(cap));
    state.SetItemsProcessed(state.iterations() * 64);
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

BENCHMARK_REGISTER_F(BitmaskFixedLargeFixture, AcquireFailWhenFull)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(BitmaskFixedLargeFixture, AcquireWorstCaseNearFull)
    ->Complexity(benchmark::oN);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== BitmaskTable Fixed Benchmark ===\n";
    std::cout << "Capacity fixed at 64 hazard slots for baseline, plus 1024-slot worst-case scan benchmarks.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
