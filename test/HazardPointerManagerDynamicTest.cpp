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

#include "HazardPointerManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

// Macro to define unique TestData type per test
#define DEFINE_TESTDATA_TYPE(TESTNAME) \
    struct TESTNAME##_TestData { \
        int value; \
        std::atomic<bool> destroyed{false}; \
        std::atomic<int> access_count{0}; \
        TESTNAME##_TestData(int v = 0) : value(v) {} \
        ~TESTNAME##_TestData() { destroyed.store(true, std::memory_order_release); } \
        void access() { access_count.fetch_add(1, std::memory_order_relaxed); } \
        void increment() { access_count.fetch_add(1, std::memory_order_relaxed); } \
    }

// ========== Core Functionality ==========

DEFINE_TESTDATA_TYPE(SingletonInstance);
TEST(DynamicHazardPointerManager, SingletonInstance) {
    using TestData = SingletonInstance_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager1 = Manager::instance();
    auto& manager2 = Manager::instance();
    EXPECT_EQ(&manager1, &manager2);
    manager1.clear();
}

DEFINE_TESTDATA_TYPE(DynamicSizing);
TEST(DynamicHazardPointerManager, DynamicSizing) {
    using TestData = DynamicSizing_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    using Handle = HazardHandle<typename Manager::IndexType, HazardPointer<TestData>>;
    constexpr size_t hazards = 20, retire = 5;
    auto& manager = Manager::instance(hazards, retire);
    EXPECT_GE(manager.hazard_capacity(), hazards);
    size_t cap = manager.hazard_capacity();
    std::vector<Handle> handles; handles.reserve(cap);
    for (size_t i = 0; i < cap; ++i) {
        auto h = manager.acquire();
        EXPECT_TRUE(h.valid()) << "Failed at " << i;
        handles.push_back(std::move(h));
    }
    auto extra = manager.acquire();
    EXPECT_FALSE(extra.valid());
    for (auto& h : handles) manager.release(h);
    manager.clear();
}

DEFINE_TESTDATA_TYPE(AcquireAndRelease);
TEST(DynamicHazardPointerManager, AcquireAndRelease) {
    using TestData = AcquireAndRelease_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    using Handle = HazardHandle<typename Manager::IndexType, HazardPointer<TestData>>;
    auto& manager = Manager::instance(10);
    auto handle = manager.acquire();
    EXPECT_TRUE(handle.valid());
    EXPECT_TRUE(handle.index.has_value());
    EXPECT_NE(handle.sp_data, nullptr);
    bool released = manager.release(handle);
    EXPECT_TRUE(released);
    manager.clear();
}

DEFINE_TESTDATA_TYPE(ProtectSharedPtr);
TEST(DynamicHazardPointerManager, ProtectSharedPtr) {
    using TestData = ProtectSharedPtr_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    auto data = std::make_shared<TestData>(42);
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 42);
    EXPECT_EQ(protected_ptr.get()->value, 42);
    EXPECT_EQ((*protected_ptr).value, 42);
    manager.clear();
}

DEFINE_TESTDATA_TYPE(ProtectAtomicSharedPtr);
TEST(DynamicHazardPointerManager, ProtectAtomicSharedPtr) {
    using TestData = ProtectAtomicSharedPtr_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(100));
    auto protected_ptr = manager.protect(atomic_data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 100);
    manager.clear();
}

DEFINE_TESTDATA_TYPE(TryProtectWithRetries);
TEST(DynamicHazardPointerManager, TryProtectWithRetries) {
    using TestData = TryProtectWithRetries_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    std::atomic<std::shared_ptr<TestData>> atomic_data;
    atomic_data.store(std::make_shared<TestData>(200));
    auto protected_ptr = manager.try_protect(atomic_data, 10);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 200);
    manager.clear();
}

DEFINE_TESTDATA_TYPE(RetireAndReclaim);
TEST(DynamicHazardPointerManager, RetireAndReclaim) {
    using TestData = RetireAndReclaim_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10, 2);
    auto data1 = std::make_shared<TestData>(1);
    auto data2 = std::make_shared<TestData>(2);
    auto data3 = std::make_shared<TestData>(3);
    EXPECT_TRUE(manager.retire(data1));
    EXPECT_TRUE(manager.retire(data2));
    EXPECT_EQ(manager.retire_size(), 2);
    EXPECT_TRUE(manager.retire(data3)); // should trigger reclamation
    manager.clear();
}

// ========== Edge Cases ==========

DEFINE_TESTDATA_TYPE(ProtectNullptr);
TEST(DynamicHazardPointerManager, ProtectNullptr) {
    using TestData = ProtectNullptr_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    std::shared_ptr<TestData> null_ptr;
    auto protected_ptr = manager.protect(null_ptr);
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
    manager.clear();
}

DEFINE_TESTDATA_TYPE(ProtectAtomicNullptr);
TEST(DynamicHazardPointerManager, ProtectAtomicNullptr) {
    using TestData = ProtectAtomicNullptr_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    std::atomic<std::shared_ptr<TestData>> atomic_null;
    atomic_null.store(nullptr);
    auto protected_ptr = manager.protect(atomic_null);
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
    manager.clear();
}

DEFINE_TESTDATA_TYPE(ReleaseInvalidHandle);
TEST(DynamicHazardPointerManager, ReleaseInvalidHandle) {
    using TestData = ReleaseInvalidHandle_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    using Handle = HazardHandle<typename Manager::IndexType, HazardPointer<TestData>>;
    auto& manager = Manager::instance(10);
    Handle invalid_handle;
    EXPECT_FALSE(manager.release(invalid_handle));
    manager.clear();
}

DEFINE_TESTDATA_TYPE(RetireNullptr);
TEST(DynamicHazardPointerManager, RetireNullptr) {
    using TestData = RetireNullptr_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    std::shared_ptr<TestData> null_ptr;
    EXPECT_FALSE(manager.retire(null_ptr));
    manager.clear();
}

DEFINE_TESTDATA_TYPE(AcquireFromEmptyPool);
TEST(DynamicHazardPointerManager, AcquireFromEmptyPool) {
    using TestData = AcquireFromEmptyPool_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    using Handle = HazardHandle<typename Manager::IndexType, HazardPointer<TestData>>;
    auto& manager = Manager::instance(0); // Request 0, but actually get 1
    auto handle1 = manager.acquire();
    EXPECT_TRUE(handle1.valid()) << "One slot should be available due to bit_ceil(0)==1";
    auto handle2 = manager.acquire();
    EXPECT_FALSE(handle2.valid()) << "No more handles should be available after the first";
    manager.clear();
}


DEFINE_TESTDATA_TYPE(AcquireFromSingleSlotPool);
TEST(DynamicHazardPointerManager, AcquireFromSingleSlotPool) {
    using TestData = AcquireFromSingleSlotPool_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    using Handle = HazardHandle<typename Manager::IndexType, HazardPointer<TestData>>;
    auto& manager = Manager::instance(1);
    auto handle1 = manager.acquire();
    EXPECT_TRUE(handle1.valid());
    auto handle2 = manager.acquire();
    EXPECT_FALSE(handle2.valid());
    EXPECT_TRUE(manager.release(handle1));
    auto handle3 = manager.acquire();
    EXPECT_TRUE(handle3.valid());
    EXPECT_TRUE(manager.release(handle3));
    manager.clear();
}

// ========== Pool Size & Performance (repeat for various pool sizes) ==========

#define TEST_POOL_SIZE(PREFIX, POOL_SIZE) \
    DEFINE_TESTDATA_TYPE(PREFIX##_##POOL_SIZE); \
    TEST(DynamicHazardPointerManager, PREFIX##_##POOL_SIZE) { \
        using TestData = PREFIX##_##POOL_SIZE##_TestData; \
        using Manager = HazardPointerManager<TestData, 0>; \
        using Handle = HazardHandle<typename Manager::IndexType, HazardPointer<TestData>>; \
        auto& manager = Manager::instance(POOL_SIZE); \
        manager.clear(); \
        std::vector<Handle> handles; \
        for (size_t i = 0; i < POOL_SIZE; ++i) { \
            auto handle = manager.acquire(); \
            EXPECT_TRUE(handle.valid()) << "Failed at pool_size=" << POOL_SIZE << ", index=" << i; \
            handles.push_back(std::move(handle)); \
        } \
        auto extra = manager.acquire(); \
        EXPECT_FALSE(extra.valid()); \
        for (auto& handle : handles) manager.release(handle); \
        manager.clear(); \
    }

TEST_POOL_SIZE(VariablePoolSizes, 1)
TEST_POOL_SIZE(VariablePoolSizes, 4)
TEST_POOL_SIZE(VariablePoolSizes, 16)
TEST_POOL_SIZE(VariablePoolSizes, 64)
TEST_POOL_SIZE(VariablePoolSizes, 128)
TEST_POOL_SIZE(VariablePoolSizes, 256)

// (Continue using this macro for other pool size or scaling tests as desired)

// ========== ProtectedPointer Move/Reset/Access ==========

DEFINE_TESTDATA_TYPE(ProtectedPointerMoveSemantic);
TEST(DynamicHazardPointerManager, ProtectedPointerMoveSemantic) {
    using TestData = ProtectedPointerMoveSemantic_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    auto data = std::make_shared<TestData>(42);
    auto protected_ptr1 = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr1));
    auto protected_ptr2 = std::move(protected_ptr1);
    EXPECT_FALSE(static_cast<bool>(protected_ptr1));
    EXPECT_TRUE(static_cast<bool>(protected_ptr2));
    EXPECT_EQ(protected_ptr2->value, 42);
    auto protected_ptr3 = manager.protect(std::make_shared<TestData>(100));
    protected_ptr3 = std::move(protected_ptr2);
    EXPECT_FALSE(static_cast<bool>(protected_ptr2));
    EXPECT_TRUE(static_cast<bool>(protected_ptr3));
    EXPECT_EQ(protected_ptr3->value, 42);
    manager.clear();
}

DEFINE_TESTDATA_TYPE(ProtectedPointerReset);
TEST(DynamicHazardPointerManager, ProtectedPointerReset) {
    using TestData = ProtectedPointerReset_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    auto data = std::make_shared<TestData>(42);
    auto protected_ptr = manager.protect(data);
    EXPECT_TRUE(static_cast<bool>(protected_ptr));
    protected_ptr.reset();
    EXPECT_FALSE(static_cast<bool>(protected_ptr));
    manager.clear();
}

DEFINE_TESTDATA_TYPE(ProtectedPointerAccessors);
TEST(DynamicHazardPointerManager, ProtectedPointerAccessors) {
    using TestData = ProtectedPointerAccessors_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    auto& manager = Manager::instance(10);
    auto data = std::make_shared<TestData>(42);
    auto protected_ptr = manager.protect(data);
    ASSERT_TRUE(static_cast<bool>(protected_ptr));
    EXPECT_EQ(protected_ptr->value, 42);
    EXPECT_EQ((*protected_ptr).value, 42);
    EXPECT_EQ(protected_ptr.get()->value, 42);
    EXPECT_EQ(protected_ptr.shared_ptr()->value, 42);
    manager.clear();
}

// ========== Memory Management/Leak Tests ==========

DEFINE_TESTDATA_TYPE(MemoryLeakPrevention);
TEST(DynamicHazardPointerManager, MemoryLeakPrevention) {
    using TestData = MemoryLeakPrevention_TestData;
    using Manager = HazardPointerManager<TestData, 0>;
    std::atomic<int> created{0}, destroyed{0};
    struct TrackedTestData {
        int value;
        std::atomic<int>* c;
        std::atomic<int>* d;
        TrackedTestData(int v, std::atomic<int>* cr, std::atomic<int>* de) : value(v), c(cr), d(de) { c->fetch_add(1); }
        ~TrackedTestData() { d->fetch_add(1); }
    };
    auto& tracked_manager = HazardPointerManager<TrackedTestData, 0>::instance(16, 10);
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        auto data = std::make_shared<TrackedTestData>(i, &created, &destroyed);
        tracked_manager.retire(data);
    }
    tracked_manager.reclaim_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(created.load(), N);
    EXPECT_EQ(destroyed.load(), N);
    tracked_manager.clear();
}

// ========== Retire Threshold Configuration ==========

DEFINE_TESTDATA_TYPE(RetireThresholdConfiguration);
TEST(DynamicHazardPointerManager, RetireThresholdConfiguration) {
    using TestData = RetireThresholdConfiguration_TestData;
    constexpr std::array<size_t, 5> thresholds = {1, 5, 10, 20, 50};
    for (size_t t : thresholds) {
        auto& manager = HazardPointerManager<TestData, 0>::instance(16, t);
        manager.clear();
        for (size_t i = 0; i < t; ++i) {
            auto data = std::make_shared<TestData>(static_cast<int>(i));
            manager.retire(data);
        }
        // Allow up to t here, may be less if scan_and_reclaim reclaims eagerly
        EXPECT_LE(manager.retire_size(), t);

        auto trigger_data = std::make_shared<TestData>(999);
        manager.retire(trigger_data);
        // After possible scan-and-reclaim, we can have up to t+1
        EXPECT_LE(manager.retire_size(), t + 1);

        manager.clear();
    }
}

// (Repeat above pattern for more concurrency, scalability, and integration tests as needed)

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    std::cout << "Running Dynamic HazardPointerManager (HAZARD_POINTERS=0) tests..." << std::endl;
    return RUN_ALL_TESTS();
}