#include <benchmark/benchmark.h>
#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>

#include "HazardThreadManager.hpp"

using namespace HazardSystem;

static void BM_HazardThreadManagerAccess(benchmark::State& state) {
    for (auto _ : state) {
        auto& manager = HazardThreadManager::instance();
        benchmark::DoNotOptimize(&manager);
    }

    state.SetItemsProcessed(state.iterations() * state.threads());
}

static void BM_HazardThreadManagerThreadLifecycle(benchmark::State& state) {
    const size_t threads_per_iter = static_cast<size_t>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::thread> workers;
        workers.reserve(threads_per_iter);
        state.ResumeTiming();

        for (size_t i = 0; i < threads_per_iter; ++i) {
            workers.emplace_back([] {
                auto& manager = HazardThreadManager::instance();
                benchmark::DoNotOptimize(&manager);
            });
        }

        for (auto& worker : workers) {
            worker.join();
        }
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(threads_per_iter));
}

BENCHMARK(BM_HazardThreadManagerAccess)
    ->ThreadRange(1, static_cast<int>(std::max(2u, std::thread::hardware_concurrency())));
BENCHMARK(BM_HazardThreadManagerThreadLifecycle)
    ->RangeMultiplier(2)
    ->Range(1, 64);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== HazardThreadManager Benchmark ===\n";
    std::cout << "Access cost (thread-local) and lifecycle churn across many threads.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
