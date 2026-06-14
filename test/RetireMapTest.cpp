#include <gtest/gtest.h>
#include <vector>
#include <set>
#include <random>
#include <atomic>
#include <string_view>
#include "RetireMap.hpp" // <-- Adjust path as needed

using HazardSystem::RetireMap;

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
auto always_hazard = std::make_shared<std::function<bool(const Dummy*)>>(
    [](const Dummy*) { return true; });

// Never hazard: always reclaim
auto never_hazard = std::make_shared<std::function<bool(const Dummy*)>>(
    [](const Dummy*) { return false; });

// Hazard even/odd: reclaim half
auto hazard_even = std::make_shared<std::function<bool(const Dummy*)>>(
    [](const Dummy* ptr) { return ptr && (ptr->value % 2 == 0); });

TEST(RetireMapTest, ConstructAndBasicOps) {
    RetireMap<Dummy> s(8, always_hazard);
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.retire(new Dummy(42)));
    EXPECT_EQ(s.size(), 1u);
    s.clear();
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, NullPointerNotInserted) {
    RetireMap<Dummy> s(8, always_hazard);
    EXPECT_FALSE(s.retire(nullptr));
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, DuplicateNotInsertedTwice) {
    RetireMap<Dummy> s(8, always_hazard);
    auto* ptr = new Dummy(5);
    EXPECT_TRUE(s.retire(ptr));
    EXPECT_FALSE(s.retire(ptr)); // duplicate
    EXPECT_EQ(s.size(), 1u);
}

TEST(RetireMapTest, ReclaimRemovesAllIfNoHazard) {
    RetireMap<Dummy> s(8, never_hazard);
    auto* ptr1 = new Dummy(1);
    auto* ptr2 = new Dummy(2);
    s.retire(ptr1);
    s.retire(ptr2);
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 2u);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, ReclaimKeepsHazard) {
    RetireMap<Dummy> s(8, always_hazard);
    auto* ptr1 = new Dummy(1);
    auto* ptr2 = new Dummy(2);
    s.retire(ptr1);
    s.retire(ptr2);
    auto removed = s.reclaim();
    // Hazard function is installed and reclaims nothing: 0 is a valid result,
    // not an error (the previous std::optional API conflated the two).
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 0u);
    EXPECT_EQ(s.size(), 2u);
}

TEST(RetireMapTest, ReclaimRemovesSome) {
    RetireMap<Dummy> s(8, hazard_even);

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

TEST(RetireMapTest, ResizeIncreasesThreshold) {
    RetireMap<Dummy> s(8, always_hazard);
    EXPECT_TRUE(s.resize(128));
    EXPECT_GE(s.size(), 0u);
    for (int i = 0; i < 120; ++i) {
        auto* ptr = new Dummy(i);
        const bool ok = s.retire(ptr).has_value();
        if (!ok) {
            delete ptr;
        }
        EXPECT_TRUE(ok);
    }
    EXPECT_GE(s.size(), 120u);
}

TEST(RetireMapTest, ResizeFailsOnShrink) {
    RetireMap<Dummy> s(8, always_hazard);
    for (int i = 0; i < 16; ++i) {
        auto* ptr = new Dummy(i);
        if (!s.retire(ptr)) {
            delete ptr;
        }
    }
    EXPECT_FALSE(s.resize(4)); // too small
    EXPECT_LE(s.size(), 16u);
}

TEST(RetireMapTest, ReclaimOnEmptyIsNoop) {
    RetireMap<Dummy> s(8, always_hazard);
    auto removed = s.reclaim();
    // Hazard function present, nothing to reclaim: a valid 0, not an error.
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 0u);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, CustomDeleterCalledOnReclaim) {
    std::atomic<int> deleted{0};
    auto never = std::make_shared<std::function<bool(const Dummy*)>>(
        [](const Dummy*) { return false; });
    RetireMap<Dummy> s(4, never);
    auto* ptr = new Dummy(11);
    auto deleter = std::make_shared<std::function<void(Dummy*)>>(
        [&](Dummy* p) { ++deleted; delete p; });
    ASSERT_TRUE(s.retire(ptr, std::move(deleter)));
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 1u);
    EXPECT_EQ(deleted.load(), 1);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, ClearInvokesDeleters) {
    std::atomic<int> deleted{0};
    auto always = std::make_shared<std::function<bool(const Dummy*)>>(
        [](const Dummy*) { return true; });
    RetireMap<Dummy> s(8, always);
    auto deleter = std::make_shared<std::function<void(Dummy*)>>(
        [&](Dummy* p) { ++deleted; delete p; });
    auto local1 = deleter;
    ASSERT_TRUE(s.retire(new Dummy(1), std::move(local1)));
    auto local2 = deleter;
    ASSERT_TRUE(s.retire(new Dummy(2), std::move(local2)));
    EXPECT_EQ(s.size(), 2u);
    s.clear();
    EXPECT_EQ(deleted.load(), 2);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, StressTest10000Pointers) {
    constexpr size_t count = 10000;
    RetireMap<Dummy> s(count, always_hazard);
    auto ptrs = make_ptrs(count);
    for (auto& ptr : ptrs)
        s.retire(ptr);
    EXPECT_EQ(s.size(), count);

    // Now reclaim with never hazard (all should be removed)
    auto ptrs2 = make_ptrs(count);
    s = RetireMap<Dummy>(count, never_hazard);
    for (auto& ptr : ptrs2)
        s.retire(ptr);
    EXPECT_EQ(s.size(), count);
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, count);
    EXPECT_EQ(s.size(), 0u);
}

TEST(RetireMapTest, RealWorldLikeHazardChange) {
    // Simulate pointer retirement, then switch hazard policy and reclaim
    RetireMap<Dummy> s(64, always_hazard); // threshold matches count
    auto ptrs = make_ptrs(64);
    for (auto& ptr : ptrs)
        s.retire(ptr);
    EXPECT_EQ(s.size(), 64u);

    // Swap to never hazard, reclaim all
    auto ptrs2 = make_ptrs(64);
    s = RetireMap<Dummy>(64, never_hazard); // threshold matches count
    for (auto& ptr : ptrs2)
        s.retire(ptr);
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, 64u);
    EXPECT_EQ(s.size(), 0u);
}


TEST(RetireMapTest, RandomHazardFunction) {
    constexpr size_t count = 500;
    constexpr size_t threshold = 32;
    // Hazard: keep if value divisible by 3
    auto hazard_div3 = std::make_shared<std::function<bool(const Dummy*)>>(
        [](const Dummy* ptr) { return ptr && (ptr->value % 3 == 0); });
    RetireMap<Dummy> s(threshold, hazard_div3);
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

TEST(RetireMapTest, SharedDeleterInvokedExactlyOncePerPointer) {
    constexpr size_t count = 8;
    std::atomic<int> deleted{0};
    auto never = std::make_shared<std::function<bool(const Dummy*)>>(
        [](const Dummy*) { return false; });
    RetireMap<Dummy> s(count, never);
    auto deleter = std::make_shared<std::function<void(Dummy*)>>(
        [&](Dummy* p) { ++deleted; delete p; });
    const long base_use_count = deleter.use_count();
    for (size_t i = 0; i < count; ++i) {
        auto local = deleter;
        ASSERT_TRUE(s.retire(new Dummy(static_cast<int>(i)), std::move(local)));
    }
    // The same shared deleter is held by every retired entry; use_count rises by N.
    EXPECT_GE(deleter.use_count(), base_use_count + static_cast<long>(count));
    auto removed = s.reclaim();
    EXPECT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, count);
    EXPECT_EQ(deleted.load(), static_cast<int>(count));
    EXPECT_EQ(s.size(), 0u);
    // After reclaim, only our local handle remains.
    EXPECT_EQ(deleter.use_count(), base_use_count);
}

TEST(RetireMapTest, NullSharedDeleterRejected) {
    auto never = std::make_shared<std::function<bool(const Dummy*)>>(
        [](const Dummy*) { return false; });
    RetireMap<Dummy> s(4, never);
    auto* ptr = new Dummy(7);
    std::shared_ptr<std::function<void(Dummy*)>> null_fn;
    auto rejected = s.retire(ptr, std::move(null_fn));
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error(), HazardSystem::RetireError::NULL_CALLBACK);
    EXPECT_EQ(s.size(), 0u);
    delete ptr; // not owned by the map; clean up to avoid leaking the test pointer
}

// std::expected carries a distinct error per failure mode; assert they are
// not collapsed (the headline win over the old bool / std::optional returns).
TEST(RetireMapTest, ReclaimWithoutHazardFunctionIsError) {
    RetireMap<Dummy> s(8, nullptr); // no hazard predicate installed
    auto* ptr = new Dummy(1);
    ASSERT_TRUE(s.retire(ptr).has_value());
    auto removed = s.reclaim();
    ASSERT_FALSE(removed.has_value());
    EXPECT_EQ(removed.error(), HazardSystem::RetireError::NO_HAZARD_FUNCTION);
    s.clear(); // drops the retained pointer through its Deleter
}

TEST(RetireMapTest, RetireErrorVariantsAreDistinct) {
    RetireMap<Dummy> s(8, always_hazard);
    EXPECT_EQ(s.retire(nullptr).error(), HazardSystem::RetireError::NULL_POINTER);
    auto* ptr = new Dummy(3);
    EXPECT_TRUE(s.retire(ptr).has_value());
    EXPECT_EQ(s.retire(ptr).error(), HazardSystem::RetireError::DUPLICATE);
}

// to_string returns the enumerator's name as std::optional<string_view>, and
// std::nullopt for a value that is not a recognised enumerator.
TEST(RetireMapTest, ErrorToStringNames) {
    using HazardSystem::RetireError;
    using HazardSystem::to_string;
    ASSERT_TRUE(to_string(RetireError::NULL_POINTER).has_value());
    EXPECT_EQ(to_string(RetireError::NULL_POINTER).value(), "NULL_POINTER");
    EXPECT_NE(to_string(RetireError::NULL_POINTER), to_string(RetireError::DUPLICATE));
    // A value that is not a recognised enumerator has no name.
    EXPECT_FALSE(to_string(static_cast<RetireError>(0)).has_value());
}
// Usable in constant expressions (constexpr, not consteval).
static_assert(HazardSystem::to_string(HazardSystem::RetireError::DUPLICATE).has_value(),
              "known RetireError values must resolve to a name");

// Locks in the size win from replacing std::function<void(T*)> with a shared_ptr inside the variant.
static_assert(sizeof(HazardSystem::Deleter<int>) <= 32,
              "Deleter footprint regressed past the 32-byte budget");
