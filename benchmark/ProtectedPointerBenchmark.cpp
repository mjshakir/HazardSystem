#include <benchmark/benchmark.h>
#include <atomic>
#include <iostream>
#include <memory>

#include "ProtectedPointer.hpp"

using namespace HazardSystem;

struct BenchmarkTestData {
    explicit BenchmarkTestData(int v = 0) : value(v) {}
    int value;
    void touch() noexcept { value++; }
};

// Guard lifecycle: construct, reset, and invoke release functor
static void BM_ProtectedPointerLifecycle(benchmark::State& state) {
    const size_t ops = static_cast<size_t>(state.range(0));
    std::atomic<size_t> releases{0};

    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            ProtectedPointer<BenchmarkTestData> guard(
                std::make_shared<BenchmarkTestData>(static_cast<int>(i)),
                [&]() noexcept {
                    releases.fetch_add(1, std::memory_order_relaxed);
                    return true;
                });
            benchmark::DoNotOptimize(guard);
            guard.reset();
        }
    }

    state.counters["releases"] = static_cast<double>(releases.load(std::memory_order_relaxed));
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

// Move construction to cover move semantics and ownership transfer
static void BM_ProtectedPointerMove(benchmark::State& state) {
    const size_t ops = static_cast<size_t>(state.range(0));
    std::atomic<size_t> releases{0};

    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            ProtectedPointer<BenchmarkTestData> src(
                std::make_shared<BenchmarkTestData>(static_cast<int>(i)),
                [&]() noexcept {
                    releases.fetch_add(1, std::memory_order_relaxed);
                    return true;
                });
            ProtectedPointer<BenchmarkTestData> dst(std::move(src));
            benchmark::DoNotOptimize(dst);
        }
    }

    state.counters["releases"] = static_cast<double>(releases.load(std::memory_order_relaxed));
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

// Access overhead for operator->, operator*, get(), and bool conversion
static void BM_ProtectedPointerAccess(benchmark::State& state) {
    const size_t ops = static_cast<size_t>(state.range(0));
    std::atomic<size_t> touched{0};

    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            ProtectedPointer<BenchmarkTestData> guard(
                std::make_shared<BenchmarkTestData>(static_cast<int>(i)),
                [&]() noexcept { return true; });

            if (guard) {
                guard->touch();
                touched.fetch_add(static_cast<size_t>((*guard).value), std::memory_order_relaxed);
                benchmark::DoNotOptimize(guard.get());
                benchmark::DoNotOptimize(guard.shared_ptr());
            }
        }
    }

    state.counters["touched"] = static_cast<double>(touched.load(std::memory_order_relaxed));
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

BENCHMARK(BM_ProtectedPointerLifecycle)
    ->RangeMultiplier(2)
    ->Range(16, 1024)
    ->Complexity(benchmark::o1);

BENCHMARK(BM_ProtectedPointerMove)
    ->RangeMultiplier(2)
    ->Range(16, 1024)
    ->Complexity(benchmark::o1);

BENCHMARK(BM_ProtectedPointerAccess)
    ->RangeMultiplier(2)
    ->Range(16, 1024)
    ->Complexity(benchmark::o1);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== ProtectedPointer Benchmark ===\n";
    std::cout << "Lifecycle, move semantics, and access operators under load.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
