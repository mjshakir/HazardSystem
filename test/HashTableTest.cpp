#include <gtest/gtest.h>
#include <gmock/gmock.h>
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
    hash_table.reclaim([](const TestNode* node) -> bool {
        return false;  // No hazards; reclaim everything
    });

    // Check that the table is empty after reclaim
    ASSERT_EQ(hash_table.size(), 0);
}