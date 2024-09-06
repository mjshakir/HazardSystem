#include <gtest/gtest.h>
#include <gmock/gmock.h>
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
    ASSERT_EQ(atomic_ptr.get(), nullptr);
    
    // Reset to a new value
    int* value = new int(42);
    atomic_ptr.reset(value);
    ASSERT_EQ(atomic_ptr.get(), value);
    ASSERT_EQ(*atomic_ptr, 42);

    // Release the pointer and ensure it is no longer owned
    int* released_value = atomic_ptr.release();
    ASSERT_EQ(released_value, value);
    ASSERT_EQ(atomic_ptr.get(), nullptr);

    delete released_value;  // Clean up manually after release

    // Reset again and check
    atomic_ptr.reset(new int(100));
    ASSERT_EQ(*atomic_ptr, 100);

    // Reset with nullptr should delete the existing value
    atomic_ptr.reset(nullptr);
    ASSERT_EQ(atomic_ptr.get(), nullptr);
}

// Test case 2: Single-threaded compare_exchange_strong/weak
TEST_F(AtomicUniquePtrTest, SingleThread_CompareExchangeTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));

    // Expected value is the current pointer value
    int* expected = atomic_ptr.get();
    int* new_value = new int(100);

    // Use compare_exchange_strong to swap the pointer
    bool success = atomic_ptr.compare_exchange_strong(expected, new_value);
    ASSERT_TRUE(success);
    ASSERT_EQ(*atomic_ptr, 100);
    ASSERT_EQ(expected, new_value);  // Expected is now the old pointer

    delete expected;  // Clean up old value

    // Use compare_exchange_weak with a different value
    int* weak_expected = atomic_ptr.get();
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

    // Ensure the pointer is null at the end
    ASSERT_EQ(atomic_ptr.get(), nullptr);
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
            int* expected = atomic_ptr.get();
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
    ASSERT_NE(atomic_ptr.get(), nullptr);
}

// Test case 6: Multithreaded transfer test
TEST_F(AtomicUniquePtrTest, MultiThread_TransferTest) {
    atomic_unique_ptr<int> atomic_ptr(new int(42));
    std::shared_ptr<int> shared_ptr;

    auto thread_func = [&]() {
        // Try to transfer ownership to shared_ptr
        if (atomic_ptr.transfer(shared_ptr)) {
            ASSERT_EQ(*shared_ptr, 42);
        }
    };

    // Run multiple threads to attempt the transfer
    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();

    ASSERT_NE(shared_ptr, nullptr);  // Ensure the shared_ptr is not null
    ASSERT_EQ(*shared_ptr, 42);      // Ensure the value transferred correctly
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

    // Ensure no crashes and the final pointer state is valid
    ASSERT_EQ(atomic_ptr.get(), nullptr);
}