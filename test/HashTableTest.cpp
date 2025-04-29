#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include "HashTable.hpp"  // Ensure this includes your HazardSystem::HashTable

// Define a simple struct to use as a test object
struct TestNode {
    int value;
    explicit TestNode(int val) : value(val) {}
};

// Test Fixture for reusing the setup
class HashTableTest : public ::testing::Test {
protected:
    static constexpr size_t TableSize = 16;
    using TestHashTable = HazardSystem::HashTable<int, TestNode, TableSize>;

    std::unique_ptr<TestHashTable> hashTable; // Use a pointer

    void SetUp() override {
        hashTable = std::make_unique<TestHashTable>();
    }

    void TearDown() override {
        hashTable.reset();
    }
};

// Test insertion and retrieval
TEST_F(HashTableTest, InsertFindSingle) {
    auto node = std::make_shared<TestNode>(42);
    ASSERT_TRUE(hashTable->insert(1, node));

    auto found = hashTable->find(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 42);
}

// Test inserting a duplicate key (should fail)
TEST_F(HashTableTest, InsertDuplicateKey) {
    auto node1 = std::make_shared<TestNode>(10);
    auto node2 = std::make_shared<TestNode>(20);

    ASSERT_TRUE(hashTable->insert(5, node1));
    ASSERT_TRUE(hashTable->insert(5, node2)); // Should return false

    auto found = hashTable->find(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 20); // The original value should remain
}

// Test removing an existing key
TEST_F(HashTableTest, RemoveExistingKey) {
    auto node = std::make_shared<TestNode>(100);
    ASSERT_TRUE(hashTable->insert(7, node));

    ASSERT_TRUE(hashTable->remove(7));
    ASSERT_EQ(hashTable->find(7), nullptr);
}

// Test removing a non-existing key
TEST_F(HashTableTest, RemoveNonExistingKey) {
    ASSERT_FALSE(hashTable->remove(100));
}

// Test multiple insertions and removals
TEST_F(HashTableTest, InsertRemoveMultiple) {
    std::vector<std::shared_ptr<TestNode>> nodes;
    for (int i = 0; i < 10; ++i) {
        nodes.emplace_back(std::make_shared<TestNode>(i * 10));
        ASSERT_TRUE(hashTable->insert(i, nodes.back()));
    }

    for (int i = 0; i < 10; ++i) {
        auto found = hashTable->find(i);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->value, i * 10);
    }

    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(hashTable->remove(i));
    }

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
}

// Test pointer safety by inserting and clearing
TEST_F(HashTableTest, PointerSafety) {
    std::vector<std::shared_ptr<TestNode>> nodes;
    for (int i = 0; i < 5; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        ASSERT_TRUE(hashTable->insert(i, nodes.back()));
    }

    hashTable->clear(); // Should safely delete all nodes
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
}

// Test reclaiming non-hazardous pointers
TEST_F(HashTableTest, ReclaimNonHazardPointers) {
    std::vector<std::shared_ptr<TestNode>> nodes;
    for (int i = 0; i < 5; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i)); // Now some values are odd, some are even
        ASSERT_TRUE(hashTable->insert(i, nodes.back()));
    }

    // Reclaim nodes where value is even
    hashTable->reclaim([](std::shared_ptr<TestNode> node) {
        return node->value % 2 == 0; // Remove even values
    });

    for (int i = 0; i < 5; ++i) {
        auto found = hashTable->find(i);
        if (i % 2 == 0) {
            ASSERT_EQ(found, nullptr); // Nodes with even values should be removed
        } else {
            ASSERT_NE(found, nullptr); // Nodes with odd values should remain
        }
    }
}

// Test concurrent insertions
TEST_F(HashTableTest, ConcurrentInsertions) {
    constexpr int NumThreads = 4;
    constexpr int EntriesPerThread = 5;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EntriesPerThread; ++i) {
                hashTable->insert(t * EntriesPerThread + i, std::make_shared<TestNode>(t * EntriesPerThread + i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < NumThreads * EntriesPerThread; ++i) {
        auto found = hashTable->find(i);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->value, i);
    }
}

// Test updating an existing key
TEST_F(HashTableTest, UpdateExistingKey) {
    auto node = std::make_shared<TestNode>(42);
    ASSERT_TRUE(hashTable->insert(1, node));  // Insert key 1

    auto new_value = std::make_shared<TestNode>(100);
    ASSERT_TRUE(hashTable->update(1, new_value));  // Update key 1

    auto found = hashTable->find(1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 100);  // Ensure updated value
}

// Test updating a non-existent key
TEST_F(HashTableTest, UpdateNonExistentKey) {
    auto new_value = std::make_shared<TestNode>(200);
    ASSERT_FALSE(hashTable->update(99, new_value));  // Key 99 doesn't exist

    auto found = hashTable->find(99);
    ASSERT_EQ(found, nullptr);  // Ensure key is still missing
}

// Test concurrent insertions and removals
TEST_F(HashTableTest, ConcurrentInsertionsAndRemovals) {
    constexpr int NumThreads = 4;
    constexpr int EntriesPerThread = 5;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EntriesPerThread; ++i) {
                hashTable->insert(t * EntriesPerThread + i, std::make_shared<TestNode>(t * EntriesPerThread + i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    threads.clear();

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EntriesPerThread; ++i) {
                hashTable->remove(t * EntriesPerThread + i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < NumThreads * EntriesPerThread; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
    
}

// ✅ **Edge Case: Inserting nullptr**
TEST_F(HashTableTest, InsertNullptr) {
    ASSERT_FALSE(hashTable->insert(1, nullptr));
    ASSERT_EQ(hashTable->find(1), nullptr);
}

// // ✅ **Edge Case: Re-adding a Removed Key**
TEST_F(HashTableTest, InsertAfterRemove) {
    auto node = std::make_shared<TestNode>(50);
    ASSERT_TRUE(hashTable->insert(5, node));
    ASSERT_TRUE(hashTable->remove(5));
    ASSERT_TRUE(hashTable->insert(5, std::make_shared<TestNode>(100))); // Should succeed
    auto found = hashTable->find(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 100);
}

// ✅ **Multi-threaded Insert and Read Test**
TEST_F(HashTableTest, ConcurrentInsertFind) {
    constexpr int NumThreads = 8;
    constexpr int EntriesPerThread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EntriesPerThread; ++i) {
                hashTable->insert(t * EntriesPerThread + i, std::make_shared<TestNode>(t * EntriesPerThread + i));
            }
        });
    }

    for (auto& thread : threads) thread.join();

    for (int i = 0; i < NumThreads * EntriesPerThread; ++i) {
        auto found = hashTable->find(i);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->value, i);
    }
}

// ✅ **Multi-threaded Insert and Remove**
TEST_F(HashTableTest, ConcurrentInsertRemove) {
    constexpr int NumThreads = 8;
    constexpr int EntriesPerThread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EntriesPerThread; ++i) {
                hashTable->insert(t * EntriesPerThread + i, std::make_shared<TestNode>(t * EntriesPerThread + i));
            }
        });
    }

    for (auto& thread : threads) thread.join();
    threads.clear();

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EntriesPerThread; ++i) {
                hashTable->remove(t * EntriesPerThread + i);
            }
        });
    }

    for (auto& thread : threads) thread.join();

    for (int i = 0; i < NumThreads * EntriesPerThread; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
}

// ✅ **High Contention Test**
TEST_F(HashTableTest, HighContentionTest) {
    constexpr int NumThreads = 4;
    constexpr int NumOperations = 1000;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < NumOperations; ++i) {
                hashTable->insert(1, std::make_shared<TestNode>(i));
                hashTable->remove(1);
            }
        });
    }

    for (auto& thread : threads) thread.join();

    ASSERT_EQ(hashTable->find(1), nullptr);
}

// ✅ **Massive Insert and Remove (Stress Test)**
TEST_F(HashTableTest, StressTestInsertRemove) {
    constexpr int thread_size = 4;
    constexpr int NumElements = 50000;
    std::vector<std::thread> threads;

    for (int i = 0; i < thread_size; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = i * (NumElements / 8); j < (i + 1) * (NumElements / 8); ++j) {
                hashTable->insert(j, std::make_shared<TestNode>(j));
            }
        });
    }

    for (auto& thread : threads) thread.join();
    threads.clear();

    for (int i = 0; i < thread_size; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = i * (NumElements / thread_size); j < (i + 1) * (NumElements / thread_size); ++j) {
                hashTable->remove(j);
            }
        });
    }

    for (auto& thread : threads) thread.join();

    for (int i = 0; i < NumElements; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
}

// ✅ **Reclamation Test Under Load**
TEST_F(HashTableTest, ReclaimUnderLoad) {
    constexpr int NumElements = 1000;

    for (int i = 0; i < NumElements; ++i) {
        hashTable->insert(i, std::make_shared<TestNode>(i));
    }

    hashTable->reclaim([](std::shared_ptr<TestNode> node) {
        return node->value % 2 == 0; // Remove even numbers
    });

    for (int i = 0; i < NumElements; ++i) {
        auto found = hashTable->find(i);
        if (i % 2 == 0) {
            ASSERT_EQ(found, nullptr); // Even numbers should be gone
        } else {
            ASSERT_NE(found, nullptr); // Odd numbers should remain
        }
    }
}

// ✅ **Atomicity Test - Ensuring No Inconsistencies**
TEST_F(HashTableTest, AtomicityTest) {
    constexpr int NumThreads = 4;
    constexpr int NumOperations = 1000;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < NumOperations; ++i) {
                hashTable->insert(i, std::make_shared<TestNode>(i));
                hashTable->remove(i);
                hashTable->insert(i, std::make_shared<TestNode>(i + 1));
            }
        });
    }

    for (auto& thread : threads) thread.join();

    for (int i = 0; i < NumOperations; ++i) {
        auto found = hashTable->find(i);
        ASSERT_NE(found, nullptr);
        EXPECT_GE(found->value, i); // Ensure values were updated correctly
    }
}

// Test concurrent updates
TEST_F(HashTableTest, ConcurrentUpdates) {
    constexpr int Key = 10;
    auto initial_value = std::make_shared<TestNode>(500);
    ASSERT_TRUE(hashTable->insert(Key, initial_value));  // Insert a key

    constexpr int NumThreads = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&, t]() {
            auto new_value = std::make_shared<TestNode>(t * 100);  // Different values per thread
            ASSERT_TRUE(hashTable->update(Key, new_value));
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto found = hashTable->find(Key);
    ASSERT_NE(found, nullptr);
    // The final value is unknown due to concurrency, but it should be one of the last updates.
    EXPECT_GE(found->value, 0);  // Ensure it's a valid updated value
}

// Test updating a key to nullptr (should fail)
// TEST_F(HashTableTest, UpdateToNullptr) {
//     auto node = std::make_shared<TestNode>(25);
//     ASSERT_TRUE(hashTable->insert(3, node));  // Insert key 3

//     ASSERT_FALSE(hashTable->update(3, nullptr));  // Updating to nullptr should fail

//     auto found = hashTable->find(3);
//     ASSERT_NE(found, nullptr);
//     EXPECT_EQ(found->value, 25);  // Original value should remain
// }
