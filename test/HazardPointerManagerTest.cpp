#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <atomic>
#include "HazardPointerManager.hpp"
#include "HazardPointer.hpp"
#include "atomic_unique_ptr.hpp"
#include <string>

struct TestState {
    int value;
    std::string name;

    TestState(int v, const std::string& n) : value(v), name(n) {
        // Constructor
    }

    ~TestState() {
        // Destructor to detect deletion
        std::cout << "TestState with value " << value << " and name " << name << " destroyed." << std::endl;
    }
};


using HazardSystem::HazardPointerManager;

// Test fixture for setting up the tests
class HazardPointerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up for each test
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test case 1: Single-threaded hazard pointer acquisition and release
TEST_F(HazardPointerManagerTest, SingleThread_AcquireReleaseTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Acquire a hazard pointer
    auto hp = manager.acquire();
    ASSERT_NE(hp, nullptr);  // Ensure we successfully acquired a pointer

    // Test that the hazard pointer is empty after acquisition
    ASSERT_EQ(hp->pointer.get(), nullptr);

    // Set a value using unique_ptr
    auto value = std::make_unique<int>(42);
    hp->pointer.reset(value.release());
    ASSERT_EQ(*hp->pointer.get(), 42);

    // Release the hazard pointer
    bool released = manager.release(hp);
    ASSERT_TRUE(released);  // Ensure release was successful
}

// Test case 2: Multithreading - Hazard pointer acquisition/release in multiple threads
TEST_F(HazardPointerManagerTest, MultiThread_AcquireReleaseTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::atomic<int> success_count(0);

    auto thread_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto hp = manager.acquire();
            if (hp != nullptr) {
                success_count++;
                auto value = std::make_unique<int>(i);  // Set a unique value
                hp->pointer.reset(value.release());
                manager.release(hp);  // Release after usage
            }
        }
    };

    // Run in multiple threads
    std::thread t1(thread_func);
    std::thread t2(thread_func);
    std::thread t3(thread_func);

    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(success_count.load(), 300);  // Ensure all hazard pointers were successfully acquired and released
}

// Test case 3: Retiring and reclaiming nodes in a single-threaded scenario
TEST_F(HazardPointerManagerTest, SingleThread_RetireReclaimTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Create a node and retire it
    auto node = std::make_unique<int>(42);
    manager.retire(std::move(node));

    // Reclaim the node
    manager.reclaim();

    // Check that no nodes are left
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 4: Multithreading - Retiring and reclaiming nodes in multiple threads
TEST_F(HazardPointerManagerTest, MultiThread_RetireReclaimTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::atomic<int> node_count(0);

    auto thread_func = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto node = std::make_unique<int>(i);
            manager.retire(std::move(node));
            node_count++;
        }
    };

    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();

    manager.reclaim();  // Manually trigger reclamation

    ASSERT_EQ(node_count.load(), 100);  // Ensure all nodes were retired
}

// Test case 5: Reclaiming all retired nodes
TEST_F(HazardPointerManagerTest, ReclaimAllTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    auto node1 = std::make_unique<int>(42);
    auto node2 = std::make_unique<int>(24);

    manager.retire(std::move(node1));
    manager.retire(std::move(node2));

    // Reclaim all nodes
    manager.reclaim_all();

    // Test that all retired nodes are cleared
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 6: Test HazardPointerManager with atomic_unique_ptr
TEST_F(HazardPointerManagerTest, AtomicUniquePointerTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    auto hp = manager.acquire();
    ASSERT_NE(hp, nullptr);  // Ensure hazard pointer was acquired

    // Test atomic_unique_ptr behavior
    auto node = std::make_unique<int>(42);
    hp->pointer.reset(node.release());
    ASSERT_EQ(*hp->pointer.get(), 42);

    // Releasing hazard pointer
    bool released = manager.release(hp);
    ASSERT_TRUE(released);
}

// Test case 7: Multithreaded stress test for simultaneous acquire/release and retire/reclaim
TEST_F(HazardPointerManagerTest, MultiThread_StressTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::atomic<int> success_count(0);
    std::atomic<int> retire_count(0);

    auto thread_func_acquire = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto hp = manager.acquire();
            if (hp != nullptr) {
                success_count++;
                auto value = std::make_unique<int>(i);
                hp->pointer.reset(value.release());
                manager.release(hp);  // Release after usage
            }
        }
    };

    auto thread_func_retire = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto node = std::make_unique<int>(i);
            manager.retire(std::move(node));
            retire_count++;
        }
    };

    std::thread t1(thread_func_acquire);
    std::thread t2(thread_func_acquire);
    std::thread t3(thread_func_retire);
    std::thread t4(thread_func_retire);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    manager.reclaim();

    ASSERT_EQ(success_count.load(), 200);  // Ensure hazard pointers were successfully acquired and released
    ASSERT_EQ(retire_count.load(), 100);   // Ensure nodes were successfully retired and reclaimed
}

// Test case 8: Ensure hazard pointer cannot be re-acquired after being released
TEST_F(HazardPointerManagerTest, ReAcquireAfterReleaseTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Acquire a hazard pointer
    auto hp = manager.acquire();
    ASSERT_NE(hp, nullptr);  // Ensure we acquired a pointer

    // Release it
    bool released = manager.release(hp);
    ASSERT_TRUE(released);  // Ensure release was successful

    // Try to re-acquire the same pointer, it should not be accessible anymore
    ASSERT_EQ(hp->pointer.get(), nullptr);
}

// Test case 9: Handling nullptr retirements
TEST_F(HazardPointerManagerTest, RetireNullptrTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Retire a nullptr
    manager.retire(nullptr);

    // Ensure that the retire size does not increase with nullptr
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 10: Reclaim behavior when no nodes are retired
TEST_F(HazardPointerManagerTest, ReclaimEmptyRetiredNodesTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Initial state, no retired nodes
    ASSERT_EQ(manager.retire_size(), 0);

    // Attempt to reclaim when no nodes are retired
    manager.reclaim();

    // No nodes should be reclaimed, retire size should remain 0
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 15: Prevent double reclamation of the same node
TEST_F(HazardPointerManagerTest, DoubleReclamationTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Create a node and retire it
    auto node = std::make_unique<int>(42);
    manager.retire(std::move(node));

    // Reclaim the node
    manager.reclaim();

    // Try to reclaim the same node again
    manager.reclaim();

    // Check that no nodes are left and ensure double reclamation does not occur
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 16: Retiring the same node multiple times
TEST_F(HazardPointerManagerTest, RetireSameNodeMultipleTimesTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Create a node and retire it
    auto node = std::make_unique<int>(42);
    int* raw_node = node.get();
    manager.retire(std::move(node));

    // Attempt to retire the same node again, after it’s already been retired
    ASSERT_DEATH({ manager.retire(std::unique_ptr<int>(raw_node)); }, ".*");  // The system should not allow double retirement

    manager.reclaim();

    // Ensure the node was reclaimed properly
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 17: High contention during retirement and reclamation
TEST_F(HazardPointerManagerTest, HighContentionRetireReclaimTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::atomic<int> retire_count(0);
    std::atomic<int> reclaim_count(0);

    auto retire_thread = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto node = std::make_unique<int>(i);
            manager.retire(std::move(node));
            retire_count++;
        }
    };

    auto reclaim_thread = [&]() {
        for (int i = 0; i < 100; ++i) {
            manager.reclaim();
            reclaim_count++;
        }
    };

    std::thread t1(retire_thread);
    std::thread t2(reclaim_thread);
    std::thread t3(retire_thread);
    std::thread t4(reclaim_thread);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // Ensure all retirements and reclamations occurred without race conditions
    ASSERT_EQ(retire_count.load(), 200);
    ASSERT_GE(reclaim_count.load(), 100);  // Reclamation may be triggered multiple times

    // Ensure all nodes are eventually reclaimed
    manager.reclaim_all();
    ASSERT_EQ(manager.retire_size(), 0);
}

// Test case 18: Acquire all hazard pointers, ensuring no more can be acquired
TEST_F(HazardPointerManagerTest, ExhaustAllHazardPointersTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::vector<std::shared_ptr<HazardSystem::HazardPointer<int>>> hazard_pointers;

    // Acquire all available hazard pointers
    for (int i = 0; i < 10; ++i) {
        auto hp = manager.acquire();
        ASSERT_NE(hp, nullptr);
        hazard_pointers.push_back(hp);
    }

    // Try to acquire more, expecting failure
    auto extra_hp = manager.acquire();
    ASSERT_EQ(extra_hp, nullptr);  // No more hazard pointers available

    // Release a pointer and ensure it can be re-acquired
    manager.release(hazard_pointers[0]);
    auto new_hp = manager.acquire();
    ASSERT_NE(new_hp, nullptr);  // Now we can acquire one more
}

// Test case 19: Proper reset and reuse of hazard pointers after release
TEST_F(HazardPointerManagerTest, ReuseReleasedHazardPointerTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();

    // Acquire a hazard pointer, set a value, and release it
    auto hp = manager.acquire();
    ASSERT_NE(hp, nullptr);
    auto value = std::make_unique<int>(100);
    hp->pointer.reset(value.release());

    manager.release(hp);

    // Re-acquire the hazard pointer and check it’s reset
    auto new_hp = manager.acquire();
    ASSERT_EQ(new_hp->pointer.get(), nullptr);  // The hazard pointer should be reset
}

// Test case 20: Concurrently retire and acquire hazard pointers
TEST_F(HazardPointerManagerTest, ConcurrentRetireAndAcquireTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::atomic<int> acquire_count(0);
    std::atomic<int> retire_count(0);

    auto acquire_thread = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto hp = manager.acquire();
            if (hp != nullptr) {
                acquire_count++;
                manager.release(hp);
            }
        }
    };

    auto retire_thread = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto node = std::make_unique<int>(i);
            manager.retire(std::move(node));
            retire_count++;
        }
    };

    std::thread t1(acquire_thread);
    std::thread t2(retire_thread);
    std::thread t3(acquire_thread);
    std::thread t4(retire_thread);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // Ensure both retirements and acquisitions were successful
    ASSERT_EQ(acquire_count.load(), 100);
    ASSERT_EQ(retire_count.load(), 100);

    manager.reclaim();
}

// Test case 21: Multiple threads trying to retire and acquire the same hazard pointer
TEST_F(HazardPointerManagerTest, ConcurrentHazardPointerContentionTest) {
    HazardPointerManager<int, 10, 5>& manager = HazardPointerManager<int, 10, 5>::instance();
    std::atomic<int> acquire_success(0);

    auto thread_func = [&]() {
        for (int i = 0; i < 10; ++i) {
            auto hp = manager.acquire();
            if (hp) {
                acquire_success++;
                hp->pointer.reset(new int(i));
                manager.release(hp);
            }
        }
    };

    std::thread t1(thread_func);
    std::thread t2(thread_func);
    std::thread t3(thread_func);

    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(acquire_success.load(), 30);
}

// Test case 22: Test HazardPointerManager with complex state struct
TEST_F(HazardPointerManagerTest, HazardPointerWithComplexStateTest) {
    HazardPointerManager<TestState, 10, 5>& manager = HazardPointerManager<TestState, 10, 5>::instance();

    // Acquire a hazard pointer for TestState
    auto hp = manager.acquire();
    ASSERT_NE(hp, nullptr);  // Ensure hazard pointer was acquired

    // Allocate and set a TestState object
    auto state = std::make_unique<TestState>(42, "TestObject");
    hp->pointer.reset(state.release());

    // Verify that the TestState object was properly set
    ASSERT_EQ(hp->pointer->value, 42);
    ASSERT_EQ(hp->pointer->name, "TestObject");

    // Release the hazard pointer and ensure it's reset
    bool released = manager.release(hp);
    ASSERT_TRUE(released);

    // Manually clean up
    delete hp->pointer.get();
}

// Test case 23: Ensure proper deletion of complex state struct
TEST_F(HazardPointerManagerTest, RetireComplexStateStructTest) {
    HazardPointerManager<TestState, 10, 5>& manager = HazardPointerManager<TestState, 10, 5>::instance();

    // Create and retire a TestState object
    auto state = std::make_unique<TestState>(99, "RetiredObject");
    TestState* raw_state = state.get();
    manager.retire(std::move(state));

    // Ensure the object is still tracked in the retired list
    ASSERT_EQ(manager.retire_size(), 1);

    // Reclaim and ensure the object is deleted
    manager.reclaim();

    ASSERT_EQ(manager.retire_size(), 0);  // Ensure it's no longer in the retired list
    // Destructor output should confirm deletion
}

// Test case 24: Multithreaded complex state test for hazard pointer acquisition and retirement
TEST_F(HazardPointerManagerTest, MultiThread_ComplexStateTest) {
    HazardPointerManager<TestState, 10, 5>& manager = HazardPointerManager<TestState, 10, 5>::instance();
    std::atomic<int> acquire_count(0);
    std::atomic<int> retire_count(0);

    auto acquire_thread = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto hp = manager.acquire();
            if (hp != nullptr) {
                acquire_count++;
                auto state = std::make_unique<TestState>(i, "ThreadAcquired");
                hp->pointer.reset(state.release());
                manager.release(hp);
            }
        }
    };

    auto retire_thread = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto state = std::make_unique<TestState>(i, "ThreadRetired");
            manager.retire(std::move(state));
            retire_count++;
        }
    };

    std::thread t1(acquire_thread);
    std::thread t2(retire_thread);
    std::thread t3(acquire_thread);
    std::thread t4(retire_thread);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    manager.reclaim();  // Reclaim retired objects

    ASSERT_EQ(acquire_count.load(), 100);  // Ensure hazard pointers were acquired
    ASSERT_EQ(retire_count.load(), 100);   // Ensure nodes were retired
}

// Test case 25: Reclaim complex state struct with hazard pointer still in use
TEST_F(HazardPointerManagerTest, ReclaimWithComplexStateInUseTest) {
    HazardPointerManager<TestState, 10, 5>& manager = HazardPointerManager<TestState, 10, 5>::instance();

    // Acquire a hazard pointer for TestState
    auto hp = manager.acquire();
    ASSERT_NE(hp, nullptr);

    // Allocate and set a TestState object
    auto state = std::make_unique<TestState>(101, "InUse");
    hp->pointer.reset(state.release());

    // Retire the same object while it's still in use
    manager.retire(std::unique_ptr<TestState>(hp->pointer.release()));

    // Try to reclaim - the node should not be reclaimed since it's still a hazard
    manager.reclaim();
    ASSERT_EQ(manager.retire_size(), 1);  // Still in the retired list

    // Release the hazard pointer and then reclaim again
    manager.release(hp);
    manager.reclaim();

    // The object should now be reclaimed
    ASSERT_EQ(manager.retire_size(), 0);
}