#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "HazardThreadManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

TEST(HazardThreadManagerTest, RegistersThreadOnFirstUse) {
    bool before = true;
    bool after = false;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister();

        before = registry.registered();
        HazardThreadManager::instance();
        after = registry.registered();
    });

    t.join();
    EXPECT_FALSE(before);
    EXPECT_TRUE(after);
}

TEST(HazardThreadManagerTest, ReturnsSameInstanceWithinThread) {
    const HazardThreadManager* first = nullptr;
    const HazardThreadManager* second = nullptr;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister();

        first = &HazardThreadManager::instance();
        second = &HazardThreadManager::instance();
        EXPECT_EQ(first, second);
        EXPECT_TRUE(registry.registered());
    });

    t.join();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
}

TEST(HazardThreadManagerTest, HandlesHighThreadChurnWithoutRegistrationFailures) {
    constexpr size_t total_threads = 1536;
    std::vector<bool> registrations(total_threads, false);

    for (size_t i = 0; i < total_threads; ++i) {
        std::thread worker([&, i] {
            auto& registry = ThreadRegistry::instance();
            registry.unregister();

            HazardThreadManager::instance();
            registrations.at(i) = registry.registered();
        });
        worker.join();
    }

    for (bool ok : registrations) {
        EXPECT_TRUE(ok);
    }
}
