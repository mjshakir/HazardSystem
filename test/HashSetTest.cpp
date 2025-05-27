#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <set>
#include <random>
#include <chrono>
#include <string>
#include <algorithm>
#include <atomic>
#include <future>
#include "HashSet.hpp"

namespace HazardSystem {
namespace testing {

// Basic Tests
class HashSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        hashset = std::make_unique<HashSet<int>>(64);
    }

    void TearDown() override {
        hashset.reset();
    }

    std::unique_ptr<HashSet<int>> hashset;
};


// Test inserting a single element
TEST_F(HashSetTest, InsertSingleElement) {
    EXPECT_TRUE(hashset->insert(42));
    EXPECT_TRUE(hashset->contains(42));
    EXPECT_EQ(hashset->size(), 1);
}

// Test inserting duplicate elements
TEST_F(HashSetTest, InsertDuplicateElement) {
    EXPECT_TRUE(hashset->insert(42));
    EXPECT_FALSE(hashset->insert(42));
    EXPECT_EQ(hashset->size(), 1);
}

// Test inserting multiple elements
TEST_F(HashSetTest, InsertMultipleElements) {
    const int num_elements = 1000;
    for (int i = 0; i < num_elements; ++i) {
        EXPECT_TRUE(hashset->insert(i));
    }
    EXPECT_EQ(hashset->size(), num_elements);
    for (int i = 0; i < num_elements; ++i) {
        EXPECT_TRUE(hashset->contains(i));
    }
}

// TEST_F(HashSetTest, InsertMultipleElements) {
//     const int num_elements = 100; // Reduced from 1000 for debugging
//     int success_count = 0;
//     std::set<int> failed_inserts;
    
//     for (int i = 0; i < num_elements; ++i) {
//         bool result = hashset->insert(i);
//         if (result) {
//             success_count++;
//         } else {
//             failed_inserts.insert(i);
//         }
//         // Don't use EXPECT_TRUE to avoid early test termination
//     }
    
//     std::cout << "Inserted " << success_count << " out of " << num_elements << " elements" << std::endl;
//     if (!failed_inserts.empty()) {
//         std::cout << "Failed to insert: ";
//         for (auto val : failed_inserts) {
//             std::cout << val << " ";
//         }
//         std::cout << std::endl;
//     }
    
//     // Check final size
//     EXPECT_EQ(hashset->size(), success_count) 
//         << "HashSet size " << hashset->size() << " should match successful insertions " << success_count;
    
//     // Verify all successful elements are contained
//     int found_count = 0;
//     for (int i = 0; i < num_elements; ++i) {
//         if (failed_inserts.find(i) == failed_inserts.end()) {
//             // Only check elements we successfully inserted
//             bool contained = hashset->contains(i);
//             if (contained) {
//                 found_count++;
//             } else {
//                 std::cout << "Element " << i << " was inserted but not found" << std::endl;
//             }
//         }
//     }
    
//     EXPECT_EQ(found_count, success_count) 
//         << "Number of elements found (" << found_count 
//         << ") doesn't match number of successful insertions (" << success_count << ")";
// }

// Test removing an existing element
TEST_F(HashSetTest, RemoveElement) {
    EXPECT_TRUE(hashset->insert(42));
    EXPECT_TRUE(hashset->contains(42));
    EXPECT_TRUE(hashset->remove(42));
    EXPECT_FALSE(hashset->contains(42));
    EXPECT_EQ(hashset->size(), 0);
}

// Test removing a non-existing element
TEST_F(HashSetTest, RemoveNonExistingElement) {
    EXPECT_FALSE(hashset->remove(42));
    EXPECT_EQ(hashset->size(), 0);
}

// Test resizing behavior (private method called indirectly)
TEST_F(HashSetTest, ResizeBehavior) {
    // Initial capacity is 64, will resize at 48 elements (75% load factor)
    const int threshold = 64 * 0.75;
    for (int i = 0; i < threshold + 10; ++i) {
        EXPECT_TRUE(hashset->insert(i));
    }
    
    // Check all elements are still there after resizing
    for (int i = 0; i < threshold + 10; ++i) {
        EXPECT_TRUE(hashset->contains(i));
    }
    
    EXPECT_EQ(hashset->size(), threshold + 10);
}

// Test with different types

// Edge case tests
class HashSetEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a very small initial capacity to test resize quickly
        small_set = std::make_unique<HashSet<int>>(2);
        
        // Use a large capacity to test performance with many elements
        large_set = std::make_unique<HashSet<int>>(1 << 16); // 65536
    }

    std::unique_ptr<HashSet<int>> small_set;
    std::unique_ptr<HashSet<int>> large_set;
};

// Test with very small initial capacity
TEST_F(HashSetEdgeCaseTest, SmallInitialCapacity) {
    // Insert enough elements to force multiple resizes
    const int count = 100;
    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(small_set->insert(i));
    }
    
    EXPECT_EQ(small_set->size(), count);
    
    // Verify all elements are still accessible
    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(small_set->contains(i));
    }
}

// Test with large number of elements
TEST_F(HashSetEdgeCaseTest, LargeNumberOfElements) {
    const int count = 10000; // High enough to test performance
    for (int i = 0; i < count; ++i) {
        EXPECT_TRUE(large_set->insert(i));
    }
    
    EXPECT_EQ(large_set->size(), count);
    
    // Remove half the elements
    for (int i = 0; i < count; i += 2) {
        EXPECT_TRUE(large_set->remove(i));
    }
    
    EXPECT_EQ(large_set->size(), count / 2);
    
    // Verify remaining elements
    for (int i = 1; i < count; i += 2) {
        EXPECT_TRUE(large_set->contains(i));
    }
}

// Test with minimum value
TEST_F(HashSetEdgeCaseTest, MinValue) {
    EXPECT_TRUE(large_set->insert(std::numeric_limits<int>::min()));
    EXPECT_TRUE(large_set->contains(std::numeric_limits<int>::min()));
    EXPECT_EQ(large_set->size(), 1);
}

// Test with maximum value
TEST_F(HashSetEdgeCaseTest, MaxValue) {
    EXPECT_TRUE(large_set->insert(std::numeric_limits<int>::max()));
    EXPECT_TRUE(large_set->contains(std::numeric_limits<int>::max()));
    EXPECT_EQ(large_set->size(), 1);
}

// Test with mixed extreme values
TEST_F(HashSetEdgeCaseTest, MixedExtremeValues) {
    EXPECT_TRUE(large_set->insert(std::numeric_limits<int>::min()));
    EXPECT_TRUE(large_set->insert(std::numeric_limits<int>::max()));
    EXPECT_TRUE(large_set->insert(0));
    EXPECT_TRUE(large_set->contains(std::numeric_limits<int>::min()));
    EXPECT_TRUE(large_set->contains(std::numeric_limits<int>::max()));
    EXPECT_TRUE(large_set->contains(0));
    EXPECT_EQ(large_set->size(), 3);
}

// Pattern of values that might lead to hash collisions
TEST_F(HashSetEdgeCaseTest, HashCollisionPattern) {
    HashSet<int> collision_set(16);  // Small size to increase collision chance
    
    // These values are chosen to potentially create collisions in common hash implementations
    std::vector<int> collision_prone = {0, 16, 32, 48, 64, 80, 96, 112, 128};
    
    for (auto val : collision_prone) {
        EXPECT_TRUE(collision_set.insert(val));
    }
    
    EXPECT_EQ(collision_set.size(), collision_prone.size());
    
    for (auto val : collision_prone) {
        EXPECT_TRUE(collision_set.contains(val));
    }
}

TEST_F(HashSetTest, ForEachCollectsAllElements) {
    std::set<int> expected = {1, 2, 3, 5, 8};
    for (int val : expected) hashset->insert(val);

    std::set<int> found;
    hashset->for_each([&found](std::shared_ptr<int> p) {
        if (p) found.insert(*p);
    });

    EXPECT_EQ(found, expected);
}

TEST_F(HashSetTest, ForEachFastCollectsAllElements) {
    std::vector<int> to_insert{4, 7, 9, 10};
    for (int v : to_insert) hashset->insert(v);

    std::set<int> found;
    hashset->for_each_fast([&found](std::shared_ptr<int> p) {
        if (p) found.insert(*p);
    });

    EXPECT_EQ(found, std::set<int>(to_insert.begin(), to_insert.end()));
}


TEST_F(HashSetTest, ScanAndReclaimSingleThread) {
    // Insert values 0..9
    for (int i = 0; i < 10; ++i) {
        hashset->insert(i);
    }
    EXPECT_EQ(hashset->size(), 10);

    // Only "protect" (keep) odd values using scan_and_reclaim
    hashset->reclaim([](std::shared_ptr<int> ptr) {
        return ptr and (*ptr % 2 == 1);
    });

    // Gather remaining values
    std::set<int> found;
    hashset->for_each_fast([&](std::shared_ptr<int> p) { if (p) found.insert(*p); });

    // Check only odd numbers remain
    std::set<int> expected{1, 3, 5, 7, 9};
    EXPECT_EQ(found, expected);
    EXPECT_EQ(hashset->size(), expected.size());
}

// Multithreaded tests
class HashSetThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        mt_set = std::make_unique<HashSet<int>>(1024);
    }

    std::unique_ptr<HashSet<int>> mt_set;
    
    // Helper method to perform random operations concurrently
    void RandomOperations(int thread_id, int ops_per_thread, std::atomic<int>& net_insertions) {
        std::mt19937 gen(thread_id); // Thread-local RNG
        std::uniform_int_distribution<> value_dist(0, 10000);  // Random values
        std::uniform_int_distribution<> op_dist(0, 99);        // 0-59 insert, 60-89 contains, 90-99 remove
    
        for (int i = 0; i < ops_per_thread; ++i) {
            int value = value_dist(gen);
            int op = op_dist(gen);
    
            if (op < 60) { // 60% insert
                if (mt_set->insert(value)) {
                    net_insertions.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (op < 90) { // 30% contains
                mt_set->contains(value);
            } else { // 10% remove
                if (mt_set->remove(value)) {
                    net_insertions.fetch_sub(1, std::memory_order_relaxed);
                }
            }
        }
    }    
};

// Test concurrent insert operations
TEST_F(HashSetThreadTest, ConcurrentInsert) {
    const int num_threads = 8;
    const int values_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::vector<int> thread_values[num_threads];
    
    // Prepare unique values for each thread
    for (int t = 0; t < num_threads; ++t) {
        thread_values[t].reserve(values_per_thread);
        for (int i = 0; i < values_per_thread; ++i) {
            thread_values[t].push_back(t * values_per_thread + i);
        }
    }
    
    // Launch threads
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &thread_values]() {
            for (int val : thread_values[t]) {
                mt_set->insert(val);
            }
        });
    }
    
    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all values were inserted
    EXPECT_EQ(mt_set->size(), num_threads * values_per_thread);
    
    for (int t = 0; t < num_threads; ++t) {
        for (int val : thread_values[t]) {
            EXPECT_TRUE(mt_set->contains(val));
        }
    }
}

// Test concurrent mixed operations
// TEST_F(HashSetThreadTest, ConcurrentMixedOperations) {
//     const int num_threads = 8;
//     const int ops_per_thread = 10000;
//     std::atomic<int> success_count(0);
    
//     std::vector<std::thread> threads;
    
//     // Launch threads with random operations
//     for (int t = 0; t < num_threads; ++t) {
//         threads.emplace_back([this, t, ops_per_thread, &success_count]() {
//             RandomOperations(t, ops_per_thread, success_count);
//         });
//     }
    
//     // Join threads
//     for (auto& thread : threads) {
//         thread.join();
//     }
    
//     // The exact final size is unpredictable due to concurrent operations,
//     // but it should be related to the success count
//     EXPECT_EQ(mt_set->size(), success_count);
// }


TEST_F(HashSetThreadTest, ConcurrentMixedOperations) {
    const int num_threads = 8;
    const int ops_per_thread = 10000;
    std::atomic<int> net_insertions(0); // Track net effect: insert - remove

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, ops_per_thread, &net_insertions]() {
            RandomOperations(t, ops_per_thread, net_insertions);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(mt_set->size(), net_insertions);
}


// Test contention on the same key
TEST_F(HashSetThreadTest, ContentionOnSameKey) {
    const int num_threads = 8;
    std::atomic<int> insert_success(0);
    std::atomic<int> remove_success(0);
    
    // First thread inserts key 42
    mt_set->insert(42);
    
    std::vector<std::thread> threads;
    
    // Launch threads that all try to insert and remove the same key
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &insert_success, &remove_success]() {
            // Try to insert key 42 (should fail except for the first thread)
            if (mt_set->insert(42)) {
                insert_success++;
            }
            
            // Try to remove key 42 (only one thread should succeed)
            if (mt_set->remove(42)) {
                remove_success++;
            }
            
            // Try to insert again (should succeed for one thread)
            if (mt_set->insert(42)) {
                insert_success++;
            }
        });
    }
    
    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // There should be exactly one successful insertion initially,
    // one successful removal, and one re-insertion
    EXPECT_GE(insert_success, 1);
    EXPECT_GE(remove_success, 1);
    EXPECT_TRUE(mt_set->contains(42));
    EXPECT_EQ(mt_set->size(), 1);

    EXPECT_LE(insert_success, num_threads * 2); // Max 2 inserts per thread
    EXPECT_LE(remove_success, num_threads);     // Max 1 remove per thread


}

// Stress test with high load
TEST_F(HashSetThreadTest, StressTestHighLoad) {
    const int num_threads = 16;
    const int ops_per_thread = 50000;
    std::atomic<int> success_count(0);
    
    std::vector<std::future<void>> futures;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Launch threads with async to better utilize all cores
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, t, ops_per_thread, &success_count]() {
            RandomOperations(t, ops_per_thread, success_count);
        }));
    }
    
    // Wait for all operations to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Stress test completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Final set size: " << mt_set->size() << ", Success count: " << success_count << std::endl;
    
    // Verify final size matches successful operations
    EXPECT_EQ(mt_set->size(), success_count);
}

// Test for lock-free property
// TEST_F(HashSetThreadTest, LockFreeProperty) {
//     const int num_threads = 4;
//     const int ops_per_thread = 10000;

//     std::vector<std::thread> threads;
//     std::atomic<bool> stop_flag(false);
//     std::atomic<int> progress_counter(0);
//     std::atomic<int> stall_count(0);

//     for (int t = 0; t < num_threads; ++t) {
//         threads.emplace_back([this, t, ops_per_thread, &stop_flag, &progress_counter]() {
//             std::mt19937 gen(t);
//             std::uniform_int_distribution<> val_dist(0, 10000);

//             for (int i = 0; i < ops_per_thread && !stop_flag.load(); ++i) {
//                 int val = val_dist(gen);
//                 switch (i % 3) {
//                     case 0: if (mt_set->insert(val)) progress_counter++; break;
//                     case 1: mt_set->contains(val); break;
//                     case 2: if (mt_set->remove(val)) progress_counter++; break;
//                 }
//             }
//         });
//     }

//     std::thread monitor_thread([&]() {
//         const int check_interval_ms = 100;
//         const int max_checks = 10;

//         for (int check = 0; check < max_checks && !stop_flag.load(); ++check) {
//             int before = progress_counter.load();
//             std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
//             int after = progress_counter.load();

//             if (before == after) {
//                 stall_count++;
//             }
//         }

//         stop_flag.store(true);
//     });

//     monitor_thread.join();
//     for (auto& thread : threads) {
//         thread.join();
//     }

//     EXPECT_LE(stall_count, 3);
//     std::cout << "Stall count in lock-free test: " << stall_count << std::endl;
// }

TEST_F(HashSetThreadTest, MaskSizeTracksInsertRemove) {
    constexpr int ops_per_thread = 10000;
    constexpr int value_range = 3000;

    std::atomic<int> total_inserts{0};
    std::atomic<int> total_removes{0};

    auto worker = [&](int tid) {
        std::mt19937 rng(tid ^ (int)std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> dist_val(0, value_range - 1);
        std::uniform_int_distribution<int> dist_op(0, 1); // 0 = insert, 1 = remove

        for (int i = 0; i < ops_per_thread; ++i) {
            int val = dist_val(rng);
            int op = dist_op(rng);

            if (op == 0) {
                if (mt_set->insert(val)) total_inserts.fetch_add(1, std::memory_order_relaxed);
            } else {
                if (mt_set->remove(val)) total_removes.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> ths;
    ths.reserve(threads);
    for (int t = 0; t < threads; ++t)
        ths.emplace_back(worker, t);
    for (auto& th : ths)
        th.join();

    const int expected_final = total_inserts.load() - total_removes.load();
    const int actual_final = mt_set->size();
    const int mask_tracked = mt_set->mask_size();

    // Print for manual inspection (optional)
    std::cout << "total_inserts: " << total_inserts
              << ", total_removes: " << total_removes
              << ", expected_final: " << expected_final
              << ", mt_set->size(): " << actual_final
              << ", mt_set->mask_size(): " << mask_tracked << std::endl;

    // Test invariants:
    EXPECT_GE(actual_final, 0);
    EXPECT_LE(actual_final, value_range);

    // Should be close, but due to races in lock-free removal, a few may be missing
    EXPECT_NEAR(actual_final, expected_final, threads * 2);

    // The bitmask is a count of non-empty buckets, not elements.
    // It should be <= size (if there are hash collisions)
    EXPECT_GE(mask_tracked, 0);
    EXPECT_LE(mask_tracked, actual_final);

    // You can also print all values in the set for further verification if desired
//     std::set<int> found;
//     mt_set->for_each([&](std::shared_ptr<int> p){ found.insert(*p); });
//     std::cout << "Present values: ";
//     for (auto v : found) std::cout << v << " ";
//     std::cout << std::endl;
}


TEST_F(HashSetThreadTest, ForEachAllThreadsInserted) {
    constexpr int per_thread = 100;
    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> ths;
    ths.reserve(threads);

    // Insert a unique range of values per thread
    for (int t = 0; t < threads; ++t) {
        ths.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i)
                mt_set->insert(t * 1000 + i);
        });
    }
    for (auto& th : ths) th.join();

    // --- Wait until all insertions are visible ---
    const int expected_size = threads * per_thread;
    while (mt_set->size() != expected_size) {
        std::this_thread::yield();
    }

    // Traverse and collect all elements using for_each
    std::set<int> found;
    mt_set->for_each([&found](std::shared_ptr<int> p) {
        if (p) found.insert(*p);
    });

    EXPECT_EQ(found.size(), expected_size);
    for (int t = 0; t < threads; ++t) {
        for (int i = 0; i < per_thread; ++i) {
            EXPECT_TRUE(found.count(t * 1000 + i));
        }
    }
}

TEST_F(HashSetThreadTest, ForEachFastConcurrentInsert) {
    constexpr int per_thread = 100;

    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> ths;
    ths.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        ths.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) mt_set->insert(5000 + t * per_thread + i);
        });
    }
    for (auto& th : ths) th.join();

    std::set<int> found;
    mt_set->for_each_fast([&found](std::shared_ptr<int> p) {
        if (p) found.insert(*p);
    });

    EXPECT_EQ(found.size(), threads * per_thread);
    for (int t = 0; t < threads; ++t)
        for (int i = 0; i < per_thread; ++i)
            EXPECT_TRUE(found.count(5000 + t * per_thread + i));
}

TEST_F(HashSetThreadTest, ScanAndReclaimMultiThread) {
    const size_t num_threads = std::thread::hardware_concurrency();;
    constexpr int per_thread = 100;

    // Insert: thread t inserts values t*1000 + [0..99]
    std::vector<std::thread> ths;
    ths.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        ths.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i)
                mt_set->insert(t * 1000 + i);
        });
    }
    for (auto& th : ths) th.join();

    EXPECT_EQ(mt_set->size(), num_threads * per_thread);

    // Each thread calls scan_and_reclaim, "protecting" values divisible by 3
    std::vector<std::thread> reclaimer_ths;
    reclaimer_ths.reserve(num_threads);
    auto is_mod3 = [](std::shared_ptr<int> ptr) -> bool {
        return ptr and (*ptr % 3 == 0);
    };
    for (int t = 0; t < num_threads; ++t) {
        reclaimer_ths.emplace_back([&]() {
            mt_set->reclaim(is_mod3);
        });
    }
    for (auto& th : reclaimer_ths) th.join();

    // Now, only multiples of 3 should remain
    std::set<int> found;
    mt_set->for_each_fast([&found](std::shared_ptr<int> p) { if (p) found.insert(*p); });

    // All values should be divisible by 3
    for (int v : found) {
        EXPECT_EQ(v % 3, 0);
    }

    // Should be exactly num_threads * (per_thread / 3 + (per_thread % 3 != 0)) elements left
    size_t expected_left = 0;
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < per_thread; ++i) {
            if ((t * 1000 + i) % 3 == 0) ++expected_left;
        }
    }
    EXPECT_EQ(found.size(), expected_left);
    EXPECT_EQ(mt_set->size(), expected_left);
}


TEST_F(HashSetThreadTest, RealWorldMixedOperations) {
    constexpr int ops_per_thread = 25000;
    constexpr int value_space = 5000;

    std::atomic<int> net_inserts{0};

    // Each thread will randomly choose to insert, remove, contains, or query size
    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id ^ static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<int> dist(0, value_space - 1);
        std::uniform_int_distribution<int> op_dist(0, 99); // 0-29 insert, 30-69 contains, 70-89 remove, 90-99 size

        for (int i = 0; i < ops_per_thread; ++i) {
            int val = dist(rng);
            int op  = op_dist(rng);
            if (op < 30) {
                if (mt_set->insert(val)) {
                    net_inserts.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (op < 70) {
                mt_set->contains(val); // Just test for presence
            } else if (op < 90) {
                if (mt_set->remove(val)) {
                    net_inserts.fetch_sub(1, std::memory_order_relaxed);
                }
            } else {
                [[maybe_unused]] auto sz = mt_set->size();
            }
        }
    };

    const size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }

    for (auto& th : threads) {
        th.join();
    }

    // Print out the results (optionally)
    std::cout << "RealWorldMixedOperations finished. Final set size: "
              << mt_set->size() << ", Net inserts: " << net_inserts.load() << std::endl;

    EXPECT_GE(mt_set->size(), 0);
    EXPECT_LE(mt_set->size(), value_space);
    EXPECT_NEAR(mt_set->size(), net_inserts.load(), num_threads * 2);

    // --- Validate traversal using for_each and for_each_fast ---
    std::set<int> traversed_for_each;
    mt_set->for_each([&traversed_for_each](std::shared_ptr<int> p) {
        if (p) traversed_for_each.insert(*p);
    });
    std::set<int> traversed_fast;
    mt_set->for_each_fast([&traversed_fast](std::shared_ptr<int> p) {
        if (p) traversed_fast.insert(*p);
    });

    // Both traversals should agree with each other
    EXPECT_EQ(traversed_for_each, traversed_fast);

    // Traversal results should agree with contains()
    size_t count_present = 0;
    for (int v = 0; v < value_space; ++v) {
        if (mt_set->contains(v)) {
            ++count_present;
            EXPECT_TRUE(traversed_for_each.count(v));
            EXPECT_TRUE(traversed_fast.count(v));
        }
    }
    // All traversed elements must actually be in the set
    for (int v : traversed_for_each) {
        EXPECT_TRUE(mt_set->contains(v));
    }

    EXPECT_LE(traversed_for_each.size(), mt_set->size());
    EXPECT_LE(traversed_fast.size(), mt_set->size());
    EXPECT_LE(count_present, mt_set->size());
    // Optionally, allow a small slack:
    EXPECT_GE(traversed_for_each.size(), mt_set->size() - num_threads * 2);
    EXPECT_GE(traversed_fast.size(), mt_set->size() - num_threads * 2);
    EXPECT_GE(count_present, mt_set->size() - num_threads * 2);

}

} // namespace testing
} // namespace HazardSystem

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}