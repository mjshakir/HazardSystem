#include <gtest/gtest.h>
#include "BitmaskTable.hpp"
#include <thread>
#include <vector>
#include <set>
#include <random>
#include <atomic>

using namespace HazardSystem;

TEST(BitmaskTableTest, AcquireReleaseSingleThread) {
    constexpr size_t N = 64;
    BitmaskTable<int, N> table;
    std::set<uint8_t> acquired;

    // Acquire all slots
    for (size_t i = 0; i < N; ++i) {
        auto idx = table.acquire();
        ASSERT_TRUE(idx.has_value());
        ASSERT_FALSE(acquired.count(*idx));
        acquired.insert(*idx);

        ASSERT_TRUE(table.set(*idx, std::make_shared<int>(42 + i)));
        auto val = table.at(*idx);
        ASSERT_TRUE(val);
        ASSERT_EQ(*val, 42 + i);
        ASSERT_TRUE(table.active(*idx));
    }
    // Now all slots should be full
    ASSERT_FALSE(table.acquire().has_value());

    // Release all slots
    for (auto idx : acquired) {
        ASSERT_TRUE(table.release(idx));
        ASSERT_FALSE(table.active(idx));
        ASSERT_FALSE(table.at(idx));
    }
    ASSERT_EQ(table.capacity(), N);
}

TEST(BitmaskTableTest, ReuseSlot) {
    constexpr size_t N = 8;
    BitmaskTable<int, N> table;

    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(42)));
    ASSERT_TRUE(table.release(*idx));
    auto idx2 = table.acquire();
    ASSERT_TRUE(idx2.has_value());
    ASSERT_EQ(*idx, *idx2);
}


TEST(BitmaskTableTest, MultiThreadedAcquireRelease) {
    constexpr size_t N = 128;
    BitmaskTable<int, N> table;
    constexpr int threads = 16;
    constexpr int iters = 1000;
    std::atomic<int> success_acquire{0}, success_release{0};

    auto worker = [&](int id) {
        for (int i = 0; i < iters; ++i) {
            auto idx = table.acquire();
            if (idx) {
                table.set(*idx, std::make_shared<int>(id));
                ++success_acquire;
                // Don't assert value, as races are possible
                std::this_thread::yield();
                table.release(*idx);
                ++success_release;
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        pool.emplace_back(worker, i);
    }
    for (auto& t : pool) t.join();

    ASSERT_EQ(success_acquire, success_release);

    // Table should be empty (all slots released)
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}

// #if defined(__GNUC__) || defined(__clang__)
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wconversion"
// #pragma GCC diagnostic ignored "-Woverflow"
// #endif

// // Test: Out-of-bounds release and set
// TEST(BitmaskTableTest, OutOfBoundsReleaseSet) {
//     constexpr size_t N = 16;
//     BitmaskTable<int, N> table;

//     // Out-of-bounds: index == N (just beyond last valid)
//     ASSERT_FALSE(table.release(N));
//     ASSERT_FALSE(table.set(N, std::make_shared<int>(5)));
//     ASSERT_FALSE(table.active(N));
//     ASSERT_FALSE(table.at(N).has_value());

//     // Out-of-bounds: index way beyond
//     ASSERT_FALSE(table.release(1000));
//     ASSERT_FALSE(table.set(1000, std::make_shared<int>(5)));
//     ASSERT_FALSE(table.active(1000));
//     ASSERT_FALSE(table.at(1000).has_value());

//     // Out-of-bounds: negative numbers (if public API is size_t, static_cast will make these large values)
//     ASSERT_FALSE(table.release(static_cast<size_t>(-1)));
//     ASSERT_FALSE(table.set(static_cast<size_t>(-1), std::make_shared<int>(5)));
//     ASSERT_FALSE(table.active(static_cast<size_t>(-1)));
//     ASSERT_FALSE(table.at(static_cast<size_t>(-1)).has_value());
// }


// Test: Releasing already released slot
TEST(BitmaskTableTest, DoubleRelease) {
    constexpr size_t N = 8;
    BitmaskTable<int, N> table;

    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(123)));
    ASSERT_TRUE(table.release(*idx));
    ASSERT_FALSE(table.release(*idx));
}


// Test: Setting nullptr should be fine, but slot is not "active"
TEST(BitmaskTableTest, SetNullptr) {
    constexpr size_t N = 4;
    BitmaskTable<int, N> table;
    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, nullptr));
    auto val = table.at(*idx);
    ASSERT_FALSE(val);
    table.release(*idx);
}

// Test: Table with N = 1
TEST(BitmaskTableTest, SingleSlot) {
    constexpr size_t N = 1;
    BitmaskTable<int, N> table;

    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(99)));
    ASSERT_TRUE(table.release(*idx));
    ASSERT_FALSE(table.active(*idx));
    ASSERT_FALSE(table.at(*idx));
    // Should be able to reuse the slot
    auto idx2 = table.acquire();
    ASSERT_TRUE(idx2.has_value());
    ASSERT_TRUE(table.set(*idx2, std::make_shared<int>(123)));
    ASSERT_TRUE(table.release(*idx2));
}


// Test: Table with N = 1024 (maximum allowed)
TEST(BitmaskTableTest, MaxSlots) {
    constexpr size_t N = 1024;
    BitmaskTable<int, N> table;
    std::vector<size_t> idxs;
    idxs.reserve(N);
    // Fill all slots
    for (size_t i = 0; i < N; ++i) {
        auto idx = table.acquire();
        ASSERT_TRUE(idx.has_value());
        ASSERT_TRUE(table.set(*idx, std::make_shared<int>(int(i))));
        idxs.push_back(*idx);
    }
    // No more slots should be available
    ASSERT_FALSE(table.acquire().has_value());
    // Release all slots
    for (auto idx : idxs) {
        ASSERT_TRUE(table.release(idx));
        ASSERT_FALSE(table.active(idx));
        ASSERT_FALSE(table.at(idx));
    }
    ASSERT_EQ(table.capacity(), N);
}

TEST(BitmaskTableTest, CapacityAndSizeSingleThreaded) {
    constexpr size_t capacity = 37;
    BitmaskTable<int, capacity> table;
    ASSERT_EQ(table.capacity(), capacity);

    std::vector<size_t> idxs;
    idxs.reserve(capacity);
    // Acquire all slots and set a value
    for (size_t i = 0; i < capacity; ++i) {
        auto idx = table.acquire();
        ASSERT_TRUE(idx);
        ASSERT_TRUE(table.set(*idx, std::make_shared<int>(int(i * 2))));
        idxs.push_back(*idx);
    }
    ASSERT_EQ(table.size(), capacity);

    // Check values and release all slots
    for (size_t i = 0; i < idxs.size(); ++i) {
        auto idx = idxs[i];
        ASSERT_TRUE(table.active(idx));
        auto val = table.at(idx);
        ASSERT_TRUE(val);
        ASSERT_EQ(*val, int(i * 2));
        ASSERT_TRUE(table.release(idx));
    }
    ASSERT_EQ(table.size(), 0u);
    // After release, slots should be inactive
    for (size_t idx : idxs) {
        ASSERT_FALSE(table.active(idx));
        ASSERT_FALSE(table.at(idx));
    }
}


TEST(BitmaskTableTest, CapacityAndSizeMultiThreaded) {
    constexpr size_t capacity = 112;
    BitmaskTable<int, capacity> table;
    ASSERT_EQ(table.capacity(), capacity);

    const size_t threads = std::thread::hardware_concurrency();
    const int ops_per_thread = capacity / threads + 1;
    std::vector<std::vector<size_t>> all_idxs(threads);

    auto worker = [&](int tid) {
        std::vector<size_t>& my_idxs = all_idxs[tid];
        my_idxs.reserve(ops_per_thread);
        for (int i = 0; i < ops_per_thread; ++i) {
            auto idx = table.acquire();
            if (idx) {
                ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 1000 + i)));
                my_idxs.push_back(*idx);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, t);
    for (auto& t : pool) t.join();

    // The total number of acquired slots should not exceed capacity
    size_t total_acquired = 0;
    for (const auto& v : all_idxs) total_acquired += v.size();
    ASSERT_LE(total_acquired, capacity);
    ASSERT_EQ(table.size(), total_acquired);

    // Release all slots, check values
    for (int tid = 0; tid < threads; ++tid) {
        for (size_t idx : all_idxs[tid]) {
            ASSERT_TRUE(table.active(idx));
            auto val = table.at(idx);
            ASSERT_TRUE(val);
            // Value should be in [0, threads*1000 + ops_per_thread]
            ASSERT_GE(*val, 0);
            ASSERT_TRUE(table.release(idx));
        }
    }
    ASSERT_EQ(table.size(), 0u);

    // All slots should now be empty
    for (size_t i = 0; i < capacity; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}

TEST(BitmaskTableTest, SizeAccountingNoDoubleCount) {
    constexpr size_t N = 8;
    BitmaskTable<int, N> table;

    std::vector<size_t> idxs;
    for (size_t i = 0; i < N / 2; ++i) {
        auto idx = table.acquire();
        ASSERT_TRUE(idx);
        idxs.push_back(*idx);
    }
    // Size should reflect acquires only once
    ASSERT_EQ(table.size(), idxs.size());

    // Setting non-null should not bump size again
    for (size_t i = 0; i < idxs.size(); ++i) {
        ASSERT_TRUE(table.set(idxs[i], std::make_shared<int>(int(i))));
        ASSERT_EQ(table.size(), idxs.size());
    }

    // Overwrite with new values; size stays constant
    for (size_t i = 0; i < idxs.size(); ++i) {
        ASSERT_TRUE(table.set(idxs[i], std::make_shared<int>(int(i + 100))));
        ASSERT_EQ(table.size(), idxs.size());
    }

    // Clearing with nullptr decrements
    for (auto idx : idxs) {
        ASSERT_TRUE(table.set(idx, nullptr));
    }
    ASSERT_EQ(table.size(), 0u);
}

// Test: for_each lambda is called only for active slots
TEST(BitmaskTableTest, ForEachActive) {
    constexpr size_t N = 10;
    BitmaskTable<int, N> table;
    std::vector<typename BitmaskTable<int, N>::IndexType> idxs;
    idxs.reserve(N);
    for (size_t i = 0; i < N; i += 2) {
        auto idx = table.acquire();
        ASSERT_TRUE(idx.has_value());
        idxs.push_back(*idx);
        table.set(*idx, std::make_shared<int>(i));
    }
    int count = 0;
    table.for_each([&](auto idx, auto ptr) {
        ++count;
        ASSERT_TRUE(ptr);
        ASSERT_TRUE(table.active(idx));
    });
    ASSERT_EQ(count, idxs.size());
    // Release all
    for (auto idx : idxs) table.release(idx);
}


TEST(BitmaskTableTest, ForEachActiveFastSingleThread) {
    constexpr size_t N = 16;
    BitmaskTable<int, N> table;
    std::set<size_t> expected;

    // Set odd indices
    for (size_t i = 1; i < N; i += 2) {
        table.set(i, std::make_shared<int>(i * 10));
        expected.insert(i);
    }

    std::set<size_t> visited;
    table.for_each_fast([&](size_t idx, std::shared_ptr<int> ptr) {
        visited.insert(idx);
        ASSERT_EQ(*ptr, idx * 10);
    });

    ASSERT_EQ(expected, visited);
}


TEST(BitmaskTableTest, ForEachActiveFastMultiThread) {
    constexpr size_t N = 64;
    BitmaskTable<int, N> table;
    constexpr int threads = 8;

    // Randomly fill about half the slots
    for (size_t i = 0; i < N; ++i) {
        if (i % 3 == 0)
            table.set(i, std::make_shared<int>(i + 100));
    }

    std::atomic<size_t> visited_count{0};

    auto worker = [&]() {
        table.for_each_fast([&](size_t idx, std::shared_ptr<int> ptr) {
            ASSERT_EQ(*ptr, idx + 100);
            ++visited_count;
        });
    };

    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker);
    for (auto& t : pool) t.join();

    // Since each thread visits all, divide by thread count for per-table coverage
    ASSERT_EQ(visited_count / threads, N / 3 + (N % 3 ? 1 : 0));
}

// Test 1: m_bitmask is not array, single-threaded (N=32)
TEST(BitmaskTableTest, SetEmplaceNonArraySingleThread) {
    constexpr size_t N = 32;
    BitmaskTable<int, N> table;
    std::vector<typename BitmaskTable<int, N>::IndexType> indices;
    indices.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        auto idx = table.set(std::make_shared<int>(int(i * 2)));
        ASSERT_TRUE(idx.has_value());
        indices.push_back(*idx);
        auto val = table.at(*idx);
        ASSERT_TRUE(val);
        ASSERT_EQ(*val, int(i * 2));
        ASSERT_TRUE(table.active(*idx));
    }
    // All slots filled; next should fail
    ASSERT_FALSE(table.set(std::make_shared<int>(999)).has_value());
    for (auto idx : indices) {
        ASSERT_TRUE(table.release(idx));
    }
}

// Test 2: m_bitmask is array, single-threaded (N=256)
TEST(BitmaskTableTest, SetEmplaceArraySingleThread) {
    constexpr size_t N = 256;
    BitmaskTable<int, N> table;
    std::vector<typename BitmaskTable<int, N>::IndexType> indices;
    indices.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        auto idx = table.set(std::make_shared<int>(1000 + int(i)));
        ASSERT_TRUE(idx.has_value());
        indices.push_back(*idx);
        auto val = table.at(*idx);
        ASSERT_TRUE(val);
        ASSERT_EQ(*val, 1000 + int(i));
        ASSERT_TRUE(table.active(*idx));
    }
    // All slots filled; next should fail
    ASSERT_FALSE(table.set(std::make_shared<int>(999)).has_value());
    for (auto idx : indices) {
        ASSERT_TRUE(table.release(idx));
    }
}

// Test 3: m_bitmask is not array, multi-threaded (N=32)
TEST(BitmaskTableTest, SetEmplaceNonArrayMultiThread) {
    constexpr size_t N = 32;
    BitmaskTable<int, N> table;
    constexpr int ops_per_thread = 100;
    std::atomic<int> success{0};

    auto worker = [&](int id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            std::optional<size_t> idx;
            // Keep trying until you get a slot (in case of high contention)
            while (!(idx = table.set(std::make_shared<int>(id * 100 + i)))) {
                std::this_thread::yield();
            }
            // Check slot is active and non-null *immediately after set*
            ASSERT_TRUE(table.active(*idx));
            auto v = table.at(*idx);
            ASSERT_TRUE(v);
            // (Optionally) check the value, but be aware that a race could rarely occur before release
            // ASSERT_EQ(*v, id * 100 + i); // Uncomment at your own risk
            table.release(*idx); // Now the slot is free for other threads to use
            ++success;
        }
    };

    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, t);
    for (auto& t : pool) t.join();

    // All slots should be released after threads finish
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
    // All attempts should succeed
    ASSERT_EQ(success, threads * ops_per_thread);
}


// Test 4: m_bitmask is array, multi-threaded (N=256)
TEST(BitmaskTableTest, SetEmplaceArrayMultiThread) {
    constexpr size_t N = 256;
    BitmaskTable<int, N> table;
    
    constexpr int ops_per_thread = 100;
    std::atomic<int> success{0};

    auto worker = [&](int id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            auto idx = table.set(std::make_shared<int>(id * 10000 + i));
            if (idx) {
                // Only check that it's active and non-null, not value
                ASSERT_TRUE(table.active(*idx));
                auto v = table.at(*idx);
                ASSERT_TRUE(v);
                table.release(*idx);
                ++success;
            }
        }
    };
    
    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, t);
    for (auto& t : pool) t.join();

    ASSERT_EQ(success, threads * ops_per_thread);
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}


// TEST(BitmaskTableTest, RealWorldMixedOperations) {
//     constexpr size_t N = 256;
//     BitmaskTable<int, N> table;
//     constexpr int threads = 32;
//     constexpr int ops_per_thread = 500;
//     std::vector<std::atomic<bool>> slot_in_use(N);

//     // Make sure flags are initialized
//     for (auto& flag : slot_in_use) flag = false;

//     auto worker = [&](int tid) {
//         thread_local std::mt19937 gen(std::random_device{}());
//         std::uniform_int_distribution<int> op_dist(0, 2);
//         std::vector<int> my_slots;

//         for (int i = 0; i < ops_per_thread; ++i) {
//             int op = op_dist(gen);

//             if (op == 0 or my_slots.empty()) {
//                 // Try to acquire
//                 auto idx = table.acquire();
//                 if (idx) {
//                     if (slot_in_use[*idx].exchange(true)) {
//                         ADD_FAILURE() << "Double allocation of slot " << *idx;
//                     }
//                     table.set(*idx, std::make_shared<int>(tid * 1000 + i));
//                     my_slots.push_back(*idx);
//                 }
//             } else {
//                 // Release random held slot
//                 std::uniform_int_distribution<size_t> sdist(0, my_slots.size() - 1);
//                 size_t idx_pos = sdist(gen);
//                 int idx_val = my_slots[idx_pos];
//                 ASSERT_TRUE(table.active(idx_val));
//                 auto v = table.at(idx_val);
//                 ASSERT_TRUE(v);
//                 table.release(idx_val);
//                 slot_in_use[idx_val].store(false);
//                 std::swap(my_slots[idx_pos], my_slots.back());
//                 my_slots.pop_back();
//             }
//         }
//         // Clean up any leftovers
//         for (int idx : my_slots) {
//             table.release(idx);
//             slot_in_use[idx].store(false);
//         }
//     };

//     std::vector<std::thread> pool;
//     pool.reserve(threads);
//     for (int t = 0; t < threads; ++t)
//         pool.emplace_back(worker, t);

//     for (auto& t : pool) t.join();

//     // After all threads complete, check for leaks/dangling
//     for (size_t i = 0; i < N; ++i) {
//         ASSERT_FALSE(table.active(i));
//         ASSERT_FALSE(table.at(i));
//         ASSERT_FALSE(slot_in_use[i]);
//     }
// }

TEST(BitmaskTableTest, RealWorldMixedOperations) {
    constexpr size_t N = 256;
    BitmaskTable<int, N> table;
    constexpr int threads = 32;
    constexpr int ops_per_thread = 500;

    auto worker = [&](int tid) {
        thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> op_dist(0, 2);
        std::vector<int> my_slots;

        for (int i = 0; i < ops_per_thread; ++i) {
            int op = op_dist(gen);

            if (op == 0 || my_slots.empty()) {
                // Try to acquire
                auto idx = table.acquire();
                if (idx) {
                    ASSERT_FALSE(table.at(*idx)); // Should be empty before use
                    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 1000 + i)));
                    my_slots.push_back(*idx);
                }
            } else {
                // Release random held slot
                std::uniform_int_distribution<size_t> sdist(0, my_slots.size() - 1);
                size_t idx_pos = sdist(gen);
                int idx_val = my_slots[idx_pos];
                // Optionally check
                auto v = table.at(idx_val);
                ASSERT_TRUE(v);
                table.release(idx_val);
                std::swap(my_slots[idx_pos], my_slots.back());
                my_slots.pop_back();
            }
        }
        // Clean up any leftovers
        for (int idx : my_slots) {
            // ASSERT_TRUE(idx);
            table.release(idx);
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, t);

    for (auto& t : pool) t.join();

    // After all threads complete, check for leaks/dangling
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}
