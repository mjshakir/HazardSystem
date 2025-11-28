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
