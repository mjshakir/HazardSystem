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
// Test Fixture for Dynamic HazardPointerManager (HAZARD_POINTERS = 0)
// ============================================================================

class DynamicHazardPointerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ThreadRegistry::instance().register_id();
    }

    void TearDown() override {
        auto& manager = HazardPointerManager<TestData, 0>::instance();
        manager.clear();
    }

    struct TestData {
        int value;
        std::atomic<bool> destroyed{false};
        std::atomic<int> access_count{0};
        
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
    
    using DynamicManagerType = HazardPointerManager<TestData, 0>;
    using DynamicHandleType = HazardHandle<typename DynamicManagerType::IndexType, HazardPointer<TestData>>;
};

// ============================================================================
// Basic Functionality Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, SingletonInstance) {
    auto& manager1 = HazardPointerManager<TestData, 0>::instance();
    auto& manager2 = HazardPointerManager<TestData, 0>::instance();
    
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(DynamicHazardPointerManagerTest, DynamicSizing) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(20, 5); // 20 hazards, 5 retire size
    
    EXPECT_EQ(manager.hazard_size(), 20);
    
    std::vector<DynamicHandleType> handles;
    
    // Should be able to acquire up to the dynamic size
    for (size_t i = 0; i < 20; ++i) {
        auto handle = manager.acquire();
        EXPECT_TRUE(handle.valid()) << "Failed at index " << i;
        handles.push_back(std::move(handle));
    }
    
    // One more should fail
    auto extra = manager.acquire();
    EXPECT_FALSE(extra.valid());
    
    for (auto& handle : handles) {
        manager.release(handle);
    }
}

TEST_F(DynamicHazardPointerManagerTest, VariablePoolSizes) {
    std::vector<size_t> pool_sizes = {1, 4, 16, 64, 128, 256};
    
    for (size_t pool_size : pool_sizes) {
        auto& manager = HazardPointerManager<TestData, 0>::instance(pool_size);
        manager.clear(); // Reset for each test
        
        EXPECT_EQ(manager.hazard_size(), pool_size);
        
        std::vector<DynamicHandleType> handles;
        
        // Acquire up to pool size
        for (size_t i = 0; i < pool_size; ++i) {
            auto handle = manager.acquire();
            EXPECT_TRUE(handle.valid()) << "Failed at pool_size=" << pool_size << ", index=" << i;
            handles.push_back(std::move(handle));
        }
        
        // Next should fail
        auto extra = manager.acquire();
        EXPECT_FALSE(extra.valid()) << "Should fail at pool_size=" << pool_size;
        
        // Clean up
        for (auto& handle : handles) {
            manager.release(handle);
        }
    }
}

TEST_F(DynamicHazardPointerManagerTest, AcquireAndRelease) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    
    auto handle = manager.acquire();
    EXPECT_TRUE(handle.valid());
    EXPECT_TRUE(handle.index.has_value());
    EXPECT_NE(handle.sp_data, nullptr);
    
    bool released = manager.release(handle);
    EXPECT_TRUE(released);
}

TEST_F(DynamicHazardPointerManagerTest, LinearAcquisitionScaling) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(100);
    
    std::vector<DynamicHandleType> handles;
    
    // Test that acquisition time increases as pool fills up
    // (This is a behavioral test, not a strict timing test)
    for (size_t i = 0; i < 100; ++i) {
        auto handle = manager.acquire();
        EXPECT_TRUE(handle.valid()) << "Failed to acquire handle " << i;
        handles.push_back(std::move(handle));
    }
    
    // Should fail when pool is exhausted
    auto extra_handle = manager.acquire();
    EXPECT_FALSE(extra_handle.valid());
    
    // Release all handles
    for (auto& handle : handles) {
        EXPECT_TRUE(manager.release(handle));
    }
}

TEST_F(DynamicHazardPointerManagerTest, ProtectSharedPtr) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 42);
    EXPECT_EQ(protected_ptr.get()->value, 42);
    EXPECT_EQ((*protected_ptr).value, 42);
}

TEST_F(DynamicHazardPointerManagerTest, ProtectAtomicSharedPtr) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(100));
    
    auto protected_ptr = manager.protect(atomic_data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 100);
}

TEST_F(DynamicHazardPointerManagerTest, TryProtectWithRetries) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(200));
    
    auto protected_ptr = manager.try_protect(atomic_data, 10);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 200);
}

TEST_F(DynamicHazardPointerManagerTest, RetireAndReclaim) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10, 2); // Small retire size
    
    auto data1 = std::make_shared<TestData>(1);
    auto data2 = std::make_shared<TestData>(2);
    auto data3 = std::make_shared<TestData>(3);
    
    EXPECT_TRUE(manager.retire(data1));
    EXPECT_TRUE(manager.retire(data2));
    EXPECT_EQ(manager.retire_size(), 2);
    
    // This should trigger reclamation
    EXPECT_TRUE(manager.retire(data3));
}

// ============================================================================
// Scalability Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, ScalabilityAcrossPoolSizes) {
    std::vector<size_t> pool_sizes = {8, 16, 32, 64, 128};
    
    for (size_t pool_size : pool_sizes) {
        auto& manager = HazardPointerManager<TestData, 0>::instance(pool_size);
        manager.clear();
        
        // Test acquire performance
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<DynamicHandleType> handles;
        for (size_t i = 0; i < pool_size; ++i) {
            auto handle = manager.acquire();
            if (handle.valid()) {
                handles.push_back(std::move(handle));
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Pool size: " << pool_size 
                  << ", Acquire time: " << duration.count() << " μs"
                  << ", Handles acquired: " << handles.size() << std::endl;
        
        EXPECT_EQ(handles.size(), pool_size);
        
        // Clean up
        for (auto& handle : handles) {
            manager.release(handle);
        }
    }
}

TEST_F(DynamicHazardPointerManagerTest, UtilizationImpact) {
    const size_t pool_size = 64;
    auto& manager = HazardPointerManager<TestData, 0>::instance(pool_size);
    
    std::vector<size_t> utilization_levels = {0, 25, 50, 75, 90}; // Percentage
    
    for (size_t util_pct : utilization_levels) {
        manager.clear();
        
        // Pre-fill pool to desired utilization
        std::vector<DynamicHandleType> background_handles;
        size_t pre_acquired = (pool_size * util_pct) / 100;
        
        for (size_t i = 0; i < pre_acquired; ++i) {
            auto handle = manager.acquire();
            if (handle.valid()) {
                background_handles.push_back(std::move(handle));
            }
        }
        
        // Measure acquisition time at this utilization level
        const int test_acquisitions = 100;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<DynamicHandleType> test_handles;
        for (int i = 0; i < test_acquisitions; ++i) {
            auto handle = manager.acquire();
            if (handle.valid()) {
                test_handles.push_back(std::move(handle));
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        double avg_time = static_cast<double>(duration.count()) / test_handles.size();
        
        std::cout << "Utilization: " << util_pct << "%, "
                  << "Avg acquire time: " << avg_time << " ns, "
                  << "Successful: " << test_handles.size() << "/" << test_acquisitions << std::endl;
        
        // Clean up
        for (auto& handle : test_handles) {
            manager.release(handle);
        }
        for (auto& handle : background_handles) {
            manager.release(handle);
        }
        
        // Higher utilization should show degraded performance
        if (util_pct > 50) {
            EXPECT_LT(test_handles.size(), test_acquisitions);
        }
    }
}

// ============================================================================
// Edge Cases Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, ProtectNullptr) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    
    std::shared_ptr<TestData> null_ptr;
    auto protected_ptr = manager.protect(null_ptr);
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
}

TEST_F(DynamicHazardPointerManagerTest, ProtectAtomicNullptr) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    
    std::atomic<std::shared_ptr<TestData>> atomic_null;
    atomic_null.store(nullptr);
    
    auto protected_ptr = manager.protect(atomic_null);
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
}

TEST_F(DynamicHazardPointerManagerTest, ReleaseInvalidHandle) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    
    DynamicHandleType invalid_handle;
    EXPECT_FALSE(manager.release(invalid_handle));
}

TEST_F(DynamicHazardPointerManagerTest, RetireNullptr) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    
    std::shared_ptr<TestData> null_ptr;
    EXPECT_FALSE(manager.retire(null_ptr));
}

TEST_F(DynamicHazardPointerManagerTest, AcquireFromEmptyPool) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(0); // Zero-sized pool
    
    auto handle = manager.acquire();
    EXPECT_FALSE(handle.valid());
}

TEST_F(DynamicHazardPointerManagerTest, AcquireFromSingleSlotPool) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(1); // Single slot
    
    auto handle1 = manager.acquire();
    EXPECT_TRUE(handle1.valid());
    
    auto handle2 = manager.acquire();
    EXPECT_FALSE(handle2.valid());
    
    EXPECT_TRUE(manager.release(handle1));
    
    auto handle3 = manager.acquire();
    EXPECT_TRUE(handle3.valid());
    
    EXPECT_TRUE(manager.release(handle3));
}

// ============================================================================
// ProtectedPointer Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, ProtectedPointerMoveSemantic) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr1 = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr1));
    
    // Move constructor
    auto protected_ptr2 = std::move(protected_ptr1);
    EXPECT_FALSE(static_cast<bool>(protected_ptr1)); // Moved from
    EXPECT_TRUE(static_cast<bool>(protected_ptr2));
    EXPECT_EQ(protected_ptr2->value, 42);
    
    // Move assignment
    auto protected_ptr3 = manager.protect(std::make_shared<TestData>(100));
    protected_ptr3 = std::move(protected_ptr2);
    EXPECT_FALSE(static_cast<bool>(protected_ptr2)); // Moved from
    EXPECT_TRUE(static_cast<bool>(protected_ptr3));
    EXPECT_EQ(protected_ptr3->value, 42);
}

TEST_F(DynamicHazardPointerManagerTest, ProtectedPointerReset) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    
    protected_ptr.reset();
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
}

TEST_F(DynamicHazardPointerManagerTest, ProtectedPointerAccessors) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr = manager.protect(data);
    ASSERT_TRUE(static_cast<bool>(protected_ptr));
    
    // Test all access methods
    EXPECT_EQ(protected_ptr->value, 42);
    EXPECT_EQ((*protected_ptr).value, 42);
    EXPECT_EQ(protected_ptr.get()->value, 42);
    EXPECT_EQ(protected_ptr.shared_ptr()->value, 42);
}

// ============================================================================
// Performance Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, WorstCasePerformance) {
    const size_t pool_size = 64;
    auto& manager = HazardPointerManager<TestData, 0>::instance(pool_size);
    
    // Fill all but one slot
    std::vector<DynamicHandleType> handles;
    for (size_t i = 0; i < pool_size - 1; ++i) {
        auto handle = manager.acquire();
        ASSERT_TRUE(handle.valid());
        handles.push_back(std::move(handle));
    }
    
    // Now acquisition should scan almost the entire pool
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto handle = manager.acquire();
        if (handle.valid()) {
            manager.release(handle);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Worst-case acquisition (pool " << (pool_size-1) << "/" << pool_size 
              << " full): " << duration.count() << " μs for " << iterations << " iterations" << std::endl;
    
    // Clean up
    for (auto& handle : handles) {
        manager.release(handle);
    }
}

TEST_F(DynamicHazardPointerManagerTest, BestCasePerformance) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(64);
    
    // Empty pool - should find first slot immediately
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto handle = manager.acquire();
        if (handle.valid()) {
            manager.release(handle);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Best-case acquisition (empty pool): " 
              << duration.count() << " μs for " << iterations << " iterations" << std::endl;
}

// ============================================================================
// Stress Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, RapidAcquireReleaseStress) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(32);
    
    const int iterations = 10000;
    int successful_cycles = 0;
    int failed_cycles = 0;
    
    for (int i = 0; i < iterations; ++i) {
        auto handle = manager.acquire();
        if (handle.valid()) {
            successful_cycles++;
            manager.release(handle);
        } else {
            failed_cycles++;
        }
    }
    
    EXPECT_EQ(successful_cycles, iterations);
    EXPECT_EQ(failed_cycles, 0);
}

TEST_F(DynamicHazardPointerManagerTest, MixedOperationsStress) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(32);
    
    std::vector<std::shared_ptr<TestData>> test_objects;
    std::vector<DynamicHandleType> handles;
    
    const int iterations = 1000;
    int operations_completed = 0;
    
    for (int i = 0; i < iterations; ++i) {
        int operation = i % 4;
        
        switch (operation) {
            case 0: { // Acquire
                auto handle = manager.acquire();
                if (handle.valid() && handles.size() < 28) { // Leave some slots free
                    handles.push_back(std::move(handle));
                    operations_completed++;
                }
                break;
            }
            case 1: { // Release
                if (!handles.empty()) {
                    manager.release(handles.back());
                    handles.pop_back();
                    operations_completed++;
                }
                break;
            }
            case 2: { // Protect
                auto data = std::make_shared<TestData>(i);
                test_objects.push_back(data);
                auto protected_ptr = manager.protect(data);
                if (protected_ptr) {
                    protected_ptr->access();
                    operations_completed++;
                }
                break;
            }
            case 3: { // Retire
                if (!test_objects.empty()) {
                    manager.retire(test_objects.back());
                    test_objects.pop_back();
                    operations_completed++;
                }
                break;
            }
        }
    }
    
    // Clean up
    for (auto& handle : handles) {
        manager.release(handle);
    }
    
    EXPECT_GT(operations_completed, iterations / 2);
}

// ============================================================================
// Memory Management Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, MemoryLeakPrevention) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(16, 10); // Small retire threshold
    
    std::atomic<int> objects_created{0};
    std::atomic<int> objects_destroyed{0};
    
    struct TrackedTestData {
        int value;
        std::atomic<int>* created_counter;
        std::atomic<int>* destroyed_counter;
        
        TrackedTestData(int v, std::atomic<int>* created, std::atomic<int>* destroyed) 
            : value(v), created_counter(created), destroyed_counter(destroyed) {
            created_counter->fetch_add(1);
        }
        
        ~TrackedTestData() {
            destroyed_counter->fetch_add(1);
        }
    };
    
    auto& tracked_manager = HazardPointerManager<TrackedTestData, 0>::instance(16, 10);
    
    const int num_objects = 100;
    
    // Create and retire objects
    for (int i = 0; i < num_objects; ++i) {
        auto data = std::make_shared<TrackedTestData>(i, &objects_created, &objects_destroyed);
        tracked_manager.retire(data);
    }
    
    // Force final cleanup
    tracked_manager.reclaim_all();
    
    // Allow some time for destructors
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_EQ(objects_created.load(), num_objects);
    EXPECT_EQ(objects_destroyed.load(), num_objects);
}

TEST_F(DynamicHazardPointerManagerTest, ReclaimScalingWithPoolSize) {
    std::vector<size_t> pool_sizes = {8, 16, 32, 64};
    const int retire_count = 50;
    
    for (size_t pool_size : pool_sizes) {
        auto& manager = HazardPointerManager<TestData, 0>::instance(pool_size, retire_count);
        manager.clear();
        
        // Retire objects
        std::vector<std::shared_ptr<TestData>> objects;
        for (int i = 0; i < retire_count; ++i) {
            auto data = std::make_shared<TestData>(i);
            objects.push_back(data);
            manager.retire(data);
        }
        
        // Measure reclamation time
        auto start = std::chrono::high_resolution_clock::now();
        manager.reclaim();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Pool size: " << pool_size 
                  << ", Reclaim time: " << duration.count() << " μs" << std::endl;
        
        // Larger pool sizes should take longer for reclamation (O(r*h) complexity)
    }
}

// ============================================================================
// Configuration Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, RetireThresholdConfiguration) {
    std::vector<size_t> thresholds = {1, 5, 10, 20, 50};
    
    for (size_t threshold : thresholds) {
        auto& manager = HazardPointerManager<TestData, 0>::instance(16, threshold);
        manager.clear();
        
        // Retire objects up to threshold
        for (size_t i = 0; i < threshold; ++i) {
            auto data = std::make_shared<TestData>(static_cast<int>(i));
            manager.retire(data);
        }
        
        EXPECT_EQ(manager.retire_size(), threshold);
        
        // One more should trigger automatic reclamation
        auto trigger_data = std::make_shared<TestData>(999);
        manager.retire(trigger_data);
        
        // The retire size should be managed automatically
        EXPECT_LE(manager.retire_size(), threshold + 1);
    }
}

// ============================================================================
// Concurrency Simulation Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, SimulatedConcurrentAccess) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(32);
    
    // Simulate multiple "threads" accessing shared data
    std::atomic<std::shared_ptr<TestData>> shared_data;
    shared_data.store(std::make_shared<TestData>(0));
    
    int successful_accesses = 0;
    int data_updates = 0;
    int failed_protections = 0;
    
    for (int i = 0; i < 1000; ++i) {
        if (i % 100 == 0) {
            // Simulate writer updating the data
            auto new_data = std::make_shared<TestData>(i / 100);
            auto old_data = shared_data.exchange(new_data);
            if (old_data) {
                manager.retire(old_data);
                data_updates++;
            }
        } else {
            // Simulate reader protecting the data
            auto protected_ptr = manager.protect(shared_data);
            if (protected_ptr) {
                protected_ptr->access();
                successful_accesses++;
            } else {
                failed_protections++;
            }
        }
    }
    
    EXPECT_GT(successful_accesses, 800);
    EXPECT_EQ(data_updates, 10);
    
    // Some protection failures are acceptable in dynamic implementation
    // due to pool exhaustion under high contention
    std::cout << "Protection failures: " << failed_protections 
              << " out of " << (1000 - data_updates) << " attempts" << std::endl;
    
    manager.reclaim_all();
}

// ============================================================================
// Exception Safety Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, ExceptionDuringProtection) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(10);
    
    struct ThrowingTestData {
        int value;
        ThrowingTestData(int v) : value(v) {}
        
        void dangerous_operation() {
            if (value == 666) {
                throw std::runtime_error("Unlucky number!");
            }
        }
    };
    
    auto& throwing_manager = HazardPointerManager<ThrowingTestData, 0>::instance(10);
    auto dangerous_data = std::make_shared<ThrowingTestData>(666);
    
    try {
        auto protected_ptr = throwing_manager.protect(dangerous_data);
        EXPECT_TRUE(static_cast<bool>(protected_ptr));
        
        EXPECT_THROW(protected_ptr->dangerous_operation(), std::runtime_error);
        
        // The protected pointer should still be valid after the exception
        EXPECT_TRUE(static_cast<bool>(protected_ptr));
        EXPECT_EQ(protected_ptr->value, 666);
        
    } catch (...) {
        FAIL() << "Unexpected exception during protection";
    }
}

// ============================================================================
// Clear Operation Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, ClearOperation) {
    auto& manager = HazardPointerManager<TestData, 0>::instance(20);
    
    // Set up some state
    std::vector<DynamicHandleType> handles;
    for (int i = 0; i < 10; ++i) {
        auto handle = manager.acquire();
        if (handle.valid()) {
            handles.push_back(std::move(handle));
        }
        
        auto data = std::make_shared<TestData>(i);
        manager.retire(data);
    }
    
    EXPECT_GT(manager.retire_size(), 0);
    
    // Clear should reset everything
    manager.clear();
    
    EXPECT_EQ(manager.retire_size(), 0);
    
    // Should be able to acquire fresh handles
    auto new_handle = manager.acquire();
    EXPECT_TRUE(new_handle.valid());
    manager.release(new_handle);
}

// ============================================================================
// Integration Tests for Dynamic Implementation
// ============================================================================

TEST_F(DynamicHazardPointerManagerTest, LockFreeQueueSimulation) {
    // Simulate a lock-free queue scenario with dynamic pool
    struct QueueNode {
        std::atomic<int> data{0};
        std::atomic<std::shared_ptr<QueueNode>> next{nullptr};
        
        QueueNode(int value) : data(value) {}
    };
    
    auto& manager = HazardPointerManager<QueueNode, 0>::instance(16);
    
    std::atomic<std::shared_ptr<QueueNode>> head;
    head.store(std::make_shared<QueueNode>(0)); // Dummy head
    
    int enqueue_count = 0;
    int dequeue_count = 0;
    
    // Simulate operations
    for (int i = 1; i <= 100; ++i) {
        if (i % 3 == 0) {
            // Enqueue operation
            auto new_node = std::make_shared<QueueNode>(i);
            
            auto current_head = manager.protect(head);
            if (current_head) {
                new_node->next.store(current_head.shared_ptr());
                
                auto expected = current_head.shared_ptr();
                if (head.compare_exchange_weak(expected, new_node)) {
                    enqueue_count++;
                }
            }
        } else {
            // Dequeue operation
            auto current_head = manager.protect(head);
            if (current_head) {
                auto next_node = current_head->next.load();
                if (next_node) {
                    auto expected = current_head.shared_ptr();
                    if (head.compare_exchange_weak(expected, next_node)) {
                        manager.retire(current_head.shared_ptr());
                        dequeue_count++;
                    }
                }
            }
        }
    }
    
    EXPECT_GT(enqueue_count, 0);
    EXPECT_GT(dequeue_count, 0);
    
    std::cout << "Queue simulation - Enqueued: " << enqueue_count 
              << ", Dequeued: " << dequeue_count << std::endl;
    
    manager.reclaim_all();
}

// ============================================================================
// Main Test Runner for Dynamic Implementation
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Dynamic HazardPointerManager (HAZARD_POINTERS=0) tests...\n";
    std::cout << "Expected Characteristics:\n";
    std::cout << "- O(n) acquire operations (linear search)\n";
    std::cout << "- O(1) release operations\n";
    std::cout << "- Configurable pool size\n";
    std::cout << "- Performance degradation with higher utilization\n";
    std::cout << "- Flexible memory footprint\n";
    std::cout << "========================================\n\n";
    
    return RUN_ALL_TESTS();
}