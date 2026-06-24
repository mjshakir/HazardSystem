#include <gtest/gtest.h>
#include <atomic>
#include <barrier>
#include <cstddef>
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

// Registration is per-thread: thread A being registered must never make a
// different thread B (which never registered) appear registered.
TEST(ThreadRegistryTest, PerThreadIsolation) {
    std::barrier sync_point(2);
    std::atomic<bool> a_registered{false};
    std::atomic<bool> b_sees_registered{true};

    std::thread a([&] {
        auto& registry = ThreadRegistry::instance();
        EXPECT_TRUE(registry.register_id());
        a_registered = registry.registered();

        sync_point.arrive_and_wait(); // A is registered here
        sync_point.arrive_and_wait(); // hold registration until B has checked

        registry.unregister();
    });

    std::thread b([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister(); // clean slate; B never registers

        sync_point.arrive_and_wait();       // wait until A is registered
        b_sees_registered = registry.registered();
        sync_point.arrive_and_wait();        // release A
    });

    a.join();
    b.join();

    EXPECT_TRUE(a_registered.load());
    EXPECT_FALSE(b_sees_registered.load()); // B is unaffected by A's registration
}

// unregister() on a thread that never registered must return false (no crash).
TEST(ThreadRegistryTest, UnregisterWithoutRegisterReturnsFalse) {
    bool first_unregister = true;
    bool registered_state = true;

    std::thread t([&] {
        auto& registry = ThreadRegistry::instance();
        registry.unregister();                 // ensure clean slate
        first_unregister = registry.unregister(); // already unregistered
        registered_state = registry.registered();
    });

    t.join();
    EXPECT_FALSE(first_unregister);
    EXPECT_FALSE(registered_state);
}

// ThreadRegistry is a per-thread instance whose constructor registers the
// thread, so the FIRST touch auto-registers. Verify that, and that registration
// is independent per thread across many sequential threads: a reused
// std::thread::id cannot leak registration, since each thread gets its own
// thread_local instance and never observes a prior thread's state.
TEST(ThreadRegistryTest, RegistersOnFirstTouchPerThread) {
    constexpr size_t thread_count = 256;

    for (size_t i = 0; i < thread_count; ++i) {
        bool registered_on_touch   = false;
        bool unregistered_was_true = false;
        bool registered_after      = true;

        std::thread t([&] {
            auto& registry = ThreadRegistry::instance(); // first touch auto-registers
            registered_on_touch   = registry.registered();
            unregistered_was_true = registry.unregister();
            registered_after      = registry.registered();
            // Deliberately leave the thread unregistered so the next sequential
            // thread may reuse this id; it must still start from its own state.
        });

        t.join();
        EXPECT_TRUE(registered_on_touch)   << "iteration " << i;
        EXPECT_TRUE(unregistered_was_true) << "iteration " << i;
        EXPECT_FALSE(registered_after)     << "iteration " << i;
    }
}

// register_id() must never fail, regardless of how many threads register
// simultaneously (per-thread state has no shared capacity to exhaust).
TEST(ThreadRegistryTest, HighConcurrencyRegisterNeverFails) {
    constexpr size_t thread_count = 1200;
    std::barrier sync_point(thread_count);
    std::atomic<size_t> register_ok{0};
    std::atomic<size_t> registered_ok{0};

    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back([&] {
            auto& registry = ThreadRegistry::instance();
            registry.unregister();

            sync_point.arrive_and_wait();

            if (registry.register_id()) {
                register_ok.fetch_add(1, std::memory_order_relaxed);
            }

            // All threads are registered at the same time here.
            sync_point.arrive_and_wait();

            if (registry.registered()) {
                registered_ok.fetch_add(1, std::memory_order_relaxed);
            }

            registry.unregister();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(register_ok.load(), thread_count);
    EXPECT_EQ(registered_ok.load(), thread_count);
}
