#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include "HashTable.hpp"

// Use the HazardSystem namespace
using HazardSystem::HashTable;

// Define a simple class to use as data in the hash table
struct TestNode {
    int data;
    TestNode(int d) : data(d) {}
    ~TestNode() {
        std::cout << "TestNode with data " << data << " is deleted.\n";
    }
};

// Test fixture for setting up the tests
class HashTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up for each test
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test case 1: Single-threaded insert, find, and remove
TEST_F(HashTableTest, SingleThread_InsertFindRemoveTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Insert elements
    for (int i = 0; i < 10; ++i) {
        auto node = std::make_unique<TestNode>(i);
        ASSERT_TRUE(hash_table.insert(i, std::move(node)));
    }

    // Find elements
    for (int i = 0; i < 10; ++i) {
        auto node = hash_table.find(i);
        ASSERT_NE(node, nullptr);
        ASSERT_EQ(node->data, i);
    }

    // Remove elements
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(hash_table.remove(i));
        ASSERT_EQ(hash_table.find(i), nullptr);  // Ensure the node is removed
    }

    ASSERT_EQ(hash_table.size(), 0);
}

// Test case 2: Multithreaded insertion test
TEST_F(HashTableTest, MultiThread_InsertTest) {
    HashTable<int, TestNode, 100> hash_table;
    std::atomic<int> success_count(0);

    auto thread_func = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto node = std::make_unique<TestNode>(i);
            if (hash_table.insert(i, std::move(node))) {
                success_count++;
            }
        }
    };

    // Run insertions in multiple threads
    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();

    ASSERT_EQ(success_count, 100);  // Ensure all insertions succeeded
}

// Test case 3: Multithreaded find test
TEST_F(HashTableTest, MultiThread_FindTest) {
    HashTable<int, TestNode, 100> hash_table;

    // Insert elements
    for (int i = 0; i < 100; ++i) {
        auto node = std::make_unique<TestNode>(i);
        ASSERT_TRUE(hash_table.insert(i, std::move(node)));
    }

    auto thread_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto node = hash_table.find(i);
            ASSERT_NE(node, nullptr);
            ASSERT_EQ(node->data, i);
        }
    };

    // Run find in multiple threads
    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();
}

// Test case 4: Multithreaded removal test
TEST_F(HashTableTest, MultiThread_RemoveTest) {
    HashTable<int, TestNode, 100> hash_table;

    // Insert elements
    for (int i = 0; i < 100; ++i) {
        auto node = std::make_unique<TestNode>(i);
        ASSERT_TRUE(hash_table.insert(i, std::move(node)));
    }

    auto thread_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            hash_table.remove(i);
        }
    };

    // Run removal in multiple threads
    std::thread t1(thread_func);
    std::thread t2(thread_func);

    t1.join();
    t2.join();

    // Ensure the table is empty
    ASSERT_EQ(hash_table.size(), 0);
}

// Test case 5: Multithreaded insert, find, and remove test
TEST_F(HashTableTest, MultiThread_InsertFindRemoveTest) {
    HashTable<int, TestNode, 100> hash_table;
    std::atomic<int> insert_count(0);
    std::atomic<int> find_count(0);
    std::atomic<int> remove_count(0);

    // Thread function to insert elements
    auto insert_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto node = std::make_unique<TestNode>(i);
            if (hash_table.insert(i, std::move(node))) {
                insert_count++;
            }
        }
    };

    // Thread function to find elements
    auto find_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto node = hash_table.find(i);
            if (node != nullptr) {
                find_count++;
            }
        }
    };

    // Thread function to remove elements
    auto remove_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            if (hash_table.remove(i)) {
                remove_count++;
            }
        }
    };

    // Run in multiple threads
    std::thread t1(insert_func);
    std::thread t2(find_func);
    std::thread t3(remove_func);

    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(insert_count, 100);
    ASSERT_EQ(find_count, 100);
    ASSERT_EQ(remove_count, 100);
}

// Test case 6: Test reclaiming unused nodes
TEST_F(HashTableTest, ReclaimNodesTest) {
    HashTable<int, TestNode, 100> hash_table;

    // Insert elements
    for (int i = 0; i < 100; ++i) {
        auto node = std::make_unique<TestNode>(i);
        ASSERT_TRUE(hash_table.insert(i, std::move(node)));
    }

    // Reclaim nodes that are no longer hazards (in this case, everything)
    hash_table.reclaim([](const std::shared_ptr<TestNode>& node) -> bool {
        return false;  // No hazards; reclaim everything
    });

    // Check that the table is empty after reclaim
    ASSERT_EQ(hash_table.size(), 0);
}

// Test case 7: Insert the same key twice (failure case)
TEST_F(HashTableTest, InsertDuplicateKeyTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Insert an element
    auto node1 = std::make_unique<TestNode>(1);
    ASSERT_TRUE(hash_table.insert(1, std::move(node1)));

    // Attempt to insert another element with the same key
    auto node2 = std::make_unique<TestNode>(2);
    ASSERT_FALSE(hash_table.insert(1, std::move(node2)));  // Should return false
}

// Test case 8: Removing a non-existent key
TEST_F(HashTableTest, RemoveNonExistentKeyTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Attempt to remove a non-existent key
    ASSERT_FALSE(hash_table.remove(999));
}

// Test case 9: Hash collision handling (basic linear probing)
TEST_F(HashTableTest, HashCollisionHandlingTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Insert two elements with the same hash index (e.g., use a custom key with the same hash value)
    auto node1 = std::make_unique<TestNode>(1);
    auto node2 = std::make_unique<TestNode>(11);  // Assuming 11 and 1 have the same hash index

    ASSERT_TRUE(hash_table.insert(1, std::move(node1)));
    ASSERT_TRUE(hash_table.insert(11, std::move(node2)));  // Collision resolution should handle this

    // Ensure both nodes are retrievable
    ASSERT_NE(hash_table.find(1), nullptr);
    ASSERT_NE(hash_table.find(11), nullptr);
}

// Test case 10: Insert into a full table
TEST_F(HashTableTest, InsertIntoFullTableTest) {
    HashTable<int, TestNode, 5> hash_table;  // Smaller table with only 5 buckets

    // Insert elements up to the capacity
    for (int i = 0; i < 5; ++i) {
        auto node = std::make_unique<TestNode>(i);
        ASSERT_TRUE(hash_table.insert(i, std::move(node)));
    }

    // Try to insert an element into the full table (assuming linear probing or chaining is not allowed)
    auto extra_node = std::make_unique<TestNode>(100);
    ASSERT_FALSE(hash_table.insert(100, std::move(extra_node)));  // Should fail due to full capacity
}

// Test case 11: Remove from an empty table
TEST_F(HashTableTest, RemoveFromEmptyTableTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Try removing a non-existent key from an empty table
    ASSERT_FALSE(hash_table.remove(1));  // Should fail, as the table is empty
}


// Test case 12: Reinsert After Removal
TEST_F(HashTableTest, ReinsertAfterRemovalTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Insert an element, remove it, and then reinsert it
    auto node1 = std::make_unique<TestNode>(42);
    ASSERT_TRUE(hash_table.insert(1, std::move(node1)));
    ASSERT_TRUE(hash_table.remove(1));

    // Reinsert the element
    auto node2 = std::make_unique<TestNode>(42);
    ASSERT_TRUE(hash_table.insert(1, std::move(node2)));  // Should succeed
}


// Test case 13: Simultaneous Insert and Remove
TEST_F(HashTableTest, MultiThread_SimultaneousInsertRemoveTest) {
    HashTable<int, TestNode, 100> hash_table;
    std::atomic<int> insert_count(0);
    std::atomic<int> remove_count(0);

    auto insert_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto node = std::make_unique<TestNode>(i);
            if (hash_table.insert(i, std::move(node))) {
                insert_count++;
            }
        }
    };

    auto remove_func = [&]() {
        for (int i = 0; i < 100; ++i) {
            if (hash_table.remove(i)) {
                remove_count++;
            }
        }
    };

    // Run simultaneous inserts and removes
    std::thread t1(insert_func);
    std::thread t2(remove_func);

    t1.join();
    t2.join();

    // Assert the final state of the table
    ASSERT_EQ(insert_count, 100);
    ASSERT_EQ(remove_count, 100);
    ASSERT_EQ(hash_table.size(), 0);  // Ensure all elements were eventually removed
}


// Test case 14: Insert with Same Key and Different Data
TEST_F(HashTableTest, InsertSameKeyDifferentDataTest) {
    HashTable<int, TestNode, 10> hash_table;

    // Insert an element with key 1
    auto node1 = std::make_unique<TestNode>(42);
    ASSERT_TRUE(hash_table.insert(1, std::move(node1)));

    // Try to insert another element with the same key but different data
    auto node2 = std::make_unique<TestNode>(100);
    bool inserted = hash_table.insert(1, std::move(node2));

    if (inserted) {
        // If allowed, check that the data has been updated
        auto node = hash_table.find(1);
        ASSERT_NE(node, nullptr);
        ASSERT_EQ(node->data, 100);  // Ensure data was updated to 100
    } else {
        // If not allowed, ensure the data remains the same
        auto node = hash_table.find(1);
        ASSERT_NE(node, nullptr);
        ASSERT_EQ(node->data, 42);  // Data remains unchanged
    }
}

// Test case 15: Stress Test with Large Number of Elements
TEST_F(HashTableTest, StressTest_LargeNumberOfElements) {
    HashTable<int, TestNode, 1000> hash_table;

    // Insert a large number of elements
    for (int i = 0; i < 1000; ++i) {
        auto node = std::make_unique<TestNode>(i);
        ASSERT_TRUE(hash_table.insert(i, std::move(node)));
    }

    // Verify the number of elements
    ASSERT_EQ(hash_table.size(), 1000);

    // Find all elements
    for (int i = 0; i < 1000; ++i) {
        auto node = hash_table.find(i);
        ASSERT_NE(node, nullptr);
        ASSERT_EQ(node->data, i);
    }

    // Remove all elements
    for (int i = 0; i < 1000; ++i) {
        ASSERT_TRUE(hash_table.remove(i));
    }

    // Ensure the table is empty
    ASSERT_EQ(hash_table.size(), 0);
}
