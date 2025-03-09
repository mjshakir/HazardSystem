#include <benchmark/benchmark.h>
#include <memory>
#include <thread>
#include <vector>
#include "HashTable.hpp"

constexpr size_t TABLE_SIZE = 1024;
using TestHashTable = HazardSystem::HashTable<int, int, TABLE_SIZE>;

// 📌 **Single-Threaded Insertion Benchmark**
static void BM_SingleThread_Insert(benchmark::State& state) {
    TestHashTable table;
    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            table.insert(i, std::make_shared<int>(i));
        }
        state.PauseTiming();
        table.clear();
        state.ResumeTiming();
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SingleThread_Insert)->RangeMultiplier(10)->Range(1, 10000)->Complexity(benchmark::oAuto);


// 📌 **Multi-Threaded Insertion Benchmark**
static void BM_MultiThread_Insert(benchmark::State& state) {
    TestHashTable table;
    int num_threads = state.range(0);
    std::vector<std::thread> threads;

    for (auto _ : state) {
        state.PauseTiming();
        table.clear();
        state.ResumeTiming();

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < 1000; ++i) {
                    table.insert(i, std::make_shared<int>(i));
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        threads.clear();
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_MultiThread_Insert)->RangeMultiplier(2)->Range(1, 16)->Complexity(benchmark::oAuto);


// 📌 **Single-Threaded Lookup Benchmark**
static void BM_SingleThread_Find(benchmark::State& state) {
    TestHashTable table;
    for (int i = 0; i < state.range(0); ++i) {
        table.insert(i, std::make_shared<int>(i));
    }

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            benchmark::DoNotOptimize(table.find(i));
        }
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SingleThread_Find)->RangeMultiplier(10)->Range(1, 10000)->Complexity(benchmark::oAuto);


// 📌 **Multi-Threaded Lookup Benchmark**
static void BM_MultiThread_Find(benchmark::State& state) {
    TestHashTable table;
    int num_threads = state.range(0);
    for (int i = 0; i < 10000; ++i) {
        table.insert(i, std::make_shared<int>(i));
    }
    std::vector<std::thread> threads;

    for (auto _ : state) {
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int i = 0; i < 10000; ++i) {
                    benchmark::DoNotOptimize(table.find(i));
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        threads.clear();
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_MultiThread_Find)->RangeMultiplier(2)->Range(1, 16)->Complexity(benchmark::oAuto);


// 📌 **Single-Threaded Update Benchmark**
static void BM_SingleThread_Update(benchmark::State& state) {
    TestHashTable table;
    for (int i = 0; i < state.range(0); ++i) {
        table.insert(i, std::make_shared<int>(i));
    }

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            table.update(i, std::make_shared<int>(i * 2));
        }
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SingleThread_Update)->RangeMultiplier(10)->Range(1, 10000)->Complexity(benchmark::oAuto);


// 📌 **Single-Threaded Remove Benchmark**
static void BM_SingleThread_Remove(benchmark::State& state) {
    TestHashTable table;
    for (int i = 0; i < state.range(0); ++i) {
        table.insert(i, std::make_shared<int>(i));
    }

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i) {
            table.remove(i);
        }
        state.PauseTiming();
        for (int i = 0; i < state.range(0); ++i) {
            table.insert(i, std::make_shared<int>(i));
        }
        state.ResumeTiming();
    }
    state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SingleThread_Remove)->RangeMultiplier(10)->Range(1, 10000)->Complexity(benchmark::oAuto);


// 📌 **Benchmark Main Entry Point**
BENCHMARK_MAIN();
