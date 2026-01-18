#include <benchmark/benchmark.h>
#include <algorithm>
#include <iostream>
#include <thread>

#include "ThreadRegistry.hpp"

using namespace HazardSystem;

static void BM_RegisterUnregister(benchmark::State& state) {
    auto& registry = ThreadRegistry::instance();
    registry.unregister();

    for (auto _ : state) {
        bool registered = registry.register_id();
        benchmark::DoNotOptimize(registered);
        benchmark::DoNotOptimize(registry.registered());
        bool unregistered = registry.unregister();
        benchmark::DoNotOptimize(unregistered);
    }

    registry.unregister();
    state.SetItemsProcessed(state.iterations() * state.threads());
}

static void BM_RegisterContention(benchmark::State& state) {
    auto& registry = ThreadRegistry::instance();
    registry.unregister();

    for (auto _ : state) {
        bool registered = registry.register_id();
        benchmark::DoNotOptimize(registered);
        bool unregistered = registry.unregister();
        benchmark::DoNotOptimize(unregistered);
    }

    state.SetItemsProcessed(state.iterations() * state.threads());
}

static void BM_RegisteredCheck(benchmark::State& state) {
    auto& registry = ThreadRegistry::instance();
    registry.unregister();
    registry.register_id();

    for (auto _ : state) {
        benchmark::DoNotOptimize(registry.registered());
    }

    registry.unregister();
    state.SetItemsProcessed(state.iterations() * state.threads());
}

BENCHMARK(BM_RegisterUnregister);
BENCHMARK(BM_RegisterContention)
    ->ThreadRange(1, static_cast<int>(std::max(2u, std::thread::hardware_concurrency())));
BENCHMARK(BM_RegisteredCheck);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== ThreadRegistry Benchmark ===\n";
    std::cout << "Measures registration/unregistration latency and contention.\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
