#include <gtest/gtest.h>
#include "HazardRegistry.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <random>

using namespace HazardSystem;

TEST(HazardRegistryTest, AddContainsRemove) {
    HazardRegistry<int> registry(8);
    int a = 1, b = 2;

    EXPECT_TRUE(registry.add(&a));
    EXPECT_TRUE(registry.add(&b));
    EXPECT_TRUE(registry.contains(&a));
    EXPECT_TRUE(registry.contains(&b));

    EXPECT_TRUE(registry.remove(&a));
    EXPECT_FALSE(registry.contains(&a));
    EXPECT_TRUE(registry.contains(&b));
}

TEST(HazardRegistryTest, TombstoneReuse) {
    HazardRegistry<int> registry(4);
    int a = 1, b = 2;

    ASSERT_TRUE(registry.add(&a));
    ASSERT_TRUE(registry.remove(&a));
    EXPECT_FALSE(registry.contains(&a));

    // Slot previously occupied by a should be reusable
    EXPECT_TRUE(registry.add(&b));
    EXPECT_TRUE(registry.contains(&b));
}

TEST(HazardRegistryTest, SnapshotIgnoresTombstones) {
    HazardRegistry<int> registry(4);
    int a = 1, b = 2;

    ASSERT_TRUE(registry.add(&a));
    ASSERT_TRUE(registry.add(&b));
    ASSERT_TRUE(registry.remove(&a));

    auto snap = registry.snapshot();
    // Only b should remain visible
    EXPECT_EQ(snap.size(), 1U);
    EXPECT_EQ(snap.front(), &b);
}

TEST(HazardRegistryTest, CapacityOverflowFails) {
    // Requested 1 => internal capacity bit-ceil((1*4)) = 4 slots
    HazardRegistry<int> registry(1);
    int a = 1, b = 2, c = 3, d = 4, e = 5;

    ASSERT_TRUE(registry.add(&a));
    ASSERT_TRUE(registry.add(&b));
    ASSERT_TRUE(registry.add(&c));
    ASSERT_TRUE(registry.add(&d));
    EXPECT_FALSE(registry.add(&e)); // no space left
}

TEST(HazardRegistryTest, HeavyLoadProbeChains) {
    constexpr size_t cap = 16;
    HazardRegistry<int> registry(cap);
    std::vector<int> items(cap);
    std::vector<int*> ptrs;
    ptrs.reserve(cap);
    for (size_t i = 0; i < cap; ++i) {
        items[i] = static_cast<int>(i);
        ptrs.push_back(&items[i]);
    }
    for (auto* p : ptrs) {
        EXPECT_TRUE(registry.add(p));
    }
    for (auto* p : ptrs) {
        EXPECT_TRUE(registry.contains(p));
    }
}

TEST(HazardRegistryTest, ContentionAddRemove) {
    constexpr size_t cap = 128;
    HazardRegistry<int> registry(cap);
    std::vector<int> items(cap * 2);
    std::atomic<bool> start{false};

    auto worker = [&](size_t idx_start) {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (size_t i = idx_start; i < idx_start + cap; ++i) {
            registry.add(&items[i]);
        }
        for (size_t i = idx_start; i < idx_start + cap; ++i) {
            registry.remove(&items[i]);
        }
    };

    std::thread t1(worker, 0);
    std::thread t2(worker, cap);
    start.store(true, std::memory_order_release);
    t1.join();
    t2.join();

    for (size_t i = 0; i < cap * 2; ++i) {
        EXPECT_FALSE(registry.contains(&items[i]));
    }
}

TEST(HazardRegistryTest, SnapshotStabilityUnderConcurrentWrites) {
    constexpr size_t cap = 128;
    HazardRegistry<int> registry(cap);
    std::vector<int> items(cap);
    for (size_t i = 0; i < cap; ++i) {
        items[i] = static_cast<int>(i);
        registry.add(&items[i]);
    }

    std::atomic<bool> stop{false};
    std::thread modifier([&] {
        for (size_t i = 0; !stop.load(std::memory_order_acquire) && i < cap; ++i) {
            registry.remove(&items[i]);
            registry.add(&items[i]);
        }
    });

    auto snap = registry.snapshot();
    stop.store(true, std::memory_order_release);
    modifier.join();

    // Snapshot must never include tombstones
    for (auto* p : snap) {
        EXPECT_NE(p, nullptr);
    }
}
