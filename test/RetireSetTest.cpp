#include <gtest/gtest.h>
#include <vector>
#include <set>
#include <random>
#include "RetireSet.hpp" // <-- Adjust path as needed

using HazardSystem::RetireSet;

struct Dummy {
    int value;
    explicit Dummy(int v) : value(v) {}
};

// Helper for unique raw pointers for stress
std::vector<Dummy*> make_ptrs(const size_t& n) {
    std::vector<Dummy*> v;
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        v.emplace_back(new Dummy(static_cast<int>(i)));
    }// end for (size_t i = 0; i < n; ++i)
    return v;
}

// Always hazard: never reclaim
auto always_hazard = [](const Dummy*) { return true; };

// Never hazard: always reclaim
auto never_hazard = [](const Dummy*) { return false; };

// Hazard even/odd: reclaim half
auto hazard_even = [](const Dummy* ptr) {
    return ptr && (ptr->value % 2 == 0);
};

auto hazard_mod3 = [](const Dummy* ptr) {
    return ptr && (ptr->value % 3 == 0);
};

TEST(RetireSetTest, ConstructAndBasicOps) {
    RetireSet<Dummy> s(8, always_hazard);
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.retire(new Dummy(42)));
    EXPECT_EQ(s.size(), 1u);
    s.clear();
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireSetTest, NullPointerNotInserted) {
    RetireSet<Dummy> s(8, always_hazard);
    EXPECT_FALSE(s.retire(nullptr));
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireSetTest, DuplicateNotInsertedTwice) {
    RetireSet<Dummy> s(8, always_hazard);
    auto* ptr = new Dummy(5);
    EXPECT_TRUE(s.retire(ptr));
    EXPECT_FALSE(s.retire(ptr)); // duplicate
    EXPECT_EQ(s.size(), 1u);
}

TEST(RetireSetTest, ReclaimRemovesAllIfNoHazard) {
    RetireSet<Dummy> s(8, never_hazard);
    auto* ptr1 = new Dummy(1);
    auto* ptr2 = new Dummy(2);
    s.retire(ptr1);
    s.retire(ptr2);
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 2u);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireSetTest, ReclaimKeepsHazard) {
    RetireSet<Dummy> s(8, always_hazard);
    auto* ptr1 = new Dummy(1);
    auto* ptr2 = new Dummy(2);
    s.retire(ptr1);
    s.retire(ptr2);
    auto removed = s.reclaim();
    EXPECT_FALSE(removed.has_value());
    EXPECT_EQ(s.size(), 2u);
}

TEST(RetireSetTest, ReclaimRemovesSome) {
    RetireSet<Dummy> s(8, hazard_even);

    auto* ptr1 = new Dummy(1);
    auto* ptr2 = new Dummy(2);
    auto* ptr3 = new Dummy(3);
    auto* ptr4 = new Dummy(4);

    EXPECT_TRUE(s.retire(ptr1));
    EXPECT_TRUE(s.retire(ptr2));
    EXPECT_TRUE(s.retire(ptr3));
    EXPECT_TRUE(s.retire(ptr4));

    auto removed = s.reclaim();
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 2u); // 1 and 3 (odd) should be removed
    EXPECT_EQ(s.size(), 2u);

    // The set should still contain ptr2 and ptr4 (evens).
    // Retiring the *same* pointers again should return false (no duplicate allowed).
    EXPECT_FALSE(s.retire(ptr2)); // Already present
    EXPECT_FALSE(s.retire(ptr4)); // Already present
    EXPECT_EQ(s.size(), 2u);

    // Retiring a new even pointer should return true and increase size.
    auto* ptr6 = new Dummy(6);
    EXPECT_TRUE(s.retire(ptr6));
    EXPECT_EQ(s.size(), 3u);

    // Retiring nullptr should return false, not increase size
    Dummy* null_ptr = nullptr;
    EXPECT_FALSE(s.retire(null_ptr));
    EXPECT_EQ(s.size(), 3u);
}

TEST(RetireSetTest, ResizeIncreasesThreshold) {
    RetireSet<Dummy> s(8, always_hazard);
    EXPECT_TRUE(s.resize(128));
    EXPECT_GE(s.size(), 0u);
    for (int i = 0; i < 120; ++i) {
        EXPECT_TRUE(s.retire(new Dummy(i)));
    }
    EXPECT_GE(s.size(), 120u);
}

TEST(RetireSetTest, ResizeFailsOnShrink) {
    RetireSet<Dummy> s(8, always_hazard);
    for (int i = 0; i < 16; ++i)
        s.retire(new Dummy(i));
    EXPECT_FALSE(s.resize(4)); // too small
    EXPECT_LE(s.size(), 16u);
}

TEST(RetireSetTest, ReclaimOnEmptyIsNoop) {
    RetireSet<Dummy> s(8, always_hazard);
    auto removed = s.reclaim();
    EXPECT_FALSE(removed.has_value());
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireSetTest, StressTest10000Pointers) {
    constexpr size_t count = 10000;
    RetireSet<Dummy> s(count, always_hazard);
    auto ptrs = make_ptrs(count);
    for (auto& ptr : ptrs)
        s.retire(ptr);
    EXPECT_EQ(s.size(), count);

    // Now reclaim with never hazard (all should be removed)
    auto ptrs2 = make_ptrs(count);
    s = RetireSet<Dummy>(count, never_hazard);
    for (auto& ptr : ptrs2)
        s.retire(ptr);
    EXPECT_EQ(s.size(), count);
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, count);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireSetTest, RealWorldLikeHazardChange) {
    // Simulate pointer retirement, then switch hazard policy and reclaim
    RetireSet<Dummy> s(64, always_hazard); // threshold matches count
    auto ptrs = make_ptrs(64);
    for (auto& ptr : ptrs)
        s.retire(ptr);
    EXPECT_EQ(s.size(), 64u);

    // Swap to never hazard, reclaim all
    auto ptrs2 = make_ptrs(64);
    s = RetireSet<Dummy>(64, never_hazard); // threshold matches count
    for (auto& ptr : ptrs2)
        s.retire(ptr);
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 64u);
    EXPECT_EQ(s.size(), 0u);
}


TEST(RetireSetTest, RandomHazardFunction) {
    constexpr size_t count = 500;
    constexpr size_t threshold = 32;
    // Hazard: keep if value divisible by 3
    RetireSet<Dummy> s(threshold, [](const Dummy* ptr) {
        return ptr && (ptr->value % 3 == 0);
    });
    auto ptrs = make_ptrs(count);
    size_t expected_survivors = 0;
    std::vector<Dummy*> survivors;
    for (auto ptr : ptrs) {
        if (ptr->value % 3 == 0) {
            ++expected_survivors;
            survivors.push_back(ptr);
        }// end if (ptr->value % 3 == 0)
        s.retire(ptr);
    }

    // Trigger reclaim
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());

    EXPECT_EQ(s.size(), expected_survivors);

    // Try to retire all survivors again (should not insert duplicates)
    for (const auto& ptr : survivors) {
        EXPECT_FALSE(s.retire(ptr)); // Already present!
    }
    EXPECT_EQ(s.size(), expected_survivors);
}
