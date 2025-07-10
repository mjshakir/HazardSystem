#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <atomic>
#include <random>
#include <algorithm>
#include <numeric>
#include <array>
#include <iostream>

// Include the headers under test
#include "HazardPointerManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

// ============================================================================
// Benchmark Setup and Utility Classes
// ============================================================================

class BenchmarkTestData {
public:
    std::array<int, 16> data;  // 64 bytes of data
    std::atomic<int> counter{0};
    
    BenchmarkTestData(int seed = 0) {
        std::iota(data.begin(), data.end(), seed);
    }
    
    void work() {
        counter.fetch_add(1, std::memory_order_relaxed);
        // Simulate some work
        volatile int sum = 0;
        for (const auto& val : data) {
            sum = sum + val;
        }
        static_cast<void>(sum);
    }
};

// Benchmark fixture for setup/teardown
class FixedHazardPointerBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Register thread for hazard pointer operations
        ThreadRegistry::instance().register_id();
        
        // Clear any existing state
        auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
        manager.clear();
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // Clean up manager
        auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
        manager.clear();
    }
};

// Type aliases for cleaner code
using FixedManagerType = HazardPointerManager<BenchmarkTestData, 64>;
using FixedHandleType = std::pair<std::optional<typename FixedManagerType::IndexType>, std::shared_ptr<HazardPointer<BenchmarkTestData>>>;

// ============================================================================
// Fixed Size Hazard Pointer Manager Benchmarks (HAZARD_POINTERS = 64)
// ============================================================================

// Time Complexity: O(1) - Direct array access via bitmask table
// BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Acquire)(benchmark::State& state) {
//     auto& manager   = HazardPointerManager<BenchmarkTestData, 64>::instance();
//     std::vector<ProtectedPointer<BenchmarkTestData>> guards;
//     guards.reserve(64);

//     // we'll re‐use the same shared_ptr for each protect
//     auto test_data = std::make_shared<BenchmarkTestData>(42);

//     for (auto _ : state) {
//         // protect() instead of acquire()
//         auto p = manager.protect(test_data);
//         benchmark::DoNotOptimize(p);

//         if (p) {
//             guards.push_back(std::move(p));
//         }

//         // once we've held ~60 pointers, reset them all to free slots
//         if (guards.size() >= 60) {
//             for (auto &g : guards) g.reset();
//             guards.clear();
//         }
//     }

//     // final cleanup
//     for (auto &g : guards) g.reset();

//     state.SetComplexityN(state.iterations());
//     state.SetItemsProcessed(state.iterations());
// }

// Time Complexity: O(1) - Direct array access with index
// -----------------------------------------------------------------------------
// Replace “Acquire” with a Protect‐only benchmark
// -----------------------------------------------------------------------------
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Acquire)(benchmark::State& state) {
    auto& manager   = HazardPointerManager<BenchmarkTestData, 64>::instance();
    std::vector<ProtectedPointer<BenchmarkTestData>> guards;
    guards.reserve(64);

    // we'll re‐use the same shared_ptr for each protect
    auto test_data = std::make_shared<BenchmarkTestData>(42);

    for (auto _ : state) {
        // protect() instead of acquire()
        auto p = manager.protect(test_data);
        benchmark::DoNotOptimize(p);

        if (p) {
            guards.push_back(std::move(p));
        }

        // once we've held ~60 pointers, reset them all to free slots
        if (guards.size() >= 60) {
            for (auto &g : guards) g.reset();
            guards.clear();
        }
    }

    // final cleanup
    for (auto &g : guards) g.reset();

    state.SetComplexityN(state.iterations());
    state.SetItemsProcessed(state.iterations());
}


// -----------------------------------------------------------------------------
// Replace “Release” with a Protect‐reset cycle benchmark
// -----------------------------------------------------------------------------
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Release)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    std::vector<ProtectedPointer<BenchmarkTestData>> guards;
    guards.reserve(64);

    // Pre‐protect a batch of 60 pointers
    for (int i = 0; i < 60; ++i) {
        auto p = manager.protect(std::make_shared<BenchmarkTestData>(i));
        if (p) guards.push_back(std::move(p));
    }

    size_t idx = 0;
    for (auto _ : state) {
        // “release” == reset one of our guards
        if (idx < guards.size()) {
            guards[idx].reset();
            ++idx;
        }

        // once they’re all reset, re‐protect another batch
        if (idx >= guards.size()) {
            guards.clear();
            for (int i = 0; i < 60; ++i) {
                auto p = manager.protect(std::make_shared<BenchmarkTestData>(i));
                if (p) guards.push_back(std::move(p));
            }
            idx = 0;
        }
    }

    state.SetComplexityN(state.iterations());
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(1) - Direct pointer assignment with atomic store
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, ProtectSharedPtr)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    auto test_data = std::make_shared<BenchmarkTestData>(42);
    
    for (auto _ : state) {
        auto protected_ptr = manager.protect(test_data);
        benchmark::DoNotOptimize(protected_ptr);
        if (protected_ptr) {
            protected_ptr->work();
        }
    }
    
    state.SetComplexityN(state.iterations());
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(1) - Atomic load + pointer assignment
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, ProtectAtomicPtr)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    std::atomic<std::shared_ptr<BenchmarkTestData>> atomic_data;
    atomic_data.store(std::make_shared<BenchmarkTestData>(42));
    
    for (auto _ : state) {
        auto protected_ptr = manager.protect(atomic_data);
        benchmark::DoNotOptimize(protected_ptr);
        if (protected_ptr) {
            protected_ptr->work();
        }
    }
    
    state.SetComplexityN(state.iterations());
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(k) where k is max_retries (default 100)
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, TryProtect)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    std::atomic<std::shared_ptr<BenchmarkTestData>> atomic_data;
    atomic_data.store(std::make_shared<BenchmarkTestData>(42));
    
    const size_t max_retries = state.range(0);
    
    for (auto _ : state) {
        auto protected_ptr = manager.try_protect(atomic_data, max_retries);
        benchmark::DoNotOptimize(protected_ptr);
        if (protected_ptr) {
            protected_ptr->work();
        }
    }
    
    state.SetComplexityN(max_retries);
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(1) - Direct insertion into hash set
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Retire)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    std::vector<std::shared_ptr<BenchmarkTestData>> test_objects;
    
    // Pre-create objects to retire
    const size_t object_count = 10000;
    test_objects.reserve(object_count);
    for (size_t i = 0; i < object_count; ++i) {
        test_objects.push_back(std::make_shared<BenchmarkTestData>(static_cast<int>(i)));
    }
    
    size_t index = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(manager.retire(test_objects[index % test_objects.size()]));
        index++;
    }
    
    state.SetComplexityN(state.iterations());
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(n*h) where n = retired objects, h = hazard pointers (64)
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Reclaim)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    const size_t retire_count = state.range(0);
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Setup: retire some objects
        std::vector<std::shared_ptr<BenchmarkTestData>> retired_objects;
        retired_objects.reserve(retire_count);
        for (size_t i = 0; i < retire_count; ++i) {
            auto data = std::make_shared<BenchmarkTestData>(static_cast<int>(i));
            retired_objects.push_back(data);
            manager.retire(data);
        }
        
        state.ResumeTiming();
        
        manager.reclaim();
        
        benchmark::DoNotOptimize(manager.retire_size());
    }
    
    state.SetComplexityN(retire_count);
    state.SetItemsProcessed(state.iterations() * retire_count);
}

// Time Complexity: O(1) - Constant time clear operation
// BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Clear)(benchmark::State& state) {
//     auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
//     const size_t setup_count = state.range(0);
    
//     for (auto _ : state) {
//         state.PauseTiming();
        
//         // Setup: acquire some handles and retire some objects
//         std::vector<FixedHandleType> handles;
//         for (size_t i = 0; i < std::min(setup_count, size_t(60)); ++i) {
//             auto handle = manager.acquire();
//             if (handle.first and handle.second) {
//                 handles.push_back(std::move(handle));
//             }
            
//             auto data = std::make_shared<BenchmarkTestData>(static_cast<int>(i));
//             manager.retire(data);
//         }
        
//         state.ResumeTiming();
        
//         manager.clear();
        
//         benchmark::DoNotOptimize(manager.hazard_size());
//     }
    
//     state.SetComplexityN(setup_count);
//     state.SetItemsProcessed(state.iterations());
// }

BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, Clear)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();

    for (auto _ : state) {
        // Pause timing while we set up some live guards and retired objects:
        state.PauseTiming();

        // 1) Create a few protected pointers
        std::vector<ProtectedPointer<BenchmarkTestData>> guards;
        guards.reserve(60);
        for (int i = 0; i < 60; ++i) {
            auto p = manager.protect(std::make_shared<BenchmarkTestData>(i));
            if (p) guards.push_back(std::move(p));
        }

        // 2) Retire a handful of objects (to fill retire‐set)
        for (int i = 0; i < 20; ++i) {
            manager.retire(std::make_shared<BenchmarkTestData>(i));
        }

        state.ResumeTiming();

        // The actual Clear under test
        manager.clear();

        // Record that we did one clear per iteration
        benchmark::DoNotOptimize(manager.hazard_size());
        benchmark::DoNotOptimize(manager.retire_size());
    }

    state.SetComplexityN(state.iterations());
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Stress Tests for Fixed Implementation
// ============================================================================

// Test rapid acquire/release cycles
// BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, RapidAcquireReleaseCycle)(benchmark::State& state) {
//     auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
//     const size_t cycle_count = state.range(0);
    
//     for (auto _ : state) {
//         for (size_t i = 0; i < cycle_count; ++i) {
//             auto handle = manager.acquire();
//             if (handle.first and handle.second) {
//                 benchmark::DoNotOptimize(handle.second);
//                 manager.release(handle);
//             }
//         }
//     }
    
//     state.SetComplexityN(cycle_count);
//     state.SetItemsProcessed(state.iterations() * cycle_count);
// }

BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, RapidProtectResetCycle)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    const size_t cycle_count = state.range(0);

    for (auto _ : state) {
        for (size_t i = 0; i < cycle_count; ++i) {
            // protect a fresh object
            auto p = manager.protect(std::make_shared<BenchmarkTestData>(static_cast<int>(i)));
            benchmark::DoNotOptimize(p);

            // immediately release it by resetting
            if (p) p.reset();
        }
    }

    state.SetComplexityN(cycle_count);
    state.SetItemsProcessed(state.iterations() * cycle_count);
}

// Test protection pattern with varying data sizes
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, ProtectionPattern)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance();
    const size_t data_count = state.range(0);
    
    // Create test data
    std::vector<std::shared_ptr<BenchmarkTestData>> test_data;
    test_data.reserve(data_count);
    for (size_t i = 0; i < data_count; ++i) {
        test_data.push_back(std::make_shared<BenchmarkTestData>(static_cast<int>(i)));
    }
    
    for (auto _ : state) {
        for (const auto& data : test_data) {
            auto protected_ptr = manager.protect(data);
            benchmark::DoNotOptimize(protected_ptr);
            if (protected_ptr) {
                protected_ptr->work();
            }
        }
    }
    
    state.SetComplexityN(data_count);
    state.SetItemsProcessed(state.iterations() * data_count);
}

// Test retire/reclaim pattern
BENCHMARK_DEFINE_F(FixedHazardPointerBenchmark, RetireReclaimPattern)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 64>::instance(state.range(0)); // Custom retire threshold
    const size_t operations = state.range(1);
    
    for (auto _ : state) {
        // Create and retire objects
        for (size_t i = 0; i < operations; ++i) {
            auto data = std::make_shared<BenchmarkTestData>(static_cast<int>(i));
            manager.retire(data);
        }
        
        // Force reclamation
        manager.reclaim();
        benchmark::DoNotOptimize(manager.retire_size());
    }
    
    state.SetComplexityN(operations);
    state.SetItemsProcessed(state.iterations() * operations);
}

// ============================================================================
// Register Fixed Size Benchmarks
// ============================================================================

// Basic operations - O(1) complexity
BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, Acquire)
    ->Range(8, 8192)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, Release)
    ->Range(8, 8192)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, ProtectSharedPtr)
    ->Range(8, 8192)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, ProtectAtomicPtr)
    ->Range(8, 8192)
    ->Complexity(benchmark::o1);

// TryProtect - O(k) where k = max_retries
BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, TryProtect)
    ->RangeMultiplier(2)
    ->Range(1, 256)
    ->Complexity(benchmark::oN);

// Retire - O(1) insertion
BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, Retire)
    ->Range(8, 8192)
    ->Complexity(benchmark::o1);

// Reclaim - O(n*h) where n = retired objects, h = 64 hazard pointers
BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, Reclaim)
    ->RangeMultiplier(2)
    ->Range(1, 1024)
    ->Complexity(benchmark::oN);

// Clear - O(1) for fixed size
BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, Clear)
    ->Range(8, 64)  // Limited by fixed hazard pointer count
    ->Complexity(benchmark::o1);

// Stress tests
BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, RapidProtectResetCycle)
    ->RangeMultiplier(2)
    ->Range(10, 1000)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, ProtectionPattern)
    ->RangeMultiplier(2)
    ->Range(1, 128)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(FixedHazardPointerBenchmark, RetireReclaimPattern)
    ->Args({2, 10})     // retire_threshold=2, operations=10
    ->Args({5, 25})     // retire_threshold=5, operations=25
    ->Args({10, 50})    // retire_threshold=10, operations=50
    ->Args({20, 100})   // retire_threshold=20, operations=100
    ->Complexity(benchmark::oN);

// ============================================================================
// Main function for Fixed benchmarks
// ============================================================================

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    
    std::cout << "=== Fixed HazardPointerManager Benchmark Suite ===\n";
    std::cout << "Testing Fixed Size Implementation (HAZARD_POINTERS=64)\n";
    std::cout << "Expected Time Complexities:\n";
    std::cout << "- Acquire/Release: O(1) - Direct bitmask table access\n";
    std::cout << "- Protect Operations: O(1) - Direct pointer assignment\n";
    std::cout << "- TryProtect: O(k) - Where k = max_retries\n";
    std::cout << "- Retire: O(1) - Hash set insertion\n";
    std::cout << "- Reclaim: O(n*64) - Where n = retired objects\n";
    std::cout << "- Clear: O(1) - Fixed size cleanup\n";
    std::cout << "===================================================\n\n";
    
    // Register the current thread
    ThreadRegistry::instance().register_id();
    
    // Run benchmarks
    ::benchmark::RunSpecifiedBenchmarks();
    
    ::benchmark::Shutdown();
    return 0;
}