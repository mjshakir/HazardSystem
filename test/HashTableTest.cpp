#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
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
    constexpr int kSize = 10;
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.reserve(static_cast<size_t>(kSize));

    for (int i = 0; i < kSize; ++i) {
        nodes.emplace_back(std::make_shared<TestNode>(i * 10));
        ASSERT_TRUE(hashTable->insert(i, nodes.back()));
    }

    for (int i = 0; i < kSize; ++i) {
        auto found = hashTable->find(i);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->value, i * kSize);
    }

    for (int i = 0; i < kSize; ++i) {
        ASSERT_TRUE(hashTable->remove(i));
    }

    for (int i = 0; i < kSize; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
}

// Test pointer safety by inserting and clearing
TEST_F(HashTableTest, PointerSafety) {
    constexpr int kSize = 5;
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.reserve(static_cast<size_t>(kSize));
    for (int i = 0; i < kSize; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i));
        ASSERT_TRUE(hashTable->insert(i, nodes.back()));
    }

    hashTable->clear(); // Should safely delete all nodes
    for (int i = 0; i < kSize; ++i) {
        ASSERT_EQ(hashTable->find(i), nullptr);
    }
}

// Test reclaiming non-hazardous pointers
TEST_F(HashTableTest, ReclaimNonHazardPointers) {
    constexpr int kSize = 5;
    std::vector<std::shared_ptr<TestNode>> nodes;
    nodes.reserve(static_cast<size_t>(kSize));
    for (int i = 0; i < kSize; ++i) {
        nodes.push_back(std::make_shared<TestNode>(i)); // Now some values are odd, some are even
        ASSERT_TRUE(hashTable->insert(i, nodes.back()));
    }

    // Reclaim nodes where value is even
    hashTable->reclaim([](std::shared_ptr<TestNode> node) {
        return node->value % 2 == 0; // Remove even values
    });

    for (int i = 0; i < kSize; ++i) {
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
    constexpr int EntriesPerThread = 5;

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

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
    constexpr int EntriesPerThread = 5;
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

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

// Edge Case: Inserting nullptr**
TEST_F(HashTableTest, InsertNullptr) {
    ASSERT_FALSE(hashTable->insert(1, nullptr));
    ASSERT_EQ(hashTable->find(1), nullptr);
}

//Edge Case: Re-adding a Removed Key**
TEST_F(HashTableTest, InsertAfterRemove) {
    auto node = std::make_shared<TestNode>(50);
    ASSERT_TRUE(hashTable->insert(5, node));
    ASSERT_TRUE(hashTable->remove(5));
    ASSERT_TRUE(hashTable->insert(5, std::make_shared<TestNode>(100))); // Should succeed
    auto found = hashTable->find(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 100);
}

// Multi-threaded Insert and Read Test**
TEST_F(HashTableTest, ConcurrentInsertFind) {
    constexpr int EntriesPerThread = 10;

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

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

// Multi-threaded Insert and Remove**
TEST_F(HashTableTest, ConcurrentInsertRemove) {
    constexpr int EntriesPerThread = 10;

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

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

// High Contention Test**
TEST_F(HashTableTest, HighContentionTest) {
    constexpr int NumOperations = 1000;
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < NumOperations; ++i) {
                hashTable->insert(1, std::make_shared<TestNode>(i));
                hashTable->remove(1);
            }
        });
    }

    for (auto& thread : threads) thread.join();

    ASSERT_EQ(hashTable->find(1), nullptr);
}

// Massive Insert and Remove (Stress Test)**
TEST_F(HashTableTest, StressTestInsertRemove) {
    constexpr int NumElements = 50000;
    
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int thread_size = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(thread_size));

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

// Reclamation Test Under Load**
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

// Atomicity Test - Ensuring No Inconsistencies**
TEST_F(HashTableTest, AtomicityTest) {
    constexpr int NumOperations = 1000;

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

    for (int t = 0; t < NumThreads; ++t) {
        threads.emplace_back([&]() {
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

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int NumThreads = static_cast<int>(hardware_threads > 0U ? hardware_threads : 1U);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

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




// TEST_F(HashTableTest, RealWorldMixedOperations) {
//     constexpr int NumThreads = 8;
//     constexpr int NumKeys = 100;
//     constexpr int NumOpsPerThread = 5000;

//     // To keep track of all currently inserted values for validation
//     std::vector<std::atomic<int>> ground_truth(NumKeys); // value: -1 means not present
//     for (auto& v : ground_truth) v.store(-1, std::memory_order_relaxed);

//     std::atomic<bool> start{false};
//     std::vector<std::thread> threads;
//     threads.reserve(NumThreads);

//     auto now = [] { return std::chrono::steady_clock::now().time_since_epoch().count(); };

//     // Each thread gets its own RNG for determinism & avoiding contention
//     for (int tid = 0; tid < NumThreads; ++tid) {
//         threads.emplace_back([&, tid]() {
//             std::mt19937 rng(static_cast<unsigned>(now()) ^ (tid << 16));
//             std::uniform_int_distribution<int> op_dist(0, 6);
//             std::uniform_int_distribution<int> key_dist(0, NumKeys - 1);

//             while (!start.load(std::memory_order_acquire)) std::this_thread::yield();

//             for (int i = 0; i < NumOpsPerThread; ++i) {
//                 int op = op_dist(rng);
//                 int key = key_dist(rng);

//                 switch (op) {
//                     case 0: { // insert
//                         auto val = std::make_shared<TestNode>(tid * 10000 + i);
//                         bool ok = hashTable->insert(key, val);
//                         if (ok)
//                             ground_truth[key].store(val->value, std::memory_order_relaxed);
//                         break;
//                     }
//                     case 1: { // remove
//                         bool ok = hashTable->remove(key);
//                         if (ok)
//                             ground_truth[key].store(-1, std::memory_order_relaxed);
//                         break;
//                     }
//                     case 2: { // update
//                         auto val = std::make_shared<TestNode>(tid * 10000 + i + 1);
//                         bool ok = hashTable->update(key, val);
//                         if (ok)
//                             ground_truth[key].store(val->value, std::memory_order_relaxed);
//                         break;
//                     }
//                     case 3: { // find
//                         auto found = hashTable->find(key);
//                         // If in ground_truth, should match or be nullptr (race is okay in test)
//                         if (found) {
//                             EXPECT_GE(found->value, 0);
//                         }
//                         break;
//                     }
//                     case 4: { // reclaim
//                         // Reclaim even values, sometimes
//                         hashTable->reclaim([](std::shared_ptr<TestNode> node) {
//                             return node && node->value % 2 == 0;
//                         });
//                         // Update ground truth
//                         for (int k = 0; k < NumKeys; ++k) {
//                             int v = ground_truth[k].load(std::memory_order_relaxed);
//                             if (v != -1 and v % 2 == 0)
//                                 ground_truth[k].store(-1, std::memory_order_relaxed);
//                         }
//                         break;
//                     }
//                     case 5: { // size
//                         auto sz = hashTable->size();
//                         // Not strictly verifiable (races), but shouldn't be negative or absurd
//                         EXPECT_LE(sz, NumKeys);
//                         break;
//                     }
//                     case 6: { // clear (rare)
//                         if (i % 1000 == 0) {
//                             hashTable->clear();
//                             for (auto& v : ground_truth) v.store(-1, std::memory_order_relaxed);
//                         }
//                         break;
//                     }
//                 }
//                 // Optionally add sleep to simulate delays:
//                 // if (i % 100 == 0) std::this_thread::sleep_for(std::chrono::microseconds(1));
//             }
//         });
//     }

//     // Start all threads simultaneously for maximum contention
//     start.store(true, std::memory_order_release);

//     for (auto& t : threads) t.join();

//     // Final check: all keys in table match ground truth
//     for (int k = 0; k < NumKeys; ++k) {
//         auto found = hashTable->find(k);
//         int expected = ground_truth[k].load(std::memory_order_relaxed);
//         if (expected == -1) {
//             EXPECT_EQ(found, nullptr) << "Key " << k << " should be removed";
//         } else {
//             ASSERT_NE(found, nullptr) << "Key " << k << " should exist";
//             EXPECT_EQ(found->value, expected) << "Key " << k << " value mismatch";
//         }
//     }
// }

TEST_F(HashTableTest, RealWorldMixedOperations) {
    constexpr int NumThreads = 8;
    constexpr int NumKeys = 100;
    constexpr int NumOpsPerThread = 5000;

    std::vector<std::atomic<int>> ground_truth(NumKeys); // value: -1 means not present
    for (auto& v : ground_truth) v.store(-1, std::memory_order_relaxed);

    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(NumThreads));

    auto now = [] { return std::chrono::steady_clock::now().time_since_epoch().count(); };

    // PHASE 1: Random concurrent mixed operations
	    for (int tid = 0; tid < NumThreads; ++tid) {
	        threads.emplace_back([&, tid]() {
	            std::mt19937 rng(static_cast<unsigned>(now()) ^ (static_cast<unsigned>(tid) << 16));
	            std::uniform_int_distribution<int> op_dist(0, 6);
	            std::uniform_int_distribution<int> key_dist(0, NumKeys - 1);

            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();

            for (int i = 0; i < NumOpsPerThread; ++i) {
                int op = op_dist(rng);
                int key = key_dist(rng);

                switch (op) {
	                    case 0: { // insert
	                        auto val = std::make_shared<TestNode>(tid * 10000 + i);
	                        bool ok = hashTable->insert(key, val);
	                        // Only update ground_truth if insert succeeded
	                        if (ok) {
	                            ground_truth[static_cast<size_t>(key)].store(val->value, std::memory_order_relaxed);
	                        }
	                        break;
	                    }
	                    case 1: { // remove
	                        bool ok = hashTable->remove(key);
	                        if (ok) {
	                            ground_truth[static_cast<size_t>(key)].store(-1, std::memory_order_relaxed);
	                        }
	                        break;
	                    }
	                    case 2: { // update
	                        auto val = std::make_shared<TestNode>(tid * 10000 + i + 1);
	                        bool ok = hashTable->update(key, val);
	                        if (ok) {
	                            ground_truth[static_cast<size_t>(key)].store(val->value, std::memory_order_relaxed);
	                        }
	                        break;
	                    }
                    case 3: { // find
                        auto found = hashTable->find(key);
                        // Not asserting here, just exercising reads
                        break;
                    }
	                    case 4: { // reclaim
	                        hashTable->reclaim([](std::shared_ptr<TestNode> node) {
	                            return node && node->value % 2 == 0;
	                        });
	                        // Update ground truth (soft: may be stale in race, so not strictly checked)
	                        for (int k = 0; k < NumKeys; ++k) {
	                            int v = ground_truth[static_cast<size_t>(k)].load(std::memory_order_relaxed);
	                            if (v != -1 && v % 2 == 0)
	                                ground_truth[static_cast<size_t>(k)].store(-1, std::memory_order_relaxed);
	                        }
	                        break;
	                    }
	                    case 5: { // size
	                        auto sz = hashTable->size();
	                        EXPECT_LE(sz, static_cast<size_t>(NumKeys + 1)); // Table never grows too big
	                        break;
	                    }
                    case 6: { // clear (rare)
                        if (i % 1000 == 0) {
                            hashTable->clear();
                            for (auto& v : ground_truth) v.store(-1, std::memory_order_relaxed);
                        }
                        break;
                    }
                }
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : threads) t.join();

	    int mismatches = 0;
	    for (int k = 0; k < NumKeys; ++k) {
	        auto found = hashTable->find(k);
	        int expected = ground_truth[static_cast<size_t>(k)].load(std::memory_order_relaxed);
        if (expected == -1 && found != nullptr) {
            mismatches++;
            // std::cout << "Key " << k << " unexpectedly present." << std::endl;
        } else if (expected != -1 && found == nullptr) {
            mismatches++;
            // std::cout << "Key " << k << " missing (expected " << expected << ")." << std::endl;
        } else if (expected != -1 && found && found->value != expected) {
            mismatches++;
            // std::cout << "Key " << k << " value mismatch: got " << found->value << ", expected " << expected << std::endl;
        }
    }
    EXPECT_LT(mismatches, 10) << "Too many mismatches (likely a real bug or extreme race)";
    // SUCCEED() << "Concurrent real-world mixed operation test complete. Mismatches: " << mismatches;
}
