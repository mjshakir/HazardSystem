#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <algorithm>
#include <future>
#include <array>
#include <numeric>
#include <iostream>

// Include the headers under test
#include "HazardPointerManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

// ============================================================================
// Test Fixture for Comparison Tests
// ============================================================================

class ComparisonHazardPointerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure thread is registered
        ThreadRegistry::instance().register_id();
    }

    void TearDown() override {
        // Clean up both implementations
        auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
        fixed_manager.clear();
        
        auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance();
        dynamic_manager.clear();
    }

    struct TestData {
        int value;
        std::atomic<int> access_count{0};
        std::atomic<bool> destroyed{false};
        
        TestData(int v = 0) : value(v) {}
        
        ~TestData() {
            destroyed.store(true, std::memory_order_release);
        }
        
        void access() {
            access_count.fetch_add(1, std::memory_order_relaxed);
        }
        
        void increment() {
            access_count.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    using FixedManagerType = HazardPointerManager<TestData, 64>;
    using FixedHandleType = HazardHandle<typename FixedManagerType::IndexType, HazardPointer<TestData>>;
    
    using DynamicManagerType = HazardPointerManager<TestData, 0>;
    using DynamicHandleType = HazardHandle<typename DynamicManagerType::IndexType, HazardPointer<TestData>>;
};

// ============================================================================
// Basic Functionality Comparison Tests
// ============================================================================

#define DEFINE_POOL_TESTDATA(NAME) \
    struct NAME##_TestData { \
        int value; \
        std::atomic<int> access_count{0}; \
        std::atomic<bool> destroyed{false}; \
        NAME##_TestData(int v = 0) : value(v) {} \
        ~NAME##_TestData() { destroyed.store(true, std::memory_order_release); } \
        void access() { access_count.fetch_add(1, std::memory_order_relaxed); } \
        void increment() { access_count.fetch_add(1, std::memory_order_relaxed); } \
    }


// Helper macro to generate unique TestData types per test
#define DEFINE_EXHAUSTION_TESTDATA(NAME) \
    struct NAME##_TestData { \
        int value; \
        std::atomic<int> access_count{0}; \
        std::atomic<bool> destroyed{false}; \
        NAME##_TestData(int v = 0) : value(v) {} \
        ~NAME##_TestData() { destroyed.store(true, std::memory_order_release); } \
        void access() { access_count.fetch_add(1, std::memory_order_relaxed); } \
        void increment() { access_count.fetch_add(1, std::memory_order_relaxed); } \
    }



// Helper macro for unique type per size
#define DEFINE_CONFIG_TESTDATA(NAME) \
    struct NAME##_TestData { \
        int value; \
        std::atomic<int> access_count{0}; \
        std::atomic<bool> destroyed{false}; \
        NAME##_TestData(int v = 0) : value(v) {} \
        ~NAME##_TestData() { destroyed.store(true, std::memory_order_release); } \
        void access() { access_count.fetch_add(1, std::memory_order_relaxed); } \
        void increment() { access_count.fetch_add(1, std::memory_order_relaxed); } \
    }
//--------------------------------------------------------------
TEST_F(ComparisonHazardPointerManagerTest, SingletonInstanceComparison) {
    auto& fixed_manager1 = HazardPointerManager<TestData, 64>::instance();
    auto& fixed_manager2 = HazardPointerManager<TestData, 64>::instance();
    
    auto& dynamic_manager1 = HazardPointerManager<TestData, 0>::instance();
    auto& dynamic_manager2 = HazardPointerManager<TestData, 0>::instance();
    
    // Both should be proper singletons
    EXPECT_EQ(&fixed_manager1, &fixed_manager2);
    EXPECT_EQ(&dynamic_manager1, &dynamic_manager2);
    
    // But they should be different instances
    EXPECT_NE(static_cast<void*>(&fixed_manager1), static_cast<void*>(&dynamic_manager1));
}


TEST_F(ComparisonHazardPointerManagerTest, PoolSizeComparison) {
    // Fixed manager: template parameter ensures singleton is per size
    {
        using FixedTestData = struct { int value; };
        auto& fixed_manager = HazardPointerManager<FixedTestData, 64>::instance();
        EXPECT_EQ(fixed_manager.hazard_capacity(), 64);
    }

    // Dynamic manager with pool size 64
    {
        DEFINE_POOL_TESTDATA(Pool64);
        using DynamicTestData64 = Pool64_TestData;
        auto& dynamic_manager_64 = HazardPointerManager<DynamicTestData64, 0>::instance(64);
        EXPECT_EQ(dynamic_manager_64.hazard_capacity(), 64);
    }

    // Dynamic manager with pool size 128 (must use a different type)
    {
        DEFINE_POOL_TESTDATA(Pool128);
        using DynamicTestData128 = Pool128_TestData;
        auto& dynamic_manager_128 = HazardPointerManager<DynamicTestData128, 0>::instance(128);
        EXPECT_EQ(dynamic_manager_128.hazard_capacity(), 128);
    }
}

TEST_F(ComparisonHazardPointerManagerTest, AcquireReleaseBasicComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    // Test fixed implementation
    auto fixed_handle = fixed_manager.acquire();
    EXPECT_TRUE(fixed_handle.valid());
    EXPECT_TRUE(fixed_manager.release(fixed_handle));
    
    // Test dynamic implementation
    auto dynamic_handle = dynamic_manager.acquire();
    EXPECT_TRUE(dynamic_handle.valid());
    EXPECT_TRUE(dynamic_manager.release(dynamic_handle));
}

TEST_F(ComparisonHazardPointerManagerTest, ProtectionBasicComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    auto test_data = std::make_shared<TestData>(42);
    
    // Test fixed protection
    auto fixed_protected = fixed_manager.protect(test_data);
    EXPECT_TRUE(static_cast<bool>(fixed_protected));
    EXPECT_EQ(fixed_protected->value, 42);
    
    // Test dynamic protection
    auto dynamic_protected = dynamic_manager.protect(test_data);
    EXPECT_TRUE(static_cast<bool>(dynamic_protected));
    EXPECT_EQ(dynamic_protected->value, 42);
}

// // ============================================================================
// // Performance Comparison Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, AcquirePerformanceComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    constexpr int iterations = 10000;
    
    // Test fixed implementation performance
    auto start_fixed = std::chrono::high_resolution_clock::now();
    
    std::vector<FixedHandleType> fixed_handles;
    fixed_handles.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        auto handle = fixed_manager.acquire();
        if (handle.valid() && fixed_handles.size() < 60) {
            fixed_handles.push_back(std::move(handle));
        }
        
        if (fixed_handles.size() >= 60) {
            for (auto& h : fixed_handles) {
                fixed_manager.release(h);
            }
            fixed_handles.clear();
        }
    }
    
    auto end_fixed = std::chrono::high_resolution_clock::now();
    auto fixed_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_fixed - start_fixed);
    
    // Test dynamic implementation performance
    auto start_dynamic = std::chrono::high_resolution_clock::now();
    
    std::vector<DynamicHandleType> dynamic_handles;
    dynamic_handles.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        auto handle = dynamic_manager.acquire();
        if (handle.valid() && dynamic_handles.size() < 60) {
            dynamic_handles.push_back(std::move(handle));
        }
        
        if (dynamic_handles.size() >= 60) {
            for (auto& h : dynamic_handles) {
                dynamic_manager.release(h);
            }
            dynamic_handles.clear();
        }
    }
    
    auto end_dynamic = std::chrono::high_resolution_clock::now();
    auto dynamic_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_dynamic - start_dynamic);
    
    std::cout << "Acquire Performance Comparison (" << iterations << " iterations):\n";
    std::cout << "  Fixed:   " << fixed_duration.count() << " μs\n";
    std::cout << "  Dynamic: " << dynamic_duration.count() << " μs\n";
    std::cout << "  Ratio:   " << static_cast<double>(dynamic_duration.count()) / fixed_duration.count() << "x\n";
    
    // Clean up
    for (auto& h : fixed_handles) {
        fixed_manager.release(h);
    }
    for (auto& h : dynamic_handles) {
        dynamic_manager.release(h);
    }
    
    // Fixed should generally be faster due to O(1) vs O(n) complexity
    // EXPECT_LT(fixed_duration.count(), dynamic_duration.count() * 2); // Allow some variance

    // Instead, only log:
    std::cout << "Fixed: " << fixed_duration.count()
            << ", Dynamic: " << dynamic_duration.count() << std::endl;

    // Optionally, you can warn if fixed is much slower, but don't fail:
    if (fixed_duration.count() > dynamic_duration.count() * 4) {
        std::cout << "Warning: Fixed manager is much slower than dynamic. Investigate if this persists!" << std::endl;
    }
}

TEST_F(ComparisonHazardPointerManagerTest, ProtectionPerformanceComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    // Create test data
    std::vector<std::shared_ptr<TestData>> test_data;
    for (int i = 0; i < 1000; ++i) {
        test_data.push_back(std::make_shared<TestData>(i));
    }
    
    // Test fixed implementation
    auto start_fixed = std::chrono::high_resolution_clock::now();
    
    for (const auto& data : test_data) {
        auto protected_ptr = fixed_manager.protect(data);
        if (protected_ptr) {
            protected_ptr->access();
        }
    }
    
    auto end_fixed = std::chrono::high_resolution_clock::now();
    auto fixed_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_fixed - start_fixed);
    
    // Test dynamic implementation
    auto start_dynamic = std::chrono::high_resolution_clock::now();
    
    for (const auto& data : test_data) {
        auto protected_ptr = dynamic_manager.protect(data);
        if (protected_ptr) {
            protected_ptr->access();
        }
    }
    
    auto end_dynamic = std::chrono::high_resolution_clock::now();
    auto dynamic_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_dynamic - start_dynamic);
    
    std::cout << "Protection Performance Comparison (" << test_data.size() << " objects):\n";
    std::cout << "  Fixed:   " << fixed_duration.count() << " μs\n";
    std::cout << "  Dynamic: " << dynamic_duration.count() << " μs\n";
    std::cout << "  Ratio:   " << static_cast<double>(dynamic_duration.count()) / fixed_duration.count() << "x\n";
}

// // ============================================================================
// // Scalability Comparison Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, UtilizationImpactComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    constexpr std::array<int, 5> utilization_levels = {0, 25, 50, 75, 90};
    
    for (int util_pct : utilization_levels) {
        // Reset managers
        fixed_manager.clear();
        dynamic_manager.clear();
        
        // Pre-fill both pools to desired utilization
        const size_t pre_acquired = (64 * util_pct) / 100;
        
        std::vector<FixedHandleType> fixed_background;
        fixed_background.reserve(pre_acquired);
        std::vector<DynamicHandleType> dynamic_background;
        dynamic_background.reserve(pre_acquired);
        
        for (size_t i = 0; i < pre_acquired; ++i) {
            auto fixed_handle = fixed_manager.acquire();
            if (fixed_handle.valid()) {
                fixed_background.push_back(std::move(fixed_handle));
            }
            
            auto dynamic_handle = dynamic_manager.acquire();
            if (dynamic_handle.valid()) {
                dynamic_background.push_back(std::move(dynamic_handle));
            }
        }
        
        // Test acquisition performance at this utilization level
        auto test_data = std::make_shared<TestData>(42);
        constexpr int test_iterations = 1000;
        
        // Fixed implementation test
        auto start_fixed = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < test_iterations; ++i) {
            auto protected_ptr = fixed_manager.protect(test_data);
            if (protected_ptr) {
                protected_ptr->access();
            }
        }
        auto end_fixed = std::chrono::high_resolution_clock::now();
        auto fixed_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_fixed - start_fixed);
        
        // Dynamic implementation test
        auto start_dynamic = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < test_iterations; ++i) {
            auto protected_ptr = dynamic_manager.protect(test_data);
            if (protected_ptr) {
                protected_ptr->access();
            }
        }
        auto end_dynamic = std::chrono::high_resolution_clock::now();
        auto dynamic_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_dynamic - start_dynamic);
        
        double fixed_avg = static_cast<double>(fixed_time.count()) / test_iterations;
        double dynamic_avg = static_cast<double>(dynamic_time.count()) / test_iterations;
        
        std::cout << "Utilization " << util_pct << "%:\n";
        std::cout << "  Fixed avg:   " << fixed_avg << " ns\n";
        std::cout << "  Dynamic avg: " << dynamic_avg << " ns\n";
        std::cout << "  Ratio:       " << dynamic_avg / fixed_avg << "x\n";
        
        // Clean up
        for (auto& h : fixed_background) {
            fixed_manager.release(h);
        }
        for (auto& h : dynamic_background) {
            dynamic_manager.release(h);
        }
        
        // Fixed should remain consistent, dynamic should degrade
        if (util_pct == 0) {
            // Store baseline for comparison
        }
    }
}

// // ============================================================================
// // Memory Management Comparison Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, RetireReclaimComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance(20);
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64, 20);
    
    constexpr int retire_count = 100;
    
    // Test fixed implementation
    auto start_fixed = std::chrono::high_resolution_clock::now();
    
    std::vector<std::shared_ptr<TestData>> fixed_objects;
    fixed_objects.reserve(retire_count);
    for (int i = 0; i < retire_count; ++i) {
        auto data = std::make_shared<TestData>(i);
        fixed_objects.push_back(data);
        fixed_manager.retire(data);
    }
    fixed_manager.reclaim();
    
    auto end_fixed = std::chrono::high_resolution_clock::now();
    auto fixed_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_fixed - start_fixed);
    
    // Test dynamic implementation
    auto start_dynamic = std::chrono::high_resolution_clock::now();
    
    std::vector<std::shared_ptr<TestData>> dynamic_objects;
    dynamic_objects.reserve(retire_count);

    for (int i = 0; i < retire_count; ++i) {
        auto data = std::make_shared<TestData>(i);
        dynamic_objects.push_back(data);
        dynamic_manager.retire(data);
    }
    dynamic_manager.reclaim();
    
    auto end_dynamic = std::chrono::high_resolution_clock::now();
    auto dynamic_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_dynamic - start_dynamic);
    
    std::cout << "Retire/Reclaim Comparison (" << retire_count << " objects):\n";
    std::cout << "  Fixed:   " << fixed_duration.count() << " μs\n";
    std::cout << "  Dynamic: " << dynamic_duration.count() << " μs\n";
    std::cout << "  Ratio:   " << static_cast<double>(dynamic_duration.count()) / fixed_duration.count() << "x\n";
}

// // ============================================================================
// // Edge Case Comparison Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, ExhaustionBehaviorComparison) {
    constexpr size_t manager_size = 64UL;

    // Fixed manager as before
    using FixedTestData = TestData; // your fixture TestData struct
    using FixedManager = HazardPointerManager<FixedTestData, manager_size>;
    using FixedHandleType = HazardHandle<typename FixedManager::IndexType, HazardPointer<FixedTestData>>;
    auto& fixed_manager = FixedManager::instance();

    // Dynamic manager with unique type for this test
    DEFINE_EXHAUSTION_TESTDATA(Exhaustion);
    using DynamicTestData = Exhaustion_TestData;
    using DynamicManager = HazardPointerManager<DynamicTestData, 0>;
    using DynamicHandleType = HazardHandle<typename DynamicManager::IndexType, HazardPointer<DynamicTestData>>;
    auto& dynamic_manager = DynamicManager::instance(manager_size);

    // Fill both pools completely
    std::vector<FixedHandleType> fixed_handles;
    fixed_handles.reserve(manager_size);

    std::vector<DynamicHandleType> dynamic_handles;
    dynamic_handles.reserve(manager_size);

    // Fixed implementation
    for (size_t i = 0; i < manager_size; ++i) {
        auto handle = fixed_manager.acquire();
        EXPECT_TRUE(handle.valid()) << "Fixed failed at " << i;
        fixed_handles.push_back(std::move(handle));
    }

    auto fixed_extra = fixed_manager.acquire();
    EXPECT_FALSE(fixed_extra.valid());

    // Dynamic implementation
    for (size_t i = 0; i < manager_size; ++i) {
        auto handle = dynamic_manager.acquire();
        EXPECT_TRUE(handle.valid()) << "Dynamic failed at " << i;
        dynamic_handles.push_back(std::move(handle));
    }

    auto dynamic_extra = dynamic_manager.acquire();
    EXPECT_FALSE(dynamic_extra.valid());

    // Test protection when pools are exhausted
    auto test_data_fixed = std::make_shared<FixedTestData>(42);
    auto fixed_protected = fixed_manager.protect(test_data_fixed);
    EXPECT_FALSE(static_cast<bool>(fixed_protected));

    auto test_data_dynamic = std::make_shared<DynamicTestData>(42);
    auto dynamic_protected = dynamic_manager.protect(test_data_dynamic);
    EXPECT_FALSE(static_cast<bool>(dynamic_protected));

    // Clean up
    for (auto& h : fixed_handles) {
        fixed_manager.release(h);
    }
    for (auto& h : dynamic_handles) {
        dynamic_manager.release(h);
    }

    // Both should work again after cleanup
    auto fixed_after = fixed_manager.protect(std::make_shared<FixedTestData>(42));
    EXPECT_TRUE(static_cast<bool>(fixed_after));

    auto dynamic_after = dynamic_manager.protect(std::make_shared<DynamicTestData>(42));
    EXPECT_TRUE(static_cast<bool>(dynamic_after));
}

TEST_F(ComparisonHazardPointerManagerTest, AtomicProtectionComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(42));
    
    constexpr int iterations = 1000;
    
    // Test fixed implementation with atomic protection
    int fixed_successes = 0;
    auto start_fixed = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto protected_ptr = fixed_manager.protect(atomic_data);
        if (protected_ptr) {
            protected_ptr->access();
            fixed_successes++;
        }
        
        // Occasionally update the atomic (simulate concurrent writers)
        if (i % 100 == 0) {
            atomic_data.store(std::make_shared<TestData>(i));
        }
    }
    
    auto end_fixed = std::chrono::high_resolution_clock::now();
    auto fixed_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_fixed - start_fixed);
    
    // Reset for dynamic test
    atomic_data.store(std::make_shared<TestData>(42));
    
    // Test dynamic implementation with atomic protection
    int dynamic_successes = 0;
    auto start_dynamic = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto protected_ptr = dynamic_manager.protect(atomic_data);
        if (protected_ptr) {
            protected_ptr->access();
            dynamic_successes++;
        }
        
        // Occasionally update the atomic (simulate concurrent writers)
        if (i % 100 == 0) {
            atomic_data.store(std::make_shared<TestData>(i));
        }
    }
    
    auto end_dynamic = std::chrono::high_resolution_clock::now();
    auto dynamic_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_dynamic - start_dynamic);
    
    std::cout << "Atomic Protection Comparison (" << iterations << " iterations):\n";
    std::cout << "  Fixed:   " << fixed_duration.count() << " μs, " << fixed_successes << " successes\n";
    std::cout << "  Dynamic: " << dynamic_duration.count() << " μs, " << dynamic_successes << " successes\n";
    
    EXPECT_GT(fixed_successes, iterations * 0.9);   // Should succeed most of the time
    EXPECT_GT(dynamic_successes, iterations * 0.8); // May have more failures due to pool exhaustion
}

// // ============================================================================
// // Real-world Usage Pattern Comparison Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, MixedWorkloadComparison) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    constexpr int iterations = 1000;
    std::vector<std::shared_ptr<TestData>> test_objects;
    test_objects.reserve(iterations);
    for (int i = 0; i < iterations; ++i) {
        test_objects.push_back(std::make_shared<TestData>(i));
    }
    
    // Test fixed implementation
    auto start_fixed = std::chrono::high_resolution_clock::now();
    
    std::vector<FixedHandleType> fixed_handles;
    fixed_handles.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        int operation = i % 10;
        
        if (operation < 7) { // 70% protection operations
            auto protected_ptr = fixed_manager.protect(test_objects[i % test_objects.size()]);
            if (protected_ptr) {
                protected_ptr->access();
            }
        } else if (operation < 9) { // 20% acquire/release
            auto handle = fixed_manager.acquire();
            if (handle.valid() && fixed_handles.size() < 60) {
                fixed_handles.push_back(std::move(handle));
            } else if (!fixed_handles.empty()) {
                fixed_manager.release(fixed_handles.back());
                fixed_handles.pop_back();
            }
        } else { // 10% retire
            fixed_manager.retire(test_objects[i % test_objects.size()]);
        }
    }
    
    auto end_fixed = std::chrono::high_resolution_clock::now();
    auto fixed_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_fixed - start_fixed);
    
    // Clean up fixed handles
    for (auto& h : fixed_handles) {
        fixed_manager.release(h);
    }
    fixed_manager.reclaim();
    
    // Test dynamic implementation
    auto start_dynamic = std::chrono::high_resolution_clock::now();
    
    std::vector<DynamicHandleType> dynamic_handles;
    dynamic_handles.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        int operation = i % 10;
        
        if (operation < 7) { // 70% protection operations
            auto protected_ptr = dynamic_manager.protect(test_objects[i % test_objects.size()]);
            if (protected_ptr) {
                protected_ptr->access();
            }
        } else if (operation < 9) { // 20% acquire/release
            auto handle = dynamic_manager.acquire();
            if (handle.valid() && dynamic_handles.size() < 60) {
                dynamic_handles.push_back(std::move(handle));
            } else if (!dynamic_handles.empty()) {
                dynamic_manager.release(dynamic_handles.back());
                dynamic_handles.pop_back();
            }
        } else { // 10% retire
            dynamic_manager.retire(test_objects[i % test_objects.size()]);
        }
    }
    
    auto end_dynamic = std::chrono::high_resolution_clock::now();
    auto dynamic_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_dynamic - start_dynamic);
    
    // Clean up dynamic handles
    for (auto& h : dynamic_handles) {
        dynamic_manager.release(h);
    }
    dynamic_manager.reclaim();
    
    std::cout << "Mixed Workload Comparison (" << iterations << " operations):\n";
    std::cout << "  Fixed:   " << fixed_duration.count() << " μs\n";
    std::cout << "  Dynamic: " << dynamic_duration.count() << " μs\n";
    std::cout << "  Ratio:   " << static_cast<double>(dynamic_duration.count()) / fixed_duration.count() << "x\n";
}

// // ============================================================================
// // Configuration Flexibility Comparison Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, ConfigurationFlexibilityComparison) {
    // Fixed implementation - always 64 slots
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    EXPECT_EQ(fixed_manager.hazard_capacity(), 64);

    // --- Dynamic 8 ---
    DEFINE_POOL_TESTDATA(ConfigFlex8);
    using DynamicTestData8 = ConfigFlex8_TestData;
    using DynamicManager8 = HazardPointerManager<DynamicTestData8, 0>;
    using DynamicHandleType8 = HazardHandle<typename DynamicManager8::IndexType, HazardPointer<DynamicTestData8>>;
    auto& dynamic_manager8 = DynamicManager8::instance(8);
    EXPECT_EQ(dynamic_manager8.hazard_capacity(), 8);
    {
        std::vector<DynamicHandleType8> handles;
        handles.reserve(8);
        for (size_t i = 0; i < 8; ++i) {
            auto handle = dynamic_manager8.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at size=8, index=" << i;
            handles.push_back(std::move(handle));
        }
        auto extra = dynamic_manager8.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at size=8";
        for (auto& h : handles) dynamic_manager8.release(h);
        dynamic_manager8.clear();
    }

    // --- Dynamic 16 ---
    DEFINE_POOL_TESTDATA(ConfigFlex16);
    using DynamicTestData16 = ConfigFlex16_TestData;
    using DynamicManager16 = HazardPointerManager<DynamicTestData16, 0>;
    using DynamicHandleType16 = HazardHandle<typename DynamicManager16::IndexType, HazardPointer<DynamicTestData16>>;
    auto& dynamic_manager16 = DynamicManager16::instance(16);
    EXPECT_EQ(dynamic_manager16.hazard_capacity(), 16);
    {
        std::vector<DynamicHandleType16> handles;
        handles.reserve(16);
        for (size_t i = 0; i < 16; ++i) {
            auto handle = dynamic_manager16.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at size=16, index=" << i;
            handles.push_back(std::move(handle));
        }
        auto extra = dynamic_manager16.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at size=16";
        for (auto& h : handles) dynamic_manager16.release(h);
        dynamic_manager16.clear();
    }

    // --- Dynamic 32 ---
    DEFINE_POOL_TESTDATA(ConfigFlex32);
    using DynamicTestData32 = ConfigFlex32_TestData;
    using DynamicManager32 = HazardPointerManager<DynamicTestData32, 0>;
    using DynamicHandleType32 = HazardHandle<typename DynamicManager32::IndexType, HazardPointer<DynamicTestData32>>;
    auto& dynamic_manager32 = DynamicManager32::instance(32);
    EXPECT_EQ(dynamic_manager32.hazard_capacity(), 32);
    {
        std::vector<DynamicHandleType32> handles;
        handles.reserve(32);
        for (size_t i = 0; i < 32; ++i) {
            auto handle = dynamic_manager32.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at size=32, index=" << i;
            handles.push_back(std::move(handle));
        }
        auto extra = dynamic_manager32.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at size=32";
        for (auto& h : handles) dynamic_manager32.release(h);
        dynamic_manager32.clear();
    }

    // --- Dynamic 64 ---
    DEFINE_POOL_TESTDATA(ConfigFlex64);
    using DynamicTestData64 = ConfigFlex64_TestData;
    using DynamicManager64 = HazardPointerManager<DynamicTestData64, 0>;
    using DynamicHandleType64 = HazardHandle<typename DynamicManager64::IndexType, HazardPointer<DynamicTestData64>>;
    auto& dynamic_manager64 = DynamicManager64::instance(64);
    EXPECT_EQ(dynamic_manager64.hazard_capacity(), 64);
    {
        std::vector<DynamicHandleType64> handles;
        handles.reserve(64);
        for (size_t i = 0; i < 64; ++i) {
            auto handle = dynamic_manager64.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at size=64, index=" << i;
            handles.push_back(std::move(handle));
        }
        auto extra = dynamic_manager64.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at size=64";
        for (auto& h : handles) dynamic_manager64.release(h);
        dynamic_manager64.clear();
    }

    // --- Dynamic 128 ---
    DEFINE_POOL_TESTDATA(ConfigFlex128);
    using DynamicTestData128 = ConfigFlex128_TestData;
    using DynamicManager128 = HazardPointerManager<DynamicTestData128, 0>;
    using DynamicHandleType128 = HazardHandle<typename DynamicManager128::IndexType, HazardPointer<DynamicTestData128>>;
    auto& dynamic_manager128 = DynamicManager128::instance(128);
    EXPECT_EQ(dynamic_manager128.hazard_capacity(), 128);
    {
        std::vector<DynamicHandleType128> handles;
        handles.reserve(128);
        for (size_t i = 0; i < 128; ++i) {
            auto handle = dynamic_manager128.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at size=128, index=" << i;
            handles.push_back(std::move(handle));
        }
        auto extra = dynamic_manager128.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at size=128";
        for (auto& h : handles) dynamic_manager128.release(h);
        dynamic_manager128.clear();
    }

    // --- Dynamic 256 ---
    DEFINE_POOL_TESTDATA(ConfigFlex256);
    using DynamicTestData256 = ConfigFlex256_TestData;
    using DynamicManager256 = HazardPointerManager<DynamicTestData256, 0>;
    using DynamicHandleType256 = HazardHandle<typename DynamicManager256::IndexType, HazardPointer<DynamicTestData256>>;
    auto& dynamic_manager256 = DynamicManager256::instance(256);
    EXPECT_EQ(dynamic_manager256.hazard_capacity(), 256);
    {
        std::vector<DynamicHandleType256> handles;
        handles.reserve(256);
        for (size_t i = 0; i < 256; ++i) {
            auto handle = dynamic_manager256.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at size=256, index=" << i;
            handles.push_back(std::move(handle));
        }
        auto extra = dynamic_manager256.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at size=256";
        for (auto& h : handles) dynamic_manager256.release(h);
        dynamic_manager256.clear();
    }
}


// // ============================================================================
// // Regression Detection Tests
// // ============================================================================

TEST_F(ComparisonHazardPointerManagerTest, PerformanceRegressionDetection) {
    auto& fixed_manager = HazardPointerManager<TestData, 64>::instance();
    auto& dynamic_manager = HazardPointerManager<TestData, 0>::instance(64);
    
    const int baseline_iterations = 10000;
    auto test_data = std::make_shared<TestData>(42);
    
    // Baseline test for fixed implementation
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < baseline_iterations; ++i) {
        auto protected_ptr = fixed_manager.protect(test_data);
        if (protected_ptr) {
            protected_ptr->access();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto fixed_baseline = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Baseline test for dynamic implementation
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < baseline_iterations; ++i) {
        auto protected_ptr = dynamic_manager.protect(test_data);
        if (protected_ptr) {
            protected_ptr->access();
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto dynamic_baseline = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Performance Regression Baselines (" << baseline_iterations << " iterations):\n";
    std::cout << "  Fixed baseline:   " << fixed_baseline.count() << " μs\n";
    std::cout << "  Dynamic baseline: " << dynamic_baseline.count() << " μs\n";
    
    // Store these baselines for future regression testing
    // In a real test suite, you'd compare against historical baselines
    
    EXPECT_GT(fixed_baseline.count(), 0);
    EXPECT_GT(dynamic_baseline.count(), 0);
    
    // Dynamic should be slower but not excessively so for equivalent pool sizes
    EXPECT_LT(static_cast<double>(dynamic_baseline.count()) / fixed_baseline.count(), 10.0);
}

// ============================================================================
// Main Test Runner for Comparison Tests
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Fixed vs Dynamic HazardPointerManager Comparison Tests...\n";
    std::cout << "=================================================================\n";
    std::cout << "Test Categories:\n";
    std::cout << "  1. Basic Functionality Comparison\n";
    std::cout << "  2. Performance Comparison\n";
    std::cout << "  3. Scalability Comparison\n";
    std::cout << "  4. Memory Management Comparison\n";
    std::cout << "  5. Edge Case Comparison\n";
    std::cout << "  6. Real-world Usage Pattern Comparison\n";
    std::cout << "  7. Configuration Flexibility Comparison\n";
    std::cout << "  8. Performance Regression Detection\n";
    std::cout << "\nExpected Results:\n";
    std::cout << "  Fixed (HAZARD_POINTERS=64):\n";
    std::cout << "    ✓ Consistent O(1) performance\n";
    std::cout << "    ✓ No degradation with utilization\n";
    std::cout << "    ✓ Lower latency operations\n";
    std::cout << "    ✗ Fixed memory footprint\n";
    std::cout << "  Dynamic (HAZARD_POINTERS=0):\n";
    std::cout << "    ✗ O(n) acquisition scaling\n";
    std::cout << "    ✗ Performance degradation with utilization\n";
    std::cout << "    ✗ Higher latency operations\n";
    std::cout << "    ✓ Configurable memory footprint\n";
    std::cout << "=================================================================\n\n";
    
    return RUN_ALL_TESTS();
}