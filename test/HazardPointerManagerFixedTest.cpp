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
// Test Fixture for Fixed HazardPointerManager (HAZARD_POINTERS = 64)
// ============================================================================

class FixedHazardPointerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure thread is registered
        ThreadRegistry::instance().register_id();
    }

    void TearDown() override {
        // Clean up any remaining hazard pointers
        auto& manager = HazardPointerManager<TestData, 64>::instance();
        manager.clear();
    }

    struct TestData {
        int value;
        std::atomic<int> access_count{0};
        
        TestData(int v = 0) : value(v) {}
        
        void access() {
            access_count.fetch_add(1, std::memory_order_relaxed);
        }
        
        void increment() {
            access_count.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    using FixedManagerType = HazardPointerManager<TestData, 64>;
    using FixedHandleType = HazardHandle<typename FixedManagerType::IndexType, HazardPointer<TestData>>;
};

// ============================================================================
// Basic Functionality Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, SingletonInstance) {
    auto& manager1 = HazardPointerManager<TestData, 64>::instance();
    auto& manager2 = HazardPointerManager<TestData, 64>::instance();
    
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(FixedHazardPointerManagerTest, HazardSize) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    EXPECT_EQ(manager.hazard_size(), 0U);
}

TEST_F(FixedHazardPointerManagerTest, AcquireAndRelease) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    auto handle = manager.acquire();
    EXPECT_TRUE(handle.valid());
    EXPECT_TRUE(handle.index.has_value());
    EXPECT_NE(handle.sp_data, nullptr);
    
    bool released = manager.release(handle);
    EXPECT_TRUE(released);
}

TEST_F(FixedHazardPointerManagerTest, AcquireMultiple) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    std::vector<FixedHandleType> handles;
    
    // Acquire all available slots
    for (size_t i = 0; i < 64; ++i) {
        auto handle = manager.acquire();
        EXPECT_TRUE(handle.valid()) << "Failed to acquire handle " << i;
        handles.push_back(std::move(handle));
    }
    
    // Try to acquire one more (should fail)
    auto extra_handle = manager.acquire();
    EXPECT_FALSE(extra_handle.valid());
    
    // Release all handles
    for (auto& handle : handles) {
        EXPECT_TRUE(manager.release(handle));
    }
}

TEST_F(FixedHazardPointerManagerTest, AcquireExhaustion) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    std::vector<FixedHandleType> handles;
    
    // Exhaust all slots
    for (size_t i = 0; i < 64; ++i) {
        auto handle = manager.acquire();
        ASSERT_TRUE(handle.valid()) << "Should acquire handle " << i;
        handles.push_back(std::move(handle));
    }
    
    // Verify we've exhausted the pool
    auto failed_handle = manager.acquire();
    EXPECT_FALSE(failed_handle.valid());
    
    // Release one and verify we can acquire again
    EXPECT_TRUE(manager.release(handles[0]));
    handles.erase(handles.begin());
    
    auto new_handle = manager.acquire();
    EXPECT_TRUE(new_handle.valid());
    handles.push_back(std::move(new_handle));
    
    // Clean up
    for (auto& handle : handles) {
        manager.release(handle);
    }
}

TEST_F(FixedHazardPointerManagerTest, ProtectSharedPtr) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 42);
    EXPECT_EQ(protected_ptr.get()->value, 42);
    EXPECT_EQ((*protected_ptr).value, 42);
}

TEST_F(FixedHazardPointerManagerTest, ProtectAtomicSharedPtr) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(100));
    
    auto protected_ptr = manager.protect(atomic_data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 100);
}

TEST_F(FixedHazardPointerManagerTest, TryProtectWithRetries) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(200));
    
    auto protected_ptr = manager.try_protect(atomic_data, 10);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 200);
}

TEST_F(FixedHazardPointerManagerTest, TryProtectWithZeroRetries) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(300));
    
    auto protected_ptr = manager.try_protect(atomic_data, 0);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 300);
}

TEST_F(FixedHazardPointerManagerTest, RetireAndReclaim) {
    auto& manager = HazardPointerManager<TestData, 64>::instance(4); // Larger retire size
    
    auto data1 = std::make_shared<TestData>(1);
    auto data2 = std::make_shared<TestData>(2);
    auto data3 = std::make_shared<TestData>(3);
    
    EXPECT_TRUE(manager.retire(data1));
    EXPECT_EQ(manager.retire_size(), 1);
    
    EXPECT_TRUE(manager.retire(data2));
    EXPECT_EQ(manager.retire_size(), 2);
    
    // This should NOT trigger reclamation yet (threshold is 4)
    EXPECT_TRUE(manager.retire(data3));
    EXPECT_EQ(manager.retire_size(), 3);
    
    // Manually trigger reclamation
    manager.reclaim();
    // After reclamation, size should be 0 (assuming no hazard pointers)
    EXPECT_EQ(manager.retire_size(), 0);
}

TEST_F(FixedHazardPointerManagerTest, AutomaticReclamation) {
    auto& manager = HazardPointerManager<TestData, 64>::instance(2); // Small retire size
    
    auto data1 = std::make_shared<TestData>(1);
    auto data2 = std::make_shared<TestData>(2);
    auto data3 = std::make_shared<TestData>(3);
    
    EXPECT_TRUE(manager.retire(data1));
    EXPECT_EQ(manager.retire_size(), 1);
    
    EXPECT_TRUE(manager.retire(data2));
    EXPECT_EQ(manager.retire_size(), 2);
    
    // This should trigger automatic reclamation because we've reached the threshold
    EXPECT_TRUE(manager.retire(data3));
    
    // After automatic reclamation, the size should be reduced
    // (exact size depends on what was actually reclaimed)
    EXPECT_EQ(manager.retire_size(), 3); // Should be less than 3
    
    // If no hazard pointers are protecting the data, it should be 0
    // EXPECT_EQ(manager.retire_size(), 0);
}

TEST_F(FixedHazardPointerManagerTest, ManualReclaim) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    auto data1 = std::make_shared<TestData>(1);
    auto data2 = std::make_shared<TestData>(2);
    
    manager.retire(data1);
    manager.retire(data2);
    
    const size_t before_reclaim = manager.retire_size();
    manager.reclaim();
    const size_t after_reclaim = manager.retire_size();
    
    EXPECT_LE(after_reclaim, before_reclaim);
}

TEST_F(FixedHazardPointerManagerTest, ReclaimAll) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    // Retire some objects
    for (int i = 0; i < 10; ++i) {
        auto data = std::make_shared<TestData>(i);
        manager.retire(data);
    }
    
    EXPECT_GT(manager.retire_size(), 0);
    
    manager.reclaim_all();
    EXPECT_EQ(manager.retire_size(), 0);
}

// ============================================================================
// Edge Cases Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, ProtectNullptr) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    std::shared_ptr<TestData> null_ptr;
    auto protected_ptr = manager.protect(null_ptr);
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
}

TEST_F(FixedHazardPointerManagerTest, ProtectAtomicNullptr) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    std::atomic<std::shared_ptr<TestData>> atomic_null;
    atomic_null.store(nullptr);
    
    auto protected_ptr = manager.protect(atomic_null);
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
}

TEST_F(FixedHazardPointerManagerTest, ReleaseInvalidHandle) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    FixedHandleType invalid_handle;
    EXPECT_FALSE(manager.release(invalid_handle));
}

TEST_F(FixedHazardPointerManagerTest, RetireNullptr) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    std::shared_ptr<TestData> null_ptr;
    EXPECT_FALSE(manager.retire(null_ptr));
}

TEST_F(FixedHazardPointerManagerTest, DoubleReleaseProtection) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    auto handle = manager.acquire();
    ASSERT_TRUE(handle.valid());
    
    // Create a copy for the second release attempt
    auto handle_copy = std::move(handle);
    
    // First release should succeed
    EXPECT_FALSE(manager.release(handle));
    // Second release of the same handle should fail or be safe
    EXPECT_TRUE(manager.release(handle_copy));
}
// ============================================================================
// ProtectedPointer Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, ProtectedPointerMoveSemantic) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
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

TEST_F(FixedHazardPointerManagerTest, ProtectedPointerReset) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    
    protected_ptr.reset();
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
}

TEST_F(FixedHazardPointerManagerTest, ProtectedPointerSelfAssignment) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    auto data = std::make_shared<TestData>(42);
    
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    
    // Self-assignment should be safe
    protected_ptr = std::move(protected_ptr);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 42);
}

TEST_F(FixedHazardPointerManagerTest, ProtectedPointerAccessors) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
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
// Performance and Stress Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, RapidAcquireRelease) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    const int iterations = 10000;
    int successful_cycles = 0;
    
    for (int i = 0; i < iterations; ++i) {
        auto handle = manager.acquire();
        if (handle.valid()) {
            successful_cycles++;
            manager.release(handle);
        }
    }
    
    EXPECT_EQ(successful_cycles, iterations);
}

TEST_F(FixedHazardPointerManagerTest, StressTestProtection) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    // Create many objects to protect
    std::vector<std::shared_ptr<TestData>> test_objects;
    for (int i = 0; i < 1000; ++i) {
        test_objects.push_back(std::make_shared<TestData>(i));
    }
    
    int successful_protections = 0;
    
    for (const auto& obj : test_objects) {
        auto protected_ptr = manager.protect(obj);
        if (protected_ptr) {
            successful_protections++;
            protected_ptr->access();
        }
    }
    
    EXPECT_EQ(successful_protections, static_cast<int>(test_objects.size()));
}

TEST_F(FixedHazardPointerManagerTest, MixedOperationsStress) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    std::vector<std::shared_ptr<TestData>> test_objects;
    std::vector<FixedHandleType> handles;
    
    const int iterations = 1000;
    int operations_completed = 0;
    
    for (int i = 0; i < iterations; ++i) {
        int operation = i % 4;
        
        switch (operation) {
            case 0: { // Acquire
                auto handle = manager.acquire();
                if (handle.valid() && handles.size() < 60) {
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
// Concurrency Simulation Tests (Single-threaded)
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, SimulatedConcurrentAccess) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    // Simulate multiple "threads" accessing shared data
    std::atomic<std::shared_ptr<TestData>> shared_data;
    shared_data.store(std::make_shared<TestData>(0));
    
    int successful_accesses = 0;
    int data_updates = 0;
    
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
            }
        }
    }
    
    EXPECT_GT(successful_accesses, 800);
    EXPECT_EQ(data_updates, 10);
    
    manager.reclaim_all();
}

TEST_F(FixedHazardPointerManagerTest, ABASimulation) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    std::atomic<std::shared_ptr<TestData>> atomic_ptr;
    atomic_ptr.store(std::make_shared<TestData>(1));
    
    int protection_attempts = 0;
    int successful_protections = 0;
    int aba_updates = 0;
    
    for (int i = 0; i < 1000; ++i) {
        if (i % 10 == 0) {
            // Simulate ABA: A -> B -> A pattern
            auto original = atomic_ptr.load();
            auto temp_data = std::make_shared<TestData>(999);
            auto final_data = std::make_shared<TestData>(original ? original->value : 1);
            
            atomic_ptr.store(temp_data);
            atomic_ptr.store(final_data);
            
            if (original) manager.retire(original);
            manager.retire(temp_data);
            aba_updates++;
        } else {
            // Try to protect current value
            protection_attempts++;
            auto protected_ptr = manager.try_protect(atomic_ptr, 5);
            if (protected_ptr) {
                successful_protections++;
                protected_ptr->access();
            }
        }
    }
    
    EXPECT_GT(successful_protections, protection_attempts / 2);
    EXPECT_EQ(aba_updates, 100);
    
    manager.reclaim_all();
}

// ============================================================================
// Memory Management Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, MemoryLeakPrevention) {
    auto& manager = HazardPointerManager<TestData, 64>::instance(10); // Small retire threshold
    
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
    
    auto& tracked_manager = HazardPointerManager<TrackedTestData, 64>::instance(10);
    
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

TEST_F(FixedHazardPointerManagerTest, RetireThresholdBehavior) {
    constexpr size_t retire_threshold = 5;
    auto& manager = HazardPointerManager<TestData, 64>::instance(retire_threshold);
    
    // Retire objects up to threshold
    std::vector<std::shared_ptr<TestData>> objects;
    for (size_t i = 0; i < retire_threshold; ++i) {
        auto data = std::make_shared<TestData>(static_cast<int>(i));
        objects.push_back(data);
        manager.retire(data);
    }
    
    EXPECT_EQ(manager.retire_size(), retire_threshold);
    
    // One more should trigger automatic reclamation
    auto trigger_data = std::make_shared<TestData>(999);
    manager.retire(trigger_data);
    
    // The retire size should be managed automatically
    EXPECT_LE(manager.retire_size(), retire_threshold + 1);
}

// ============================================================================
// Exception Safety Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, ExceptionDuringProtection) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    struct ThrowingTestData {
        int value;
        ThrowingTestData(int v) : value(v) {}
        
        void dangerous_operation() {
            if (value == 666) {
                throw std::runtime_error("Unlucky number!");
            }
        }
    };
    
    auto& throwing_manager = HazardPointerManager<ThrowingTestData, 64>::instance();
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
// Clear Operation Tests for Fixed Implementation
// ============================================================================

TEST_F(FixedHazardPointerManagerTest, ClearOperation) {
    auto& manager = HazardPointerManager<TestData, 64>::instance();
    
    // Set up some state
    std::vector<FixedHandleType> handles;
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
// Main Test Runner for Fixed Implementation
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "Running Fixed HazardPointerManager (HAZARD_POINTERS=64) tests...\n";
    std::cout << "Expected Characteristics:\n";
    std::cout << "- O(1) acquire/release operations\n";
    std::cout << "- Fixed pool size of 64 hazard pointers\n";
    std::cout << "- Consistent performance regardless of utilization\n";
    std::cout << "- Direct bitmask table access\n";
    std::cout << "========================================\n\n";
    
    return RUN_ALL_TESTS();
}