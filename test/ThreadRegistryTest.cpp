#include <gtest/gtest.h>
#include <atomic>
#include <barrier>
#include <thread>
#include <vector>

#include "ThreadRegistry.hpp"

using namespace HazardSystem;

TEST(ThreadRegistryTest, NewThreadIsUnregistered) {
    bool registered_before = true;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister(); // ensure clean slate for the thread id
        registered_before = registry.registered();
    });

    t.join();
    EXPECT_FALSE(registered_before);
}

TEST(ThreadRegistryTest, RegisterAndUnregisterLifecycle) {
    bool registered_after = false;
    bool registered_final = true;
    bool first_unregistered = false;
    bool second_unregistered = true;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister();

        EXPECT_TRUE(registry.register_id());
        registered_after = registry.registered();

        first_unregistered = registry.unregister();
        registered_final = registry.registered();
        second_unregistered = registry.unregister();
    });

    t.join();
    EXPECT_TRUE(registered_after);
    EXPECT_FALSE(registered_final);
    EXPECT_TRUE(first_unregistered);
    EXPECT_FALSE(second_unregistered);
}

TEST(ThreadRegistryTest, IdempotentRegisterCalls) {
    bool first = false;
    bool second = false;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister();

        first = registry.register_id();
        second = registry.register_id(); // should be a no-op and return true
        registry.unregister();
    });

    t.join();
    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
}

TEST(ThreadRegistryTest, ReRegisterAfterUnregister) {
    bool second_registration = false;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister();

        EXPECT_TRUE(registry.register_id());
        EXPECT_TRUE(registry.unregister());

        second_registration = registry.register_id();
        EXPECT_TRUE(registry.registered());
        EXPECT_TRUE(registry.unregister());
        EXPECT_FALSE(registry.registered());
    });

    t.join();
    EXPECT_TRUE(second_registration);
}

TEST(ThreadRegistryTest, ConcurrentRegisterAndUnregister) {
    constexpr size_t thread_count = 32;
    std::barrier sync_point(thread_count);
    std::atomic<size_t> registered_ok{0};
    std::atomic<size_t> unregistered_ok{0};

    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back([&] {
            auto& registry = ThreadRegistry::instance();
            registry.unregister();

            sync_point.arrive_and_wait();

            if (registry.register_id() && registry.registered()) {
                registered_ok.fetch_add(1, std::memory_order_relaxed);
            }

            sync_point.arrive_and_wait();

            if (registry.unregister()) {
                unregistered_ok.fetch_add(1, std::memory_order_relaxed);
            }

            EXPECT_FALSE(registry.registered());
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(registered_ok.load(), thread_count);
    EXPECT_EQ(unregistered_ok.load(), thread_count);
}
