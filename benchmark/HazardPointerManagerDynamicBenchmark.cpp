#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <atomic>
#include <random>
#include <algorithm>
#include <numeric>
#include <array>
#include <iostream>
#include <string_view>
#include <charconv>
#include <system_error>

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
class DynamicHazardPointerBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override {
        // Register thread for hazard pointer operations
        ThreadRegistry::instance().register_id();
        
        // Clear any existing state
        auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance();
        manager.clear();
    }
    
    void TearDown(const ::benchmark::State&) override {
        // Clean up manager
        auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance();
        manager.clear();
    }
};

// Type aliases for cleaner code
using DynamicManagerType = HazardPointerManager<BenchmarkTestData, 0>;
using DynamicHandleType = std::pair<std::optional<typename DynamicManagerType::IndexType>, std::shared_ptr<HazardPointer<BenchmarkTestData>>>;

namespace {
constexpr size_t kDefaultHazardSize = 8192;
constexpr size_t kDefaultRetiredSize = 2;

bool consume_size_arg(std::string_view arg, std::string_view key, size_t* out) {
    if (!out) {
        return false;
    }
    if (!arg.starts_with("--")) {
        return false;
    }
    arg.remove_prefix(2);
    if (!arg.starts_with(key)) {
        return false;
    }
    arg.remove_prefix(key.size());
    if (arg.empty() || arg.front() != '=') {
        return false;
    }
    arg.remove_prefix(1);
    if (arg.empty()) {
        return false;
    }

    size_t parsed = 0;
    const auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), parsed);
    if (ec != std::errc{} || ptr != arg.data() + arg.size()) {
        return false;
    }

    *out = parsed;
    return true;
}

void consume_custom_args(int* argc, char** argv, size_t* hazard_size, size_t* retired_size) {
    if (!argc || !argv) {
        return;
    }
    int write = 1;
    for (int read = 1; read < *argc; ++read) {
        const std::string_view arg{argv[read]};

        size_t parsed = 0;
        if (consume_size_arg(arg, "hazard_size", &parsed)) {
            if (hazard_size) {
                *hazard_size = parsed;
            }
            continue;
        }
        if (consume_size_arg(arg, "retired_size", &parsed)) {
            if (retired_size) {
                *retired_size = parsed;
            }
            continue;
        }

        argv[write++] = argv[read];
    }
    *argc = write;
}
} // namespace

// ============================================================================
// Dynamic Size Hazard Pointer Manager Benchmarks (HAZARD_POINTERS = 0)
// ============================================================================

// Time Complexity (expected): O(1) for acquire/release in the common case
// (bitmask + availability hints).
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Acquire)(benchmark::State& state) {
//     const size_t hazard_size = state.range(0);
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
//     std::vector<DynamicHandleType> handles;
    
//     for (auto _ : state) {
//         auto handle = manager.acquire();
//         if (handle.first and handle.second) {
//             handles.push_back(std::move(handle));
//         }
        
//         // Clean up periodically to avoid exhaustion (keep some slots free)
//         if (handles.size() >= hazard_size - 10) {
//             for (auto& h : handles) {
//                 manager.release(h);
//             }
//             handles.clear();
//         }
//     }
    
//     // Clean up remaining handles
//     for (auto& h : handles) {
//         manager.release(h);
//     }
    
//     state.SetComplexityN(hazard_size);
//     state.SetItemsProcessed(state.iterations());
// }


BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Acquire)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    BenchmarkTestData data(0);
    
    for (auto _ : state) {
        auto p = manager.protect(&data);
        benchmark::DoNotOptimize(p);
        // immediately reset to release the hazard slot
        if (p) {
            p.reset();
        }
    }

    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(1) - Direct access with index
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Release)(benchmark::State& state) {
//     const size_t hazard_size = state.range(0);
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
//     std::vector<DynamicHandleType> handles;
    
//     // Pre-acquire handles for release testing
//     const size_t handle_count = std::min(hazard_size - 10, size_t(100));
//     for (size_t i = 0; i < handle_count; ++i) {
//         auto handle = manager.acquire();
//         if (handle.first and handle.second) {
//             handles.push_back(std::move(handle));
//         }
//     }
    
//     size_t index = 0;
//     for (auto _ : state) {
//         if (index < handles.size()) {
//             benchmark::DoNotOptimize(manager.release(handles[index]));
//             index++;
//         }
        
//         // Replenish handles when needed
//         if (index >= handles.size()) {
//             handles.clear();
//             for (size_t i = 0; i < handle_count; ++i) {
//                 auto handle = manager.acquire();
//                 if (handle.first and handle.second) {
//                     handles.push_back(std::move(handle));
//                 }
//             }
//             index = 0;
//         }
//     }
    
//     state.SetComplexityN(hazard_size);
//     state.SetItemsProcessed(state.iterations());
// }

BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Release)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    BenchmarkTestData data(0);

    // Pre‐warm: grab hazard_size–1 live protects so we can exercise resets only
    std::vector<ProtectedPointer<BenchmarkTestData>> guards;
    guards.reserve(hazard_size-1);
    for (size_t i = 0; i + 1 < hazard_size; ++i) {
        auto p = manager.protect(&data);
        if (p) guards.push_back(std::move(p));
    }

    size_t idx = 0;
    for (auto _ : state) {
        // reset one guard at a time
        guards[idx].reset();
        benchmark::DoNotOptimize(guards[idx]);
        // immediately re-protect into the same slot
        guards[idx] = manager.protect(&data);
        if (++idx >= guards.size()) idx = 0;
    }

    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}


// Time Complexity (expected): O(1) per protect (acquire + registry add + store).
// Worst-case can degrade due to contention / registry probing.
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ProtectSharedPtr)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    auto test_data = std::make_shared<BenchmarkTestData>(42);
    
    for (auto _ : state) {
        auto protected_ptr = manager.protect(test_data);
        benchmark::DoNotOptimize(protected_ptr);
        if (protected_ptr) {
            protected_ptr->work();
        }
    }
    
    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity (expected): O(1) per protect (acquire + atomic load + registry add + store).
// Worst-case can degrade due to contention / registry probing.
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ProtectAtomicPtr)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    std::atomic<std::shared_ptr<BenchmarkTestData>> atomic_data;
    atomic_data.store(std::make_shared<BenchmarkTestData>(42));
    
    for (auto _ : state) {
        auto protected_ptr = manager.protect(atomic_data);
        benchmark::DoNotOptimize(protected_ptr);
        if (protected_ptr) {
            protected_ptr->work();
        }
    }
    
    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(k) retries in the worst case (each attempt is a protect-like operation).
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, TryProtect)(benchmark::State& state) {
    const size_t hazard_size = 64;  // Fixed for this test
    const auto max_retries_range = state.range(0);
    const size_t max_retries = static_cast<size_t>(max_retries_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    std::atomic<std::shared_ptr<BenchmarkTestData>> atomic_data;
    atomic_data.store(std::make_shared<BenchmarkTestData>(42));
    
    for (auto _ : state) {
        auto protected_ptr = manager.try_protect(atomic_data, max_retries);
        benchmark::DoNotOptimize(protected_ptr);
        if (protected_ptr) {
            protected_ptr->work();
        }
    }
    
    state.SetComplexityN(max_retries_range);
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity: O(1) - Direct insertion into hash set
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Retire)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
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
    
    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}

// Time Complexity (expected): O(r) where r = retired objects (hazard checks via registry).
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Reclaim)(benchmark::State& state) {
    const size_t hazard_size = 64;  // Fixed for this test
    const auto retire_count_range = state.range(0);
    const size_t retire_count = static_cast<size_t>(retire_count_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
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
    
    state.SetComplexityN(retire_count_range);
    state.SetItemsProcessed(state.iterations() * retire_count_range);
}

// Time Complexity: O(hazards + registry + retired) for cleanup.
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Clear)(benchmark::State& state) {
//     const size_t hazard_size = state.range(0);
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
//     for (auto _ : state) {
//         state.PauseTiming();
        
//         // Setup: acquire some handles and retire some objects
//         std::vector<DynamicHandleType> handles;
//         const size_t setup_count = std::min(hazard_size - 10, size_t(100));
//         for (size_t i = 0; i < setup_count; ++i) {
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
    
//     state.SetComplexityN(hazard_size);
//     state.SetItemsProcessed(state.iterations());
// }

BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, Clear)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);

    for (auto _ : state) {
        state.PauseTiming();

        // Setup: hold a bunch of protections and retire some objects
        std::vector<ProtectedPointer<BenchmarkTestData>> guards;
        const size_t setup_count = std::min(hazard_size - 10, size_t(100));
        guards.reserve(setup_count);
        for (size_t i = 0; i < setup_count; ++i) {
            // grab and hold
            auto p = manager.protect(std::make_shared<BenchmarkTestData>(static_cast<int>(i)));
            if (p) guards.push_back(std::move(p));
            // also retire some objects to fill retire‐set
            manager.retire(std::make_shared<BenchmarkTestData>(static_cast<int>(i)));
        }

        state.ResumeTiming();

        // this is the operation under test
        manager.clear();
        benchmark::DoNotOptimize(manager.hazard_size());
    }

    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Scalability Tests for Dynamic Implementation
// ============================================================================

// Test how acquisition performance scales with hazard pointer count
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ScalabilityAcquisition)(benchmark::State& state) {
//     const size_t hazard_size = state.range(0);
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
//     for (auto _ : state) {
//         // Acquire all available hazard pointers
//         std::vector<DynamicHandleType> handles;
//         handles.reserve(hazard_size);
        
//         for (size_t i = 0; i < hazard_size - 1; ++i) {  // Leave one slot free
//             auto handle = manager.acquire();
//             if (handle.first and handle.second) {
//                 handles.push_back(std::move(handle));
//             } else {
//                 break;  // No more slots available
//             }
//         }
        
//         benchmark::DoNotOptimize(handles.size());
        
//         // Release all handles
//         for (auto& h : handles) {
//             manager.release(h);
//         }
//     }
    
//     state.SetComplexityN(hazard_size);
//     state.SetItemsProcessed(state.iterations() * hazard_size);
// }

BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ScalabilityAcquisition)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    BenchmarkTestData data(0);

    for (auto _ : state) {
        // fill all but one slot
        std::vector<ProtectedPointer<BenchmarkTestData>> guards;
        guards.reserve(hazard_size);
        for (size_t i = 0; i + 1 < hazard_size; ++i) {
            auto p = manager.protect(&data);
            if (!p) break;
            guards.push_back(std::move(p));
        }

        // record how many we got
        benchmark::DoNotOptimize(guards.size());
        // when `guards` goes out of scope, each ProtectedPointer resets itself
    }

    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}


// Test protection performance with different hazard pool sizes
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ScalabilityProtection)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const auto data_count_range = state.range(1);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    const size_t data_count = static_cast<size_t>(data_count_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
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
    
    state.SetComplexityN(hazard_size_range * data_count_range);
    state.SetItemsProcessed(state.iterations() * data_count_range);
}

// Test reclamation performance with varying hazard pool sizes
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ScalabilityReclamation)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    const size_t retire_count = 100;  // Fixed retire count
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size, retire_count);
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Setup: retire objects
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
    
    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(retire_count));
}

// ============================================================================
// Stress Tests for Dynamic Implementation
// ============================================================================

// Test rapid acquire/release cycles with different pool sizes
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, RapidAcquireReleaseCycle)(benchmark::State& state) {
//     const size_t hazard_size = state.range(0);
//     const size_t cycle_count = state.range(1);
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
//     for (auto _ : state) {
//         for (size_t i = 0; i < cycle_count; ++i) {
//             auto handle = manager.acquire();
//             if (handle.first and handle.second) {
//                 benchmark::DoNotOptimize(handle.second);
//                 manager.release(handle);
//             }
//         }
//     }
    
//     state.SetComplexityN(hazard_size * cycle_count);
//     state.SetItemsProcessed(state.iterations() * cycle_count);
// }

BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, RapidProtectResetCycle)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const auto cycle_count_range = state.range(1);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    const size_t cycle_count = static_cast<size_t>(cycle_count_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    BenchmarkTestData data(0);

    for (auto _ : state) {
        for (size_t i = 0; i < cycle_count; ++i) {
            auto p = manager.protect(&data);
            benchmark::DoNotOptimize(p);
            if (p) p.reset();
        }
    }

    state.SetComplexityN(hazard_size_range * cycle_count_range);
    state.SetItemsProcessed(state.iterations() * cycle_count_range);
}

// Test protection pattern with varying pool and data sizes
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, ProtectionPattern)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const auto data_count_range = state.range(1);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    const size_t data_count = static_cast<size_t>(data_count_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
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
    
    state.SetComplexityN(hazard_size_range * data_count_range);
    state.SetItemsProcessed(state.iterations() * data_count_range);
}

// Test retire/reclaim pattern with different configurations
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, RetireReclaimPattern)(benchmark::State& state) {
    const size_t hazard_size = 64;  // Fixed hazard size
    const auto retire_threshold_range = state.range(0);
    const auto operations_range = state.range(1);
    const size_t retire_threshold = static_cast<size_t>(retire_threshold_range);
    const size_t operations = static_cast<size_t>(operations_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size, retire_threshold);
    
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
    
    state.SetComplexityN(operations_range);
    state.SetItemsProcessed(state.iterations() * operations_range);
}

// Test worst-case scenario: full hazard pool
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, WorstCaseFullPool)(benchmark::State& state) {
//     const size_t hazard_size = state.range(0);
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
//     // Fill up most of the hazard pool
//     std::vector<DynamicHandleType> permanent_handles;
//     for (size_t i = 0; i < hazard_size - 2; ++i) {  // Leave 2 slots
//         auto handle = manager.acquire();
//         if (handle.first and handle.second) {
//             permanent_handles.push_back(std::move(handle));
//         }
//     }
    
//     auto test_data = std::make_shared<BenchmarkTestData>(42);
    
//     for (auto _ : state) {
//         // Try to acquire from nearly full pool
//         auto protected_ptr = manager.protect(test_data);
//         benchmark::DoNotOptimize(protected_ptr);
//         if (protected_ptr) {
//             protected_ptr->work();
//         }
//     }
    
//     // Clean up
//     for (auto& h : permanent_handles) {
//         manager.release(h);
//     }
    
//     state.SetComplexityN(hazard_size);
//     state.SetItemsProcessed(state.iterations());
// }

BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, WorstCaseFullPool)(benchmark::State& state) {
    const auto hazard_size_range = state.range(0);
    const size_t hazard_size = static_cast<size_t>(hazard_size_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);

    // permanently hold hazard_size-2 slots
    auto test_data = std::make_shared<BenchmarkTestData>(42);
    std::vector<ProtectedPointer<BenchmarkTestData>> permanent;
    permanent.reserve(hazard_size - 2);
    for (size_t i = 0; i + 2 < hazard_size; ++i) {
        auto p = manager.protect(test_data);
        if (!p) break;
        permanent.push_back(std::move(p));
    }

    for (auto _ : state) {
        // now this protect will usually fail (pool is nearly exhausted)
        auto p = manager.protect(test_data);
        benchmark::DoNotOptimize(p);
    }

    // drop out of scope: permanent guards reset themselves
    state.SetComplexityN(hazard_size_range);
    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Comparison Tests (Dynamic vs Expected Performance)
// ============================================================================

// Compare acquisition time vs pool utilization
// BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, AcquisitionVsUtilization)(benchmark::State& state) {
//     const size_t hazard_size = 128;  // Fixed pool size
//     const size_t utilization_percent = state.range(0);  // 0-90%
//     auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size);
    
//     // Pre-fill pool to desired utilization
//     const size_t pre_acquired = (hazard_size * utilization_percent) / 100;
//     std::vector<DynamicHandleType> background_handles;
//     for (size_t i = 0; i < pre_acquired; ++i) {
//         auto handle = manager.acquire();
//         if (handle.first and handle.second) {
//             background_handles.push_back(std::move(handle));
//         }
//     }
    
//     for (auto _ : state) {
//         auto handle = manager.acquire();
//         benchmark::DoNotOptimize(handle);
//         if (handle.first and handle.second) {
//             manager.release(handle);
//         }
//     }
    
//     // Clean up
//     for (auto& h : background_handles) {
//         manager.release(h);
//     }
    
//     state.SetComplexityN(utilization_percent);
//     state.SetItemsProcessed(state.iterations());
// }

BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, AcquisitionVsUtilization)(benchmark::State& state) {
    const auto util_pct_range = state.range(0);
    const size_t util_pct = static_cast<size_t>(util_pct_range);
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(kDefaultHazardSize);
    const size_t hazard_size = manager.hazard_capacity();
    BenchmarkTestData data(0);

    // Pre‐fill
    size_t to_acquire = (hazard_size * util_pct) / 100;
    std::vector<ProtectedPointer<BenchmarkTestData>> background;
    background.reserve(to_acquire);
    for (size_t i = 0; i < to_acquire; ++i) {
        auto p = manager.protect(&data);
        if (!p) break;
        background.push_back(std::move(p));
    }

    for (auto _ : state) {
        // single protect+reset under that utilization
        auto p = manager.protect(&data);
        benchmark::DoNotOptimize(p);
        if (p) p.reset();
    }

    state.SetComplexityN(util_pct_range);
    state.SetItemsProcessed(state.iterations());
}

// Worst-case near-full acquisition: keep the pool at (capacity-1) utilization and force the
// search hint to start at the beginning, so the only free slot is far away.
BENCHMARK_DEFINE_F(DynamicHazardPointerBenchmark, AcquireWorstCaseNearFull)(benchmark::State& state) {
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(kDefaultHazardSize);
    BenchmarkTestData data(0);

    const size_t capacity = manager.hazard_capacity();
    if (capacity < 2) {
        state.SkipWithError("Need hazard_capacity >= 2");
        return;
    }

    // Fill capacity-1 slots once (leaving the last slot free).
    std::vector<ProtectedPointer<BenchmarkTestData>> guards;
    guards.reserve(capacity - 1);
    for (size_t i = 0; i + 1 < capacity; ++i) {
        auto p = manager.protect(&data);
        if (!p) {
            state.SkipWithError("Unable to prefill hazard pool");
            return;
        }
        guards.push_back(std::move(p));
    }

    for (auto _ : state) {
        state.PauseTiming();
        // Force the internal hint back toward the start by releasing + re-acquiring the first slot.
        guards.front().reset();
        guards.front() = manager.protect(&data);
        state.ResumeTiming();

        auto p = manager.protect(&data);
        benchmark::DoNotOptimize(p);
        if (p) {
            p.reset();
        }
    }

    state.SetItemsProcessed(state.iterations());
}

// ============================================================================
// Register Dynamic Size Benchmarks
// ============================================================================

// Basic operations with scaling complexity
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, Acquire)
    ->RangeMultiplier(2)
    ->Range(8, 512)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, Release)
    ->RangeMultiplier(2)
    ->Range(8, 512)
    ->Complexity(benchmark::o1);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, ProtectSharedPtr)
    ->RangeMultiplier(2)
    ->Range(8, 512)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, ProtectAtomicPtr)
    ->RangeMultiplier(2)
    ->Range(8, 512)
    ->Complexity(benchmark::oN);

// TryProtect - O(k) where k = max_retries (each attempt is a protect-like operation)
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, TryProtect)
    ->RangeMultiplier(2)
    ->Range(1, 256)
    ->Complexity(benchmark::oN);

// Retire - O(1) insertion (independent of hazard_size)
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, Retire)
    ->RangeMultiplier(2)
    ->Range(8, 512)
    ->Complexity(benchmark::o1);

// Reclaim - expected O(r) where r = retired objects (hazard checks via registry)
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, Reclaim)
    ->RangeMultiplier(2)
    ->Range(1, 1024)
    ->Complexity(benchmark::oN);

// Clear - O(n) where n = hazard_size
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, Clear)
    ->RangeMultiplier(2)
    ->Range(8, 256)
    ->Complexity(benchmark::oN);

// Scalability tests
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, ScalabilityAcquisition)
    ->RangeMultiplier(2)
    ->Range(16, 256)
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, ScalabilityProtection)
    ->Args({32, 10})     // hazard_size=32, data_count=10
    ->Args({64, 10})     // hazard_size=64, data_count=10
    ->Args({128, 10})    // hazard_size=128, data_count=10
    ->Args({32, 50})     // hazard_size=32, data_count=50
    ->Args({64, 50})     // hazard_size=64, data_count=50
    ->Args({128, 50})    // hazard_size=128, data_count=50
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, ScalabilityReclamation)
    ->RangeMultiplier(2)
    ->Range(16, 256)
    ->Complexity(benchmark::oN);

// Stress tests
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, RapidProtectResetCycle)
    ->Args({32, 100})    // hazard_size=32, cycle_count=100
    ->Args({64, 100})    // hazard_size=64, cycle_count=100
    ->Args({128, 100})   // hazard_size=128, cycle_count=100
    ->Args({64, 50})     // hazard_size=64, cycle_count=50
    ->Args({64, 200})    // hazard_size=64, cycle_count=200
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, ProtectionPattern)
    ->Args({32, 20})     // hazard_size=32, data_count=20
    ->Args({64, 20})     // hazard_size=64, data_count=20
    ->Args({128, 20})    // hazard_size=128, data_count=20
    ->Args({64, 10})     // hazard_size=64, data_count=10
    ->Args({64, 40})     // hazard_size=64, data_count=40
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, RetireReclaimPattern)
    ->Args({10, 50})     // retire_threshold=10, operations=50
    ->Args({20, 100})    // retire_threshold=20, operations=100
    ->Args({50, 250})    // retire_threshold=50, operations=250
    ->Args({100, 500})   // retire_threshold=100, operations=500
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, WorstCaseFullPool)
    ->RangeMultiplier(2)
    ->Range(32, 256)
    ->Complexity(benchmark::oN);

// Utilization tests
BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, AcquisitionVsUtilization)
    ->DenseRange(10, 90, 10)  // 0%, 10%, 20%, ..., 90% utilization
    ->Complexity(benchmark::oN);

BENCHMARK_REGISTER_F(DynamicHazardPointerBenchmark, AcquireWorstCaseNearFull)
    ->Complexity(benchmark::o1);

// ============================================================================
// Main function for Dynamic benchmarks
// ============================================================================

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);

    size_t hazard_size = kDefaultHazardSize;
    size_t retired_size = kDefaultRetiredSize;
    consume_custom_args(&argc, argv, &hazard_size, &retired_size);
    
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    // Initialize the singleton once with a large pool (tree-mode bitset summary).
    auto& manager = HazardPointerManager<BenchmarkTestData, 0>::instance(hazard_size, retired_size);
    
    std::cout << "=== Dynamic HazardPointerManager Benchmark Suite ===\n";
    std::cout << "Testing Dynamic Size Implementation (HAZARD_POINTERS=0)\n";
    std::cout << "Configured hazards_size=" << hazard_size
              << " (capacity=" << manager.hazard_capacity() << "), retired_size=" << retired_size << "\n";
    std::cout << "Expected Time Complexities:\n";
    std::cout << "- Acquire/Release: expected O(1) - Bitmask + availability hints\n";
    std::cout << "- Protect: expected O(1) - Acquire + registry add + store\n";
    std::cout << "- TryProtect: O(k) - Where k = max_retries\n";
    std::cout << "- Retire: expected O(1) - Hash insertion (amortized)\n";
    std::cout << "- Reclaim: expected O(r) - Where r = retired objects (hazard checks via registry)\n";
    std::cout << "- Clear: O(hazards + registry + retired)\n";
    std::cout << "======================================================\n\n";
    
    // Register the current thread
    ThreadRegistry::instance().register_id();
    
    // Run benchmarks
    ::benchmark::RunSpecifiedBenchmarks();
    
    ::benchmark::Shutdown();
    return 0;
}
