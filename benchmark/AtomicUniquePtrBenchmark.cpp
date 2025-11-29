//--------------------------------------------------------------
// Standard C++ library
//--------------------------------------------------------------
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
//--------------------------------------------------------------
// Google Benchmark
//--------------------------------------------------------------
#include <benchmark/benchmark.h>
//--------------------------------------------------------------
// Project headers
//--------------------------------------------------------------
#include "atomic_unique_ptr.hpp"

using HazardSystem::atomic_unique_ptr;

// Measure pure load on a pre-initialized pointer.
static void BM_LoadOnly(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(42));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            benchmark::DoNotOptimize(ptr.load(std::memory_order_relaxed));
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

// Store a fresh pointer, read it, then release.
static void BM_LoadStore(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            ptr.store(new int(1), std::memory_order_relaxed);
            benchmark::DoNotOptimize(ptr.load(std::memory_order_relaxed));
            auto old = ptr.release(std::memory_order_relaxed);
            delete old;
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

// Store-only path (overwrite with a fresh pointer each time).
static void BM_StoreOnly(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            ptr.store(new int(1), std::memory_order_relaxed);
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

static void BM_Reset(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            ptr.reset(new int(1), std::memory_order_relaxed);
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

static void BM_CAS_Success(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            int* expected = ptr.load(std::memory_order_relaxed);
            int* desired = new int(1);
            if (ptr.compare_exchange_strong(expected, desired, std::memory_order_relaxed)) {
                delete expected;
            } else {
                delete desired;
            }
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

static void BM_CAS_Fail(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            int* wrong_expected = new int(999);
            int* desired = new int(1);
            int* expected = wrong_expected;
            if (!ptr.compare_exchange_weak(expected, desired, std::memory_order_relaxed)) {
                delete desired;
            } else {
                delete expected;
            }
            delete wrong_expected;
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

static void BM_Transfer(benchmark::State& state) {
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            atomic_unique_ptr<int> ptr(new int(0));
            std::shared_ptr<int> out;
            ptr.transfer(out);
            benchmark::DoNotOptimize(out);
        }
    }
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

static void BM_Protect(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        for (size_t i = 0; i < ops; ++i) {
            auto protected_ptr = ptr.protect();
            benchmark::DoNotOptimize(protected_ptr.get());
        }
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}

static void BM_MultiThreaded_ResetRelease(benchmark::State& state) {
    atomic_unique_ptr<int> ptr(new int(0));
    const size_t ops = static_cast<size_t>(state.range(0));
    for (auto _ : state) {
        std::thread t1([&] {
            for (size_t i = 0; i < ops; ++i) {
                ptr.reset(new int(i), std::memory_order_relaxed);
            }
        });
        std::thread t2([&] {
            for (size_t i = 0; i < ops; ++i) {
                auto old = ptr.release(std::memory_order_relaxed);
                delete old;
            }
        });
        t1.join();
        t2.join();
    }
    auto leftover = ptr.release(std::memory_order_relaxed);
    delete leftover;
    state.SetComplexityN(ops);
    state.SetItemsProcessed(state.iterations() * ops);
}


BENCHMARK(BM_LoadStore)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_Reset)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_CAS_Success)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_CAS_Fail)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_Transfer)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_Protect)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_MultiThreaded_ResetRelease)->RangeMultiplier(2)->Range(1, 1024)->ThreadRange(1, std::thread::hardware_concurrency())->Complexity(benchmark::o1);
BENCHMARK(BM_LoadOnly)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);
BENCHMARK(BM_StoreOnly)->RangeMultiplier(2)->Range(1, 1024)->Complexity(benchmark::o1);

BENCHMARK_MAIN();
