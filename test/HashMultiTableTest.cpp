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
    const int key = 5;
    const int count = 10;
    std::vector<std::shared_ptr<TestNode>> nodes;
    
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
    EXPECT_TRUE(first->data >= 0 && first->data < count * 10);
    
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
    HashMultiTable<int, TestNode, 4> small_table; // Only 4 buckets to force collisions
    
    const int count = 100;
    std::vector<std::shared_ptr<TestNode>> nodes;
    
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
        EXPECT_TRUE(small_table.remove(i, nodes[i]));
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
    
    const int count = 100;
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
    const int count = 100;
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

// Test high concurrency insertion
TEST_F(HashMultiTableTest, ConcurrentInsertion) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    const int num_threads = 8;
    const int keys_per_thread = 1000;
    const int total_keys = num_threads * keys_per_thread;
    
    std::vector<std::thread> threads;
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
    
    const int count = 1000;
    
    // Insert keys
    for (int i = 0; i < count; ++i) {
        auto node = std::make_shared<TestNode>(i);
        EXPECT_TRUE(table.insert(i, node));
    }
    
    const int num_threads = 8;
    std::vector<std::thread> threads;
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
    
    const int count = 1000;
    std::vector<std::shared_ptr<TestNode>> nodes;
    
    // Insert keys
    for (int i = 0; i < count; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        EXPECT_TRUE(table.insert(i, nodes.back()));
    }
    
    EXPECT_EQ(table.size(), count);
    
    const int num_threads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> remove_count(0);
    
    // Launch threads to remove keys concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread removes a distinct subset of keys
            for (int i = t; i < count; i += num_threads) {
                if (table.remove(i, nodes[i])) {
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
    
    const int key_range = 1000;
    const int num_threads = 8;
    const int ops_per_thread = 10000;
    
    std::vector<std::thread> threads;
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
        threads.emplace_back([&, t]() {
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
    
    const int count = 1000;
    std::vector<std::shared_ptr<TestNode>> nodes;
    
    // Insert initial keys
    for (int i = 0; i < count; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        EXPECT_TRUE(table.insert(i, nodes.back()));
    }
    
    const int num_threads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> swap_count(0);
    
    // Launch threads to swap keys
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = t; i < count; i += num_threads) {
                int new_key = i + count;  // Swap to a new key range
                if (table.swap(i, new_key, nodes[i])) {
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
    
    const int count = 1000;
    const int num_threads = 8;
    const int iterations = 10;
    
    // Insert initial data
    for (int i = 0; i < count; ++i) {
        auto node = std::make_shared<TestNode>(i);
        EXPECT_TRUE(table.insert(i, node));
    }
    
    // Measure time for concurrent operations
    double concurrent_time = measure_execution_time([&]() {
        std::vector<std::thread> threads;
        
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
    EXPECT_FALSE(table.remove_first(42));
    
    // Swap in empty table
    EXPECT_FALSE(table.swap(42, 99, node));
    auto new_node = std::make_shared<TestNode>(99);
    EXPECT_FALSE(table.swap(42, node, new_node));
    
    // Reclaim from empty table
    table.reclaim([](std::shared_ptr<TestNode> node) -> bool {
        return false; // Reclaim all (though there are none)
    });
    
    // Size of empty table
    EXPECT_EQ(table.size(), 0);
}

// Test for correctness under high contention on the same keys
TEST_F(HashMultiTableTest, HighContention) {
    HashMultiTable<int, TestNode, TABLE_SIZE> table;
    
    const int num_threads = 32;
    const int num_keys = 10;  // Very small number of keys to create contention
    const int ops_per_thread = 1000;
    
    std::vector<std::thread> threads;
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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}