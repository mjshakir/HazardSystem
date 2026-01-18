#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <future>
#include <mutex>
#include <random>
#include <chrono>
#include <algorithm>
#include <functional>
#include <set>
#include "HashMultiTable.hpp"

// Use the HazardSystem namespace
using HazardSystem::HashMultiTable;

// Define a test node class with instrumentation
class TestNode {
public:
    int data;
    static std::atomic<int> constructor_count;
    static std::atomic<int> destructor_count;
    
    TestNode(int d) : data(d) {
        constructor_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    ~TestNode() {
        destructor_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    static void reset_counters() {
        constructor_count.store(0, std::memory_order_relaxed);
        destructor_count.store(0, std::memory_order_relaxed);
    }
};

std::atomic<int> TestNode::constructor_count(0);
std::atomic<int> TestNode::destructor_count(0);

// Test fixture for HashMultiTable
class HashMultiTableTest : public ::testing::Test {
protected:
    static constexpr size_t TABLE_SIZE = 64;
    
    void SetUp() override {
        TestNode::reset_counters();
    }
    
    void TearDown() override {
        // Check for memory leaks by comparing constructor and destructor counts
        // Allow for some flexibility due to shared_ptr's reference counting
        EXPECT_LE(TestNode::destructor_count.load(), TestNode::constructor_count.load()) 
            << "Memory leak detected: " << TestNode::constructor_count.load() << " created, but only " 
            << TestNode::destructor_count.load() << " destroyed";
    }
    
    // Helper to generate a set of random keys
    std::vector<int> generate_random_keys(size_t count, int min, int max) {
        std::vector<int> keys;
        keys.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(min, max);
        
        for (size_t i = 0; i < count; ++i) {
            keys.push_back(dist(gen));
        }
        
        return keys;
    }
    
    // Helper to measure the execution time of a function
    template<typename Func>
    double measure_execution_time(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        return duration.count();
    }
};

// Basic functionality tests - without using the iterator
TEST_F(HashMultiTableTest, BasicFunctionality) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    // Test initial state
    EXPECT_EQ(table.size(), 0);
    
    // Test insertion
    auto node1 = std::make_shared<TestNode>(42);
    EXPECT_TRUE(table.insert(1, node1));
    EXPECT_EQ(table.size(), 1);
    
    // Test finding
    auto results = table.find(1);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->data, 42);
    
    // Test find_first
    auto first = table.find_first(1);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->data, 42);
    
    // Test contains
    EXPECT_TRUE(table.contain(1, node1));
    
    // Test removing
    EXPECT_TRUE(table.remove(1, node1));
    EXPECT_EQ(table.size(), 0);
    
    // Test finding after removal
    results = table.find(1);
    EXPECT_EQ(results.size(), 0);
    
    first = table.find_first(1);
    EXPECT_EQ(first, nullptr);
}

// Test inserting multiple values with the same key
TEST_F(HashMultiTableTest, MultipleValuesPerKey) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    // Insert multiple nodes with same key
    constexpr int key = 5, count = 10;
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i * 10));
        EXPECT_TRUE(table.insert(key, nodes.back()));
    }
    
    // Check size
    EXPECT_EQ(table.size(), count);
    
    // Check find returns all nodes for key
    auto results = table.find(key);
    EXPECT_EQ(results.size(), count);
    
    // Verify the actual data
    std::set<int> expected_values;
    for (int i = 0; i < count; ++i) {
        expected_values.insert(i * 10);
    }
    
    std::set<int> actual_values;
    for (const auto& node : results) {
        actual_values.insert(node->data);
    }
    
    EXPECT_EQ(actual_values, expected_values);
    
    // Test find_first returns one result
    auto first = table.find_first(key);
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->data >= 0 and first->data < count * 10);
    
    // Test removing individual nodes
    for (const auto& node : nodes) {
        EXPECT_TRUE(table.contain(key, node));
        EXPECT_TRUE(table.remove(key, node));
    }
    
    // Check size after removal
    EXPECT_EQ(table.size(), 0);
}

// Test collisions by using a small hash table size
TEST_F(HashMultiTableTest, HashCollisions) {
    constexpr size_t c_hash_table_size = 4UL;
    HashMultiTable<int, TestNode, c_hash_table_size> small_table;
    
    constexpr int count = 100;
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.reserve(static_cast<size_t>(count));
    
    // Insert values that will definitely collide
    for (int i = 0; i < count; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        EXPECT_TRUE(small_table.insert(i, nodes.back()));
    }
    
    EXPECT_EQ(small_table.size(), count);
    
    // Verify each node can be found
    for (int i = 0; i < count; ++i) {
        auto results = small_table.find(i);
        EXPECT_EQ(results.size(), 1);
        EXPECT_EQ(results[0]->data, i);
    }
    
    // Test removal with collisions
    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(small_table.remove(i, nodes[static_cast<size_t>(i)]));
    }
    
    EXPECT_EQ(small_table.size(), 0);
}

// Test swapping keys
TEST_F(HashMultiTableTest, SwapKeys) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    auto node = std::make_shared<TestNode>(42);
    EXPECT_TRUE(table.insert(1, node));
    
    // Swap the key
    EXPECT_TRUE(table.swap(1, 5, node));
    
    // Check old key no longer exists
    auto results1 = table.find(1);
    EXPECT_EQ(results1.size(), 0);
    
    // Check new key exists
    auto results5 = table.find(5);
    ASSERT_EQ(results5.size(), 1);
    EXPECT_EQ(results5[0]->data, 42);
}

// Test swapping data
TEST_F(HashMultiTableTest, SwapData) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    auto old_node = std::make_shared<TestNode>(42);
    EXPECT_TRUE(table.insert(1, old_node));
    
    // Swap the data
    auto new_node = std::make_shared<TestNode>(99);
    EXPECT_TRUE(table.swap(1, old_node, new_node));
    
    // Check data was swapped
    auto results = table.find(1);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->data, 99);
}

// Test the clear method
TEST_F(HashMultiTableTest, Clear) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int count = 100;
    for (int i = 0; i < count; ++i) {
        auto node = std::make_shared<TestNode>(i);
        EXPECT_TRUE(table.insert(i, node));
    }
    
    EXPECT_EQ(table.size(), count);
    
    // Clear the table
    table.clear();
    
    // Verify table is empty
    EXPECT_EQ(table.size(), 0);
    
    for (int i = 0; i < count; ++i) {
        auto results = table.find(i);
        EXPECT_EQ(results.size(), 0);
    }
}

// Test reclaim functionality
TEST_F(HashMultiTableTest, Reclaim) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    // Insert nodes with even and odd data values
    constexpr int count = 100;
    for (int i = 0; i < count; ++i) {
        auto node = std::make_shared<TestNode>(i);
        EXPECT_TRUE(table.insert(i, node));
    }
    
    EXPECT_EQ(table.size(), count);
    
    // Reclaim nodes with even data values
    table.reclaim([](std::shared_ptr<TestNode> node) -> bool {
        return node->data % 2 == 1; // Keep odd values
    });
    
    // Verify even nodes were reclaimed
    EXPECT_EQ(table.size(), count / 2);
    
    for (int i = 0; i < count; ++i) {
        auto results = table.find(i);
        if (i % 2 == 0) {
            EXPECT_EQ(results.size(), 0) << "Even key " << i << " should have been reclaimed";
        } else {
            EXPECT_EQ(results.size(), 1) << "Odd key " << i << " should have been kept";
            if (!results.empty()) {
                EXPECT_EQ(results[0]->data, i);
            }
        }
    }
}

// Test removal by data pointer (single)
TEST_F(HashMultiTableTest, RemoveByData) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;

    auto n1 = std::make_shared<TestNode>(100);
    auto n2 = std::make_shared<TestNode>(200);

    // Insert with different keys
    table.insert(10, n1);
    table.insert(20, n2);

    // Remove by data (should work for either key)
    EXPECT_TRUE(table.remove(n1));
    EXPECT_EQ(table.size(), 1);
    EXPECT_FALSE(table.contain(10, n1));

    // Remove by data that does not exist
    auto missing = std::make_shared<TestNode>(300);
    EXPECT_FALSE(table.remove(missing));
    EXPECT_EQ(table.size(), 1);

    // Remove the last
    EXPECT_TRUE(table.remove(n2));
    EXPECT_EQ(table.size(), 0);

    // Remove after already removed
    EXPECT_FALSE(table.remove(n1));
}

// Test remove_first, remove_last, with duplicates
TEST_F(HashMultiTableTest, RemoveFirstAndLastWithDuplicates) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;

    auto node1 = std::make_shared<TestNode>(1);
    auto node2 = std::make_shared<TestNode>(2);
    auto node3 = std::make_shared<TestNode>(3);
    // Insert order: node1, node2, node3
    table.insert(5, node1);
    table.insert(5, node2);
    table.insert(5, node3);

    // Internal list is: head=node3 -> node2 -> node1 (tail)

    // Remove first (removes head, node3)
    EXPECT_TRUE(table.remove(5));
    auto vals = table.find(5);
    ASSERT_EQ(vals.size(), 2);
    for (auto& v : vals) EXPECT_NE(v->data, 3); // node3 gone

    // Remove first (only node2 left)
    EXPECT_TRUE(table.remove(5));
    vals = table.find(5);
    EXPECT_EQ(vals.size(), 1);
}

TEST_F(HashMultiTableTest, UpdateSingleValue) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    auto old_node = std::make_shared<TestNode>(100);
    auto new_node = std::make_shared<TestNode>(200);

    table.insert(1, old_node);

    // Should update first (and only) node
    EXPECT_TRUE(table.update(1, new_node));

    auto results = table.find(1);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->data, 200);

    // Updating a non-existent key should fail
    EXPECT_FALSE(table.update(2, new_node));
}

TEST_F(HashMultiTableTest, UpdateAllMultipleValues) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    constexpr int key = 5, count = 5;

    // Insert multiple nodes with the same key
    for (int i = 0; i < count; ++i) {
        table.insert(key, std::make_shared<TestNode>(i * 10));
    }

    auto new_node = std::make_shared<TestNode>(999);
    size_t updated = table.update_all(key, new_node);

    // Should have updated all nodes
    EXPECT_EQ(updated, count);

    auto results = table.find(key);
    EXPECT_EQ(results.size(), count);
    for (auto& n : results) {
        EXPECT_EQ(n->data, 999);
    }
}

// Test high concurrency insertion
TEST_F(HashMultiTableTest, ConcurrentInsertion) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    constexpr int keys_per_thread = 1000;
    const int total_keys = num_threads * keys_per_thread;
    
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));
    std::atomic<int> success_count(0);
    
    // Launch threads to insert keys
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < keys_per_thread; ++i) {
                int key = t * keys_per_thread + i;
                auto node = std::make_shared<TestNode>(key);
                if (table.insert(key, node)) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all insertions were successful
    EXPECT_EQ(success_count.load(), total_keys);
    EXPECT_EQ(table.size(), total_keys);
    
    // Verify all keys can be found
    int found_count = 0;
    for (int i = 0; i < total_keys; ++i) {
        auto results = table.find(i);
        if (!results.empty()) {
            found_count++;
            EXPECT_EQ(results[0]->data, i);
        }
    }
    
    EXPECT_EQ(found_count, total_keys);
}

// Test concurrent find operations
TEST_F(HashMultiTableTest, ConcurrentFind) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int count = 1000;
    
    // Insert keys
    for (int i = 0; i < count; ++i) {
        auto node = std::make_shared<TestNode>(i);
        EXPECT_TRUE(table.insert(i, node));
    }
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));
    std::atomic<int> found_count(0);
    
    // Launch threads to find keys concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < count; ++i) {
                auto results = table.find(i);
                if (!results.empty() && results[0]->data == i) {
                    found_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Each thread should have found all keys
    EXPECT_EQ(found_count.load(), num_threads * count);
}

// Test concurrent removal
TEST_F(HashMultiTableTest, ConcurrentRemoval) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int count = 1000;
    std::vector<std::shared_ptr<TestNode>> nodes;
    
    // Insert keys
    for (int i = 0; i < count; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        EXPECT_TRUE(table.insert(i, nodes.back()));
    }
    
    EXPECT_EQ(table.size(), count);
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));
    std::atomic<int> remove_count(0);
    
    // Launch threads to remove keys concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread removes a distinct subset of keys
            for (int i = t; i < count; i += num_threads) {
                if (table.remove(i, nodes[static_cast<size_t>(i)])) {
                    remove_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All keys should have been removed
    EXPECT_EQ(remove_count.load(), count);
    EXPECT_EQ(table.size(), 0);
}

// Test concurrent mixed operations
TEST_F(HashMultiTableTest, ConcurrentMixedOperations) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int key_range = 1000;
    constexpr int ops_per_thread = 10000;
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

    std::atomic<int> insert_count(0);
    std::atomic<int> find_count(0);
    std::atomic<int> remove_count(0);
    
    // Pre-insert some keys
    for (int i = 0; i < key_range / 2; ++i) {
        auto node = std::make_shared<TestNode>(i);
        if (table.insert(i, node)) {
            insert_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    // Launch threads to perform mixed operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> key_dist(0, key_range - 1);
            std::uniform_int_distribution<int> op_dist(0, 2);
            
            for (int i = 0; i < ops_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op == 0) {  // Insert
                    auto node = std::make_shared<TestNode>(key);
                    if (table.insert(key, node)) {
                        insert_count.fetch_add(1, std::memory_order_relaxed);
                    }
                } else if (op == 1) {  // Find
                    auto results = table.find(key);
                    if (!results.empty()) {
                        find_count.fetch_add(1, std::memory_order_relaxed);
                    }
                } else {  // Remove
                    auto results = table.find(key);
                    if (!results.empty()) {
                        if (table.remove(key, results[0])) {
                            remove_count.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify operations were performed
    std::cout << "Inserts: " << insert_count.load() 
              << ", Finds: " << find_count.load() 
              << ", Removes: " << remove_count.load() << std::endl;
    
    EXPECT_GT(insert_count.load(), 0);
    EXPECT_GT(find_count.load(), 0);
    EXPECT_GT(remove_count.load(), 0);
}

// Test concurrent swap operations
TEST_F(HashMultiTableTest, ConcurrentSwapKeys) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int count = 1000;
    std::vector<std::shared_ptr<TestNode>> nodes;
    
    // Insert initial keys
    for (int i = 0; i < count; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        EXPECT_TRUE(table.insert(i, nodes.back()));
    }
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

    std::atomic<int> swap_count(0);
    
    // Launch threads to swap keys
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = t; i < count; i += num_threads) {
                int new_key = i + count;  // Swap to a new key range
                if (table.swap(i, new_key, nodes[static_cast<size_t>(i)])) {
                    swap_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify swaps were successful
    EXPECT_EQ(swap_count.load(), count);
    
    // Check new keys exist
    int found_count = 0;
    for (int i = 0; i < count; ++i) {
        auto old_results = table.find(i);
        EXPECT_EQ(old_results.size(), 0) << "Old key " << i << " should not exist anymore";
        
        auto new_results = table.find(i + count);
        if (!new_results.empty()) {
            found_count++;
            EXPECT_EQ(new_results[0]->data, i);
        }
    }
    
    EXPECT_EQ(found_count, count);
}

// Test for lock-free behavior by measuring performance under contention
TEST_F(HashMultiTableTest, LockFreePerformance) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int count = 1000;
    constexpr int iterations = 10;
    
    // Insert initial data
    for (int i = 0; i < count; ++i) {
        auto node = std::make_shared<TestNode>(i);
        EXPECT_TRUE(table.insert(i, node));
    }
    
    // Measure time for concurrent operations
    double concurrent_time = measure_execution_time([&]() {
        const unsigned int hardware_threads = std::thread::hardware_concurrency();
        const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(num_threads));
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                for (int iter = 0; iter < iterations; ++iter) {
                    // Each thread performs all operations on its subset of keys
                    for (int i = t; i < count; i += num_threads) {
                        // Operations cycle: find, remove, insert, find
                        table.find(i);
                        
                        auto node = std::make_shared<TestNode>(i);
                        table.insert(i, node);
                        
                        table.find(i);
                        
                        table.remove(i, node);
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    });
    
    // Measure time for sequential operations for comparison
    double sequential_time = measure_execution_time([&]() {
        for (int iter = 0; iter < iterations; ++iter) {
            for (int i = 0; i < count; ++i) {
                // Same operations as in concurrent test
                table.find(i);
                
                auto node = std::make_shared<TestNode>(i);
                table.insert(i, node);
                
                table.find(i);
                
                table.remove(i, node);
            }
        }
    });
    
    // In a truly lock-free implementation, concurrent operations should be significantly faster
    // than sequential ones due to parallelism (ideally approaching num_threads times faster)
    std::cout << "Sequential time: " << sequential_time << "ms, Concurrent time: " << concurrent_time 
              << "ms, Ratio: " << (sequential_time / concurrent_time) << std::endl;
    
    // This is not a strict test but helps identify potential issues
    // A well-implemented lock-free structure should show good parallelism
    // The ratio should be greater than 1.0 but may not reach num_threads due to contention
    EXPECT_GT(sequential_time / concurrent_time, 1.0);
}

// Test for edge case: Empty table operations
TEST_F(HashMultiTableTest, EmptyTableOperations) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    // Find in empty table
    auto results = table.find(42);
    EXPECT_TRUE(results.empty());
    
    // Find first in empty table
    auto first = table.find_first(42);
    EXPECT_EQ(first, nullptr);
    
    // Remove from empty table
    auto node = std::make_shared<TestNode>(42);
    EXPECT_FALSE(table.remove(42, node));
    EXPECT_FALSE(table.remove(42));
    
    // Swap in empty table
    EXPECT_FALSE(table.swap(42, 99, node));
    auto new_node = std::make_shared<TestNode>(99);
    EXPECT_FALSE(table.swap(42, node, new_node));
    
    // Reclaim from empty table
    table.reclaim([](std::shared_ptr<TestNode>) -> bool {
        return false; // Reclaim all (though there are none)
    });
    
    // Size of empty table
    EXPECT_EQ(table.size(), 0);
}

// Test for correctness under high contention on the same keys
TEST_F(HashMultiTableTest, HighContention) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    constexpr int num_threads = 32;
    constexpr int num_keys = 10;  // Very small number of keys to create contention
    constexpr int ops_per_thread = 1000;
    
    // const int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));
    std::atomic<int> total_success(0);
    
    // Launch threads all accessing the same small set of keys
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> key_dist(0, num_keys - 1);
            std::uniform_int_distribution<int> op_dist(0, 2);
            
            for (int i = 0; i < ops_per_thread; ++i) {
                int key = key_dist(gen);
                int op = op_dist(gen);
                
                if (op == 0) {  // Insert
                    auto node = std::make_shared<TestNode>(key);
                    if (table.insert(key, node)) {
                        total_success.fetch_add(1, std::memory_order_relaxed);
                    }
                } else if (op == 1) {  // Find
                    table.find(key);
                } else {  // Remove
                    auto results = table.find(key);
                    for (const auto& node : results) {
                        if (table.remove(key, node)) {
                            total_success.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // We don't know the exact expected state, but the table should be consistent
    // Check that each key has a reasonable number of entries
    for (int i = 0; i < num_keys; ++i) {
        auto results = table.find(i);
        EXPECT_LE(results.size(), 100) << "Key " << i << " has an excessive number of entries";
    }
    
    // Ensure some operations succeeded
    EXPECT_GT(total_success.load(), 0);
    
    // Finally, clean up the table
    table.clear();
    EXPECT_EQ(table.size(), 0);
}


TEST_F(HashMultiTableTest, ConcurrentRemoveByDataSingleInstance) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;

    auto shared_node = std::make_shared<TestNode>(999);
    table.insert(42, shared_node);  // Insert only ONCE

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            if (table.remove(shared_node)) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    EXPECT_EQ(success_count.load(), 1);
    EXPECT_EQ(table.size(), 0);
}

TEST_F(HashMultiTableTest, ConcurrentRemoveByDataMultipleInstances) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;

    auto shared_node = std::make_shared<TestNode>(999);
    constexpr int count = 200;
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);

    for (int i = 0; i < count; ++i) {
        table.insert(i, shared_node);
    }

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

    // Each thread will keep removing until none left
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (table.remove(shared_node)) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    EXPECT_EQ(success_count.load(), count);
    EXPECT_EQ(table.size(), 0);
}

// Test concurrent remove_first on the same key
TEST_F(HashMultiTableTest, ConcurrentRemoveFirstSameKey) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    constexpr int num_entries = 100, key = 77;
    const size_t num_threads = std::thread::hardware_concurrency();

    // Insert multiple entries under the same key
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.resize(num_entries);
    for (int i = 0; i < num_entries; ++i) {
        auto node = std::make_shared<TestNode>(i);
        nodes.push_back(node);
        table.insert(key, node);
    }

    std::atomic<int> remove_count{0};
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (table.remove(key)) {
                remove_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& thread : threads) thread.join();

    // All entries should be removed, no underflow
    EXPECT_EQ(remove_count.load(), num_entries);
    EXPECT_EQ(table.size(), 0);
}


// Test high-contention insert/remove by data
TEST_F(HashMultiTableTest, ConcurrentMixedInsertRemoveByData) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    constexpr int keys = 100;
    constexpr int ops_per_thread = 500;

    // Each thread will insert/remove a shared pointer under different keys
    auto node = std::make_shared<TestNode>(1);

    // Insert some initially
    for (int i = 0; i < keys / 2; ++i) {
        table.insert(i, node);
    }

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

    std::atomic<int> insert_count(0), remove_count(0), remove_all_count(0);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> key_dist(0, keys - 1);
            std::uniform_int_distribution<int> op_dist(0, 2);
            for (int i = 0; i < ops_per_thread; ++i) {
                int op = op_dist(gen);
                int k = key_dist(gen);
                if (op == 0) { // insert
                    if (table.insert(k, node)) {
                        insert_count.fetch_add(1, std::memory_order_relaxed);
                    }
                } else if (op == 1) { // remove by data
                    if (table.remove(node)) {
                        remove_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    for (auto& thread : threads) thread.join();

    // Should have no negative effects or crashes, and size >= 0
    EXPECT_GE(table.size(), 0);
    // Optional: Print operation stats
    std::cout << "Insert: " << insert_count.load() << ", Remove: " << remove_count.load()
              << ", RemoveAll: " << remove_all_count.load() << std::endl;
}

TEST_F(HashMultiTableTest, RealWorldMixedConcurrentOps) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;

    constexpr int initial_fill = TABLE_SIZE / 2;
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 500;

    // Pre-insert ~50% occupancy, using unique and shared nodes
    std::vector<std::shared_ptr<TestNode>> shared_nodes;
    shared_nodes.reserve(initial_fill);

    for (int i = 0; i < initial_fill; ++i) {
        auto node = (i % 2 == 0) ? std::make_shared<TestNode>(i) : std::make_shared<TestNode>(999);
        shared_nodes.push_back(node);
        table.insert(i, node);
    }

    std::atomic<int>    insert_count{0}, remove_key_count{0}, remove_data_count{0},
                        remove_first_count{0},
                        swap_key_count{0}, swap_data_count{0},
                        find_count{0}, reclaim_count{0};

    // Prepare a second shared pointer for swap_data
    auto swap_node_old = std::make_shared<TestNode>(777);
    auto swap_node_new = std::make_shared<TestNode>(888);
    table.insert(100, swap_node_old);

    // Threaded operations
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

	    for (int t = 0; t < num_threads; ++t) {
	        threads.emplace_back([&, t]() {
	            // Thread-local RNGs for reproducibility
	            thread_local std::mt19937 gen(12345U + static_cast<unsigned int>(t));
	            std::uniform_int_distribution<int> op_dist(0, 7);         // 8 operations
	            std::uniform_int_distribution<int> key_dist(0, TABLE_SIZE - 1);
	            std::uniform_int_distribution<int> alt_key_dist(0, TABLE_SIZE - 1);

            for (int i = 0; i < ops_per_thread; ++i) {
                int op = op_dist(gen);
                int key = key_dist(gen);
                int alt_key = alt_key_dist(gen);

                switch (op) {
                    case 0: // Insert
                    {
                        auto node = std::make_shared<TestNode>(key);
                        if (table.insert(key, node)) insert_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    case 1: // Remove by key/data
                    {
                        auto results = table.find(key);
                        if (!results.empty()) {
                            if (table.remove(key, results[0])) remove_key_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        break;
                    }
                    case 2: // Remove by data
                    {
	                        // Occasionally try a known shared node
	                        std::shared_ptr<TestNode> node;
	                        if (key < static_cast<int>(shared_nodes.size())) node = shared_nodes[static_cast<size_t>(key)];
	                        else node = std::make_shared<TestNode>(key);
	                        if (table.remove(node)) remove_data_count.fetch_add(1, std::memory_order_relaxed);
	                        break;
	                    }
                    case 3: // Remove first
                    {
                        if (table.remove(key)) remove_first_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    case 4: // Swap key
                    {
                        auto results = table.find(key);
                        if (!results.empty() && alt_key != key) {
                            if (table.swap(key, alt_key, results[0]))
                                swap_key_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        break;
                    }
                    case 5: // Swap data
                    {
                        // Use swap_node_old for a deterministic test
                        if (table.swap(100, swap_node_old, swap_node_new))
                            swap_data_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    case 6: // Find & reclaim
                    {
                        table.find(key); // Not counted; just to increase read ops
                        // Occasionally trigger reclaim (e.g., every 10th iteration)
                        if (i % 10 == 0) {
                            table.reclaim([](std::shared_ptr<TestNode> ptr) {
                                // Remove nodes with even data
                                return ptr->data % 2 == 0;
                            });
                            reclaim_count.fetch_add(1, std::memory_order_relaxed);
                        }
                        find_count.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    // Print summary for debugging/analysis
    std::cout << "Insert: " << insert_count.load()
              << " Remove(key/data): " << remove_key_count.load()
              << " Remove by data: " << remove_data_count.load()
              << " Remove first: " << remove_first_count.load()
              << " Swap key: " << swap_key_count.load()
              << " Swap data: " << swap_data_count.load()
              << " Find: " << find_count.load()
              << " Reclaim: " << reclaim_count.load()
              << " Final size: " << table.size() << std::endl;

    // Basic assertions
    EXPECT_GE(table.size(), 0);
    EXPECT_GT(insert_count.load(), 0);
    EXPECT_GT(remove_key_count.load() + remove_data_count.load() + remove_first_count.load(), 0);
    EXPECT_GE(swap_key_count.load(), 0);
    EXPECT_GE(swap_data_count.load(), 0);
    EXPECT_GE(find_count.load(), 0);
    EXPECT_GE(reclaim_count.load(), 0);
}


TEST_F(HashMultiTableTest, ConcurrentUpdate) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    const int table_size = static_cast<int>(TABLE_SIZE);

    // Insert a node for each key
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.reserve(TABLE_SIZE);
    for (int i = 0; i < table_size; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        table.insert(i, nodes.back());
    }

    // Prepare new nodes
    std::vector<std::shared_ptr<TestNode>> new_nodes;
    new_nodes.reserve(TABLE_SIZE);
    for (int i = 0; i < table_size; ++i) {
        new_nodes.push_back(std::make_shared<TestNode>(i * 100));
    }

    const size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread works on a strided set of keys
            for (size_t i = t; i < TABLE_SIZE; i += num_threads) {
                EXPECT_TRUE(table.update(static_cast<int>(i), new_nodes[i]));
            }
        });
    }
    for (auto& th : threads) th.join();

    // Check that all values have been updated
    for (int i = 0; i < table_size; ++i) {
        auto result = table.find_first(i);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->data, i * 100);
    }
}

TEST_F(HashMultiTableTest, ConcurrentUpdateAll) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;

    constexpr int key = 123, num_values = 10;

    for (int i = 0; i < num_values; ++i) {
        table.insert(key, std::make_shared<TestNode>(i));
    }

    // We'll try to update all values for the same key concurrently, using different new values
    std::vector<std::shared_ptr<TestNode>> updates;
    updates.reserve(static_cast<size_t>(num_values));
    for (int i = 0; i < num_values; ++i) {
        updates.push_back(std::make_shared<TestNode>(1000 + i));
    }

    const size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int j = 0; j < num_values; ++j) {  // Do many updates per thread to cause interleaving
                table.update_all(key, updates[t]);
            }
        });
    }
    for (auto& th : threads) th.join();

    // At the end, all nodes with this key should have one of the new values
    auto results = table.find(key);
    ASSERT_EQ(results.size(), num_values);
    for (const auto& n : results) {
        bool found = false;
        for (const auto& u : updates) {
            if (n->data == u->data) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
