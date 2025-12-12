#include <gtest/gtest.h>
#include <atomic>
#include <memory>

#include "ProtectedPointer.hpp"

using namespace HazardSystem;

struct PPDummy {
    int value{1};
    void touch() { ++value; }
};

TEST(ProtectedPointerTest, MoveClearsSourceAndReleasesOnce) {
    std::atomic<int> releases{0};
    auto* raw = new PPDummy();
    {
        ProtectedPointer<PPDummy> p1(
            raw,
            [&]() {
                ++releases;
                delete raw;
                return true;
            });
        ProtectedPointer<PPDummy> p2(std::move(p1));
        EXPECT_FALSE(static_cast<bool>(p1));
        EXPECT_TRUE(static_cast<bool>(p2));
        p2.reset();
    }
    EXPECT_EQ(releases.load(), 1);
}

TEST(ProtectedPointerTest, OwnerKeepsObjectAlive) {
    std::shared_ptr<PPDummy> owner = std::make_shared<PPDummy>();
    std::weak_ptr<PPDummy> weak = owner;
    {
        ProtectedPointer<PPDummy> guard(
            owner.get(),
            []() { return true; },
            owner);
        owner.reset();
        ASSERT_FALSE(owner);
        auto locked = weak.lock();
        EXPECT_TRUE(locked);
        locked->touch();
        EXPECT_GT(locked->value, 1);
    }
    EXPECT_TRUE(weak.expired());
}

TEST(ProtectedPointerTest, MoveAssignmentReleasesExistingAndClearsSource) {
    std::atomic<int> releases{0};
    auto* first = new PPDummy();
    auto* second = new PPDummy();
    ProtectedPointer<PPDummy> p1(
        first,
        [&]() {
            ++releases;
            delete first;
            return true;
        });
    ProtectedPointer<PPDummy> p2(
        second,
        [&]() {
            ++releases;
            delete second;
            return true;
        });

    p1 = std::move(p2);
    EXPECT_EQ(releases.load(), 1);
    EXPECT_FALSE(static_cast<bool>(p2));
    EXPECT_TRUE(static_cast<bool>(p1));
    EXPECT_EQ(p1->value, 1);

    p1.reset();
    EXPECT_EQ(releases.load(), 2);
}

TEST(ProtectedPointerTest, MoveFromEmptyClearsTarget) {
    std::atomic<int> releases{0};
    auto* raw = new PPDummy();
    ProtectedPointer<PPDummy> target(
        raw,
        [&]() {
            ++releases;
            delete raw;
            return true;
        });
    ProtectedPointer<PPDummy> empty;

    target = std::move(empty);
    EXPECT_EQ(releases.load(), 1);
    EXPECT_FALSE(static_cast<bool>(target));
    EXPECT_FALSE(static_cast<bool>(empty));
}

TEST(ProtectedPointerTest, ResetIsIdempotent) {
    std::atomic<int> releases{0};
    auto* raw = new PPDummy();
    ProtectedPointer<PPDummy> guard(
        raw,
        [&]() {
            ++releases;
            delete raw;
            return true;
        });

    EXPECT_TRUE(guard.reset());
    EXPECT_FALSE(guard.reset());
    EXPECT_EQ(releases.load(), 1);
    EXPECT_FALSE(static_cast<bool>(guard));
}

TEST(ProtectedPointerTest, SharedPtrWithoutOwnerIsNonOwning) {
    std::atomic<int> releases{0};
    auto* raw = new PPDummy();
    {
        ProtectedPointer<PPDummy> guard(
            raw,
            [&]() {
                ++releases;
                delete raw;
                return true;
            });
        auto non_owning = guard.shared_ptr();
        EXPECT_EQ(non_owning.get(), raw);
        EXPECT_EQ(non_owning.use_count(), 1);
        non_owning.reset();
        EXPECT_EQ(releases.load(), 0);
        EXPECT_TRUE(static_cast<bool>(guard));
        EXPECT_EQ(guard.get(), raw);
        guard.reset();
    }
    EXPECT_EQ(releases.load(), 1);
}
