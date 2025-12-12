#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include "atomic_unique_ptr.hpp"

// Use the HazardSystem namespace
using HazardSystem::atomic_unique_ptr;

// Test fixture for setting up the tests
class AtomicUniquePtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up for each test
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test case 1: Single-threaded basic operations
TEST_F(AtomicUniquePtrTest, SingleThread_BasicOperations) {
    atomic_unique_ptr<int> atomic_ptr;
    
    // Initially, the pointer should be null
    ASSERT_EQ(atomic_ptr.load(), nullptr);
    
    // Reset to a new value
    int* value = new int(42);
    atomic_ptr.reset(value);
    ASSERT_EQ(atomic_ptr.load(), value);
    ASSERT_EQ(*atomic_ptr, 42);

    // Release the pointer and ensure it is no longer owned
    int* released_value = atomic_ptr.release();
    ASSERT_EQ(released_value, value);
    ASSERT_EQ(atomic_ptr.load(), nullptr);

    delete released_value;  // Clean up manually after release

    // Reset again and check
    atomic_ptr.reset(new int(100));
    ASSERT_EQ(*atomic_ptr, 100);

    // Reset with nullptr should delete the existing value
    atomic_ptr.reset(nullptr);
    ASSERT_EQ(atomic_ptr.load(), nullptr);
}

// Test case 2: Single-threaded compare_exchange_strong/weak
TEST_F(AtomicUniquePtrTest, SingleThread_CompareExchangeTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));

    // Expected value is the current pointer value
    int* expected = atomic_ptr.load();
    int* old_ptr  = expected;
    int* new_value = new int(100);

    // Use compare_exchange_strong to swap the pointer
    bool success = atomic_ptr.compare_exchange_strong(expected, new_value);
    ASSERT_TRUE(success);
    ASSERT_EQ(*atomic_ptr, 100);
    ASSERT_EQ(expected, old_ptr);  // Expected now holds the old pointer

    delete expected;  // Clean up old value

    // Use compare_exchange_weak with a different value
    int* weak_expected = atomic_ptr.load();
    int* another_new_value = new int(200);

    success = atomic_ptr.compare_exchange_weak(weak_expected, another_new_value);
    ASSERT_TRUE(success);
    ASSERT_EQ(*atomic_ptr, 200);

    delete weak_expected;  // Clean up old value
}

// Test case 3: Multithreaded reset and release test
TEST_F(AtomicUniquePtrTest, MultiThread_ResetAndReleaseTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(0));

    auto reset_func = [&]() {
        for (int i = 1; i <= 100; ++i) {
            atomic_ptr.reset(new int(i));
        }
    };

    auto release_func = [&]() {
        for (int i = 1; i <= 100; ++i) {
            int* released_ptr = atomic_ptr.release();
            if (released_ptr) {
                delete released_ptr;
            }
        }
    };

    // Run two threads: one resets, the other releases
    std::thread t1(reset_func);
    std::thread t2(release_func);

    t1.join();
    t2.join();

    // Ensure the pointer is null at the end (drain any remaining value)
    int* remaining = atomic_ptr.release();
    if (remaining) {
        delete remaining;
    }
    ASSERT_EQ(atomic_ptr.load(), nullptr);
}

// Test case 4: Multithreaded swap test
TEST_F(AtomicUniquePtrTest, MultiThread_SwapTest) {
    atomic_unique_ptr<int> atomic_ptr1(new int(10));
    atomic_unique_ptr<int> atomic_ptr2(new int(20));

    auto swap_func = [&]() {
        for (int i = 0; i < 50; ++i) {
            atomic_ptr1.swap(atomic_ptr2);
        }
    };

    // Run two threads to swap the pointers concurrently
    std::thread t1(swap_func);
    std::thread t2(swap_func);

    t1.join();
    t2.join();

    // Ensure that the values are swapped correctly and no crashes occurred
    ASSERT_TRUE(((*atomic_ptr1 == 10 && *atomic_ptr2 == 20) || (*atomic_ptr1 == 20 && *atomic_ptr2 == 10)));
}

// Test case 5: Multithreaded compare_exchange_strong stress test
TEST_F(AtomicUniquePtrTest, MultiThread_CompareExchangeStrongTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));
    std::atomic<int> success_count(0);

    auto thread_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            int* expected = atomic_ptr.load();
            int* new_value = new int(i + 100);
            if (atomic_ptr.compare_exchange_strong(expected, new_value)) {
                success_count++;
                delete expected;
            } else {
                delete new_value;
            }
        }
    };

    // Run multiple threads trying to perform compare_exchange_strong
    std::thread t1(thread_func);
    std::thread t2(thread_func);
    std::thread t3(thread_func);

    t1.join();
    t2.join();
    t3.join();

    // Ensure that we have succeeded in swapping at least some values
    ASSERT_GT(success_count.load(), 0);
    ASSERT_NE(atomic_ptr.load(), nullptr);
}

// Test case 14: Protect returns nullptr when empty and valid when set
TEST_F(AtomicUniquePtrTest, ProtectBehavior) {
    atomic_unique_ptr<int> atomic_ptr;
    {
        auto protected_null = atomic_ptr.protect();
        ASSERT_FALSE(protected_null);
    }

    atomic_ptr.reset(new int(7));
    {
        auto protected_ptr = atomic_ptr.protect();
        ASSERT_TRUE(protected_ptr);
        ASSERT_EQ(*protected_ptr, 7);
    }
    int* remaining = atomic_ptr.release();
    delete remaining;
}

// Test case 15: CompareExchangeWeak updates expected on failure
TEST_F(AtomicUniquePtrTest, CompareExchangeWeakUpdatesExpectedOnFailure) {
    atomic_unique_ptr<int> atomic_ptr(new int(1));
    int* wrong = new int(999); // wrong expected
    int* expected = wrong;
    int* desired = new int(2);

    bool success = atomic_ptr.compare_exchange_weak(expected, desired);
    ASSERT_FALSE(success);
    ASSERT_EQ(*atomic_ptr, 1);
    ASSERT_EQ(*expected, 1); // expected updated to current

    delete desired;
    delete wrong;
    int* remaining = atomic_ptr.release();
    delete remaining;
}

// Test case 16: Swap with nullptr destination
TEST_F(AtomicUniquePtrTest, SwapWithNullptr) {
    atomic_unique_ptr<int> atomic_ptr(new int(5));
    atomic_unique_ptr<int> empty_ptr;

    atomic_ptr.swap(empty_ptr);
    ASSERT_EQ(atomic_ptr.load(), nullptr);
    ASSERT_NE(empty_ptr.load(), nullptr);
    ASSERT_EQ(*empty_ptr, 5);

    int* remaining = empty_ptr.release();
    delete remaining;
}

// Test case 17: Release on empty is idempotent
TEST_F(AtomicUniquePtrTest, ReleaseOnEmptyIsIdempotent) {
    atomic_unique_ptr<int> atomic_ptr;
    ASSERT_EQ(atomic_ptr.release(), nullptr);
    ASSERT_EQ(atomic_ptr.release(), nullptr);
}

// Test case 6: Multithreaded transfer test
TEST_F(AtomicUniquePtrTest, MultiThread_TransferTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));
    std::atomic<bool> transferred{false};

    auto thread_func = [&]() {
        // Each thread uses its own destination to avoid races on shared_ptr.
        std::shared_ptr<int> local;
        if (atomic_ptr.transfer(local)) {
            ASSERT_NE(local, nullptr);
            ASSERT_EQ(*local, 42);
            transferred.store(true, std::memory_order_release);
        }
    };

    // Run multiple threads to attempt the transfer
    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();

    ASSERT_TRUE(transferred.load(std::memory_order_acquire));
}

// Test case 7: Stress test with concurrent operations
TEST_F(AtomicUniquePtrTest, MultiThread_StressTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(0));

    auto reset_func = [&]() {
        for (int i = 1; i <= 1000; ++i) {
            atomic_ptr.reset(new int(i));
        }
    };

    auto release_func = [&]() {
        for (int i = 1; i <= 1000; ++i) {
            int* released_ptr = atomic_ptr.release();
            if (released_ptr) {
                delete released_ptr;
            }
        }
    };

    auto swap_func = [&]() {
        atomic_unique_ptr<int> atomic_ptr2(new int(5000));
        for (int i = 1; i <= 500; ++i) {
            atomic_ptr.swap(atomic_ptr2);
        }
    };

    // Run multiple threads performing different operations
    std::thread t1(reset_func);
    std::thread t2(release_func);
    std::thread t3(swap_func);

    t1.join();
    t2.join();
    t3.join();

    // Ensure no crashes and clean up any remaining pointer state
    int* remaining = atomic_ptr.release();
    if (remaining) {
        delete remaining;
    }
    ASSERT_EQ(atomic_ptr.load(), nullptr);
}

// Test case 8: Resetting to nullptr multiple times
TEST_F(AtomicUniquePtrTest, ResetToNullptrTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));

    // Reset to nullptr once
    atomic_ptr.reset(nullptr);
    ASSERT_EQ(atomic_ptr.load(), nullptr);

    // Reset to nullptr again, should remain nullptr
    atomic_ptr.reset(nullptr);
    ASSERT_EQ(atomic_ptr.load(), nullptr);
}

// Test case 9: Double release test
TEST_F(AtomicUniquePtrTest, DoubleReleaseTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));

    // First release should return the pointer
    int* released_value = atomic_ptr.release();
    ASSERT_NE(released_value, nullptr);
    ASSERT_EQ(*released_value, 42);
    ASSERT_EQ(atomic_ptr.load(), nullptr);

    // Second release should return nullptr since it was already released
    ASSERT_EQ(atomic_ptr.release(), nullptr);

    delete released_value;
}

// Test case 10: Compare exchange failure test
TEST_F(AtomicUniquePtrTest, CompareExchangeFailureTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));

    int* wrong_expected = new int(100);  // Incorrect expected value
    int* expected = wrong_expected;
    int* new_value = new int(200);

    // Compare exchange should fail because expected is incorrect
    bool success = atomic_ptr.compare_exchange_strong(expected, new_value);
    ASSERT_FALSE(success);

    // Ensure the original pointer value remains the same
    ASSERT_EQ(*atomic_ptr, 42);

    delete wrong_expected;
    delete new_value;
}

// Test case 11: Move constructor and move assignment operator test
TEST_F(AtomicUniquePtrTest, MoveSemanticsTest) {
    atomic_unique_ptr<int> atomic_ptr1(new int(42));

    // Move construct atomic_ptr2 from atomic_ptr1
    atomic_unique_ptr<int> atomic_ptr2(std::move(atomic_ptr1));

    // atomic_ptr1 should now be null
    ASSERT_EQ(atomic_ptr1.load(), nullptr);

    // atomic_ptr2 should have the original value
    ASSERT_EQ(*atomic_ptr2, 42);

    // Move assign atomic_ptr1 from atomic_ptr2
    atomic_ptr1 = std::move(atomic_ptr2);

    // atomic_ptr2 should now be null
    ASSERT_EQ(atomic_ptr2.load(), nullptr);

    // atomic_ptr1 should have the original value again
    ASSERT_EQ(*atomic_ptr1, 42);
}


// Test case 12: Concurrent compare_exchange_weak test
TEST_F(AtomicUniquePtrTest, ConcurrentCompareExchangeWeakTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));
    std::atomic<int> success_count(0);

    auto thread_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            int* expected = atomic_ptr.load();
            int* new_value = new int(i + 100);
            if (atomic_ptr.compare_exchange_weak(expected, new_value)) {
                success_count++;
                delete expected;
            } else {
                delete new_value;
            }
        }
    };

    // Run multiple threads trying to perform compare_exchange_weak
    std::thread t1(thread_func);
    std::thread t2(thread_func);
    std::thread t3(thread_func);

    t1.join();
    t2.join();
    t3.join();

    // Ensure that we have succeeded in swapping at least some values
    ASSERT_GT(success_count.load(), 0);
    ASSERT_NE(atomic_ptr.load(), nullptr);
}

// Test case 13: Shared ownership transfer failure test
TEST_F(AtomicUniquePtrTest, TransferOwnershipToEmptySharedPtr) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));
    std::shared_ptr<int> null_shared_ptr;

    // Attempt to transfer ownership to an empty shared_ptr
    bool result = atomic_ptr.transfer(null_shared_ptr);

    ASSERT_TRUE(result);
    ASSERT_NE(null_shared_ptr, nullptr);
    ASSERT_EQ(*null_shared_ptr, 42);
    ASSERT_EQ(atomic_ptr.load(), nullptr);
}

TEST_F(AtomicUniquePtrTest, TransferOwnershipFailsWhenDestinationHasValue) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));
    std::shared_ptr<int> target = std::make_shared<int>(99);

    bool result = atomic_ptr.transfer(target);

    ASSERT_FALSE(result);
    ASSERT_NE(target, nullptr);
    ASSERT_EQ(*target, 99);
    ASSERT_NE(atomic_ptr.load(), nullptr);
}

TEST_F(AtomicUniquePtrTest, TransferOwnershipFailsWhenAtomicEmpty) {
    atomic_unique_ptr<int> atomic_ptr(nullptr);
    std::shared_ptr<int> target;

    bool result = atomic_ptr.transfer(target);

    ASSERT_FALSE(result);
    ASSERT_EQ(target, nullptr);
    ASSERT_EQ(atomic_ptr.load(), nullptr);
}
