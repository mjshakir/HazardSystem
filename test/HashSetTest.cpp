#include <gtest/gtest.h>
#include <set>
#include <thread>
#include <vector>

#include "HashSet.hpp"

using namespace HazardSystem;

class HashSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        set = std::make_unique<HashSet<int>>(128);
    }

    std::unique_ptr<HashSet<int>> set;
};

TEST_F(HashSetTest, InsertAndContains) {
    EXPECT_TRUE(set->insert(42));
    EXPECT_TRUE(set->contains(42));
    EXPECT_FALSE(set->contains(7));
    EXPECT_EQ(set->size(), 1U);
}

TEST_F(HashSetTest, DuplicateInsertIsRejected) {
    EXPECT_TRUE(set->insert(5));
    EXPECT_FALSE(set->insert(5));
    EXPECT_EQ(set->size(), 1U);
}

TEST_F(HashSetTest, RemoveElement) {
    EXPECT_TRUE(set->insert(10));
    EXPECT_TRUE(set->remove(10));
    EXPECT_FALSE(set->contains(10));
    EXPECT_FALSE(set->remove(10));
    EXPECT_EQ(set->size(), 0U);
}

TEST_F(HashSetTest, FillAndTraverse) {
    constexpr int count = 50;
    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(set->insert(i));
    }
    std::set<int> found;
    set->for_each_fast([&](int v) { found.insert(v); });
    EXPECT_EQ(found.size(), count);
    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(found.count(i));
    }
}

TEST_F(HashSetTest, ReclaimRemovesNonHazards) {
    EXPECT_TRUE(set->insert(1));
    EXPECT_TRUE(set->insert(2));
    EXPECT_TRUE(set->insert(3));

    set->reclaim([](int value) { return value % 2 == 1; }); // keep odds

    EXPECT_TRUE(set->contains(1));
    EXPECT_TRUE(set->contains(3));
    EXPECT_FALSE(set->contains(2));
}

TEST_F(HashSetTest, ClearResetsTheTable) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(set->insert(i));
    }
    set->clear();
    EXPECT_EQ(set->size(), 0U);
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(set->contains(i));
    }
}

TEST_F(HashSetTest, ConcurrentInsertsAndRemoves) {
    constexpr int threads = 8;
    constexpr int items_per_thread = 32;
    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&, offset = t * items_per_thread] {
            for (int i = 0; i < items_per_thread; ++i) {
                set->insert(offset + i);
            }
            for (int i = 0; i < items_per_thread; ++i) {
                set->remove(offset + i);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    EXPECT_EQ(set->size(), 0U);
}
