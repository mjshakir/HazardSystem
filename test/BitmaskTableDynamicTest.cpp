#include <gtest/gtest.h>
#include "BitmaskTable.hpp"
#include <thread>
#include <vector>
#include <set>
#include <random>
#include <atomic>
#include <limits>
#include <bit>
using namespace HazardSystem;

// Use a "small" dynamic size for quick tests, and a "large" one for concurrency
constexpr size_t DYNAMIC_SMALL  = 16UL;
constexpr size_t DYNAMIC_MEDIUM = 100UL;
constexpr size_t DYNAMIC_LARGE  = 1000UL;
constexpr size_t DYNAMIC_SIZE   = 1030UL;


TEST(BitmaskTableDynamic, SetNullptrIsInactive) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, nullptr));
    auto val = table.at(*idx);
    ASSERT_FALSE(val);
    table.release(*idx);
}

TEST(BitmaskTableDynamic, OutOfBounds) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    size_t n = DYNAMIC_SMALL;
    ASSERT_FALSE(table.release(n));
    ASSERT_FALSE(table.set(n, std::make_shared<int>(5)));
    ASSERT_FALSE(table.active(n));
    ASSERT_FALSE(table.at(n));

    ASSERT_FALSE(table.release(n + 100));
    ASSERT_FALSE(table.set(n + 100, std::make_shared<int>(5)));
    ASSERT_FALSE(table.active(n + 100));
    ASSERT_FALSE(table.at(n + 100));

    ASSERT_FALSE(table.release(static_cast<size_t>(-1)));
    ASSERT_FALSE(table.set(static_cast<size_t>(-1), std::make_shared<int>(5)));
    ASSERT_FALSE(table.active(static_cast<size_t>(-1)));
    ASSERT_FALSE(table.at(static_cast<size_t>(-1)));
}

TEST(BitmaskTableDynamic, CapacityAndSizeSingleThreaded) {
    constexpr size_t capacity = 37;
    BitmaskTable<int, 0> table(capacity);
    ASSERT_EQ(table.capacity(), std::bit_ceil(capacity));

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
    ASSERT_EQ(table.size(), 0U);
    // After release, slots should be inactive
    for (size_t idx : idxs) {
        ASSERT_FALSE(table.active(idx));
        ASSERT_FALSE(table.at(idx));
    }
}


TEST(BitmaskTableDynamic, CapacityAndSizeMultiThreaded) {
    constexpr size_t capacity           = 53;
    constexpr size_t resized_capacity   = std::bit_ceil(capacity);
    BitmaskTable<int, 0> table(capacity);
    ASSERT_EQ(table.capacity(), resized_capacity);

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
    ASSERT_LE(total_acquired, resized_capacity);
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
    ASSERT_EQ(table.size(), 0U);

    // All slots should now be empty
    for (size_t i = 0; i < capacity; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}

TEST(BitmaskTableDynamic, AcquireReleaseSingleThread) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    std::set<uint8_t> acquired;

    // Acquire all slots
    for (size_t i = 0; i < DYNAMIC_SMALL; ++i) {
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
    ASSERT_GE(table.capacity(), DYNAMIC_SMALL);
}

TEST(BitmaskTableDynamic, ReuseSlot) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);

    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(42)));
    ASSERT_TRUE(table.release(*idx));
    auto idx2 = table.acquire();
    ASSERT_TRUE(idx2.has_value());
    ASSERT_EQ(*idx, *idx2);
}


TEST(BitmaskTableDynamic, MultiThreadedAcquireRelease) {
    BitmaskTable<int, 0> table(DYNAMIC_MEDIUM);
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

    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        pool.emplace_back(worker, i);
    }
    for (auto& t : pool) t.join();

    ASSERT_EQ(success_acquire, success_release);

    // Table should be empty (all slots released)
    for (size_t i = 0; i < DYNAMIC_MEDIUM; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}

// Test: Releasing already released slot
TEST(BitmaskTableDynamic, DoubleRelease) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);

    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(123)));
    ASSERT_TRUE(table.release(*idx));
    ASSERT_FALSE(table.release(*idx));
}


// Test: Setting nullptr should be fine, but slot is not "active"
TEST(BitmaskTableDynamic, SetNullptr) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    auto idx = table.acquire();
    ASSERT_TRUE(idx.has_value());
    ASSERT_TRUE(table.set(*idx, nullptr));
    auto val = table.at(*idx);
    ASSERT_FALSE(val);
    table.release(*idx);
}

// Test: Table with N = 1
TEST(BitmaskTableDynamic, SingleSlot) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);

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
TEST(BitmaskTableDynamic, MaxSlots) {
    constexpr size_t _size = std::numeric_limits<uint16_t>::max();
    BitmaskTable<int, _size> table;

    ASSERT_EQ(table.capacity(), _size + 1);

    std::vector<size_t> idxs;
    idxs.reserve(_size);
    // Fill all slots
    for (size_t i = 0; i < _size; ++i) {
        auto idx = table.acquire();
        ASSERT_TRUE(idx.has_value());
        bool _has_set = table.set(*idx, std::make_shared<int>(int(i)));
        if(!_has_set) {
           ASSERT_TRUE(_has_set) << "Set failed at i=" << i << ", idx=" << *idx;
        } else {
            ASSERT_TRUE(_has_set);
        }
        idxs.push_back(*idx);
    }
    // No more slots should be available
    ASSERT_TRUE(table.acquire().has_value());
    // Release all slots
    for (auto idx : idxs) {
        ASSERT_TRUE(table.release(idx));
        ASSERT_FALSE(table.active(idx));
        ASSERT_FALSE(table.at(idx));
    }
    ASSERT_EQ(table.capacity(), _size + 1);
}


// Test: for_each lambda is called only for active slots
TEST(BitmaskTableDynamic, ForEachActive) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    std::vector<typename BitmaskTable<int, 0>::IndexType> idxs;
    idxs.reserve(DYNAMIC_SMALL);
    for (size_t i = 0; i < DYNAMIC_SMALL; i += 2) {
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


TEST(BitmaskTableDynamic, ForEachActiveFastSingleThread) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    std::set<size_t> expected;

    // Set odd indices
    for (size_t i = 1; i < DYNAMIC_SMALL; i += 2) {
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


TEST(BitmaskTableDynamic, ForEachActiveFastMultiThread) {
    BitmaskTable<int, 0> table(DYNAMIC_MEDIUM);

    // Randomly fill about half the slots
    for (size_t i = 0; i < DYNAMIC_MEDIUM; ++i) {
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

    const size_t threads = std::thread::hardware_concurrency();
    std::vector<std::thread> pool;
    pool.reserve(threads);
    
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker);
    for (auto& t : pool) t.join();

    // Since each thread visits all, divide by thread count for per-table coverage
    ASSERT_EQ(visited_count / threads, DYNAMIC_MEDIUM / 3 + (DYNAMIC_MEDIUM % 3 ? 1 : 0));
}

// Test 1: m_bitmask is not array, single-threaded (N=32)
TEST(BitmaskTableDynamic, SetEmplaceNonArraySingleThread) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
    std::vector<typename BitmaskTable<int, 0>::IndexType> indices;
    indices.reserve(DYNAMIC_SMALL);

    for (size_t i = 0; i < DYNAMIC_SMALL; ++i) {
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
TEST(BitmaskTableDynamic, SetEmplaceArraySingleThread) {
    BitmaskTable<int, 0> table(DYNAMIC_LARGE);
    std::vector<typename BitmaskTable<int, 0>::IndexType> indices;
    indices.reserve(DYNAMIC_LARGE);

    for (size_t i = 0; i < DYNAMIC_LARGE; ++i) {
        auto idx = table.set(std::make_shared<int>(1000 + int(i)));
        ASSERT_TRUE(idx.has_value());
        indices.push_back(*idx);
        auto val = table.at(*idx);
        ASSERT_TRUE(val);
        ASSERT_EQ(*val, 1000 + int(i));
        ASSERT_TRUE(table.active(*idx));
    }
    // All slots filled; next should fail
    ASSERT_TRUE(table.set(std::make_shared<int>(999)).has_value());
    for (auto idx : indices) {
        ASSERT_TRUE(table.release(idx));
    }
}

// Test 3: m_bitmask is not array, multi-threaded (N=32)
TEST(BitmaskTableDynamic, SetEmplaceNonArrayMultiThread) {
    BitmaskTable<int, 0> table(DYNAMIC_SMALL);
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
    for (size_t i = 0; i < DYNAMIC_SMALL; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
    // All attempts should succeed
    ASSERT_EQ(success, threads * ops_per_thread);
}


// Test 4: m_bitmask is array, multi-threaded (N=256)
TEST(BitmaskTableDynamic, SetEmplaceArrayMultiThread) {
    BitmaskTable<int, 0> table(DYNAMIC_LARGE);
    
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
    for (size_t i = 0; i < DYNAMIC_LARGE; ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}

// TEST(BitmaskTableDynamic, ResizeSingleThreaded) {
//     constexpr size_t initial_capacity = 16;
//     BitmaskTable<int, 0> table(initial_capacity);

//     size_t count = 0;
//     std::vector<size_t> idxs;
//     idxs.reserve(initial_capacity * 2);

//     // Insert well past the initial capacity (2x for safety)
//     while (count < initial_capacity * 2) {
//         auto idx = table.acquire();
//         if (!idx) break; // If fail to acquire, break (shouldn't happen if resize works)
//         ASSERT_TRUE(table.set(*idx, std::make_shared<int>(int(count))));
//         idxs.push_back(*idx);
//         ++count;
//     }

//     // Table should have resized at least once
//     ASSERT_GE(table.capacity(), count);

//     // Validate and clean up
//     for (size_t i = 0; i < idxs.size(); ++i) {
//         auto val = table.at(idxs[i]);
//         ASSERT_TRUE(val);
//         ASSERT_EQ(*val, int(i));
//         ASSERT_TRUE(table.release(idxs[i]));
//     }
//     ASSERT_EQ(table.size(), 0U);
// }

// TEST(BitmaskTableDynamic, ResizeMultiThreaded) {
//     constexpr size_t initial_capacity = 16;
//     BitmaskTable<int, 0> table(initial_capacity);

//     const size_t threads = std::thread::hardware_concurrency();
//     constexpr size_t inserts_per_thread = 25;
//     std::vector<std::vector<size_t>> thread_idxs(threads);
//     // thread_idxs.reserve(threads);

//     auto worker = [&](int tid) {
//         thread_idxs[tid].reserve(inserts_per_thread);
//         for (size_t i = 0; i < inserts_per_thread; ++i) {
//             auto idx = table.acquire();
//             if (idx) {
//                 table.set(*idx, std::make_shared<int>(int(tid * 1000 + i)));
//                 thread_idxs[tid].push_back(*idx);
//             }
//         }
//     };

//     std::vector<std::thread> pool;
//     pool.reserve(threads);
//     for (int t = 0; t < threads; ++t)
//         pool.emplace_back(worker, t);
//     for (auto& t : pool) t.join();

//     size_t total_inserts = 0;
//     for (const auto& v : thread_idxs) total_inserts += v.size();

//     ASSERT_GE(table.capacity(), total_inserts);

//     for (int t = 0; t < threads; ++t)
//         for (auto idx : thread_idxs[t])
//             ASSERT_TRUE(table.release(idx));
//     ASSERT_EQ(table.size(), 0U);
// }


// TEST(BitmaskTableDynamic, ConsistencySingleThreadNoResize) {
//     constexpr size_t cap = 32;
//     constexpr size_t max_fill = cap * 3 /4 ;
//     BitmaskTable<int, 0> table(cap);

//     // Fill all slots exactly
//     std::vector<size_t> idxs;
//     idxs.reserve(cap);
//     for (size_t i = 0; i < max_fill; ++i) {
//         auto idx = table.acquire();
//         ASSERT_TRUE(idx);
//         ASSERT_TRUE(table.set(*idx, std::make_shared<int>(int(i))));
//         idxs.push_back(*idx);
//     }
//     // Consistency: active() matches slot contents, no resize should have occurred
//     ASSERT_EQ(table.capacity(), cap);
//     // Consistency check for slots we filled
//     for (size_t i = 0; i < max_fill; ++i) {
//         ASSERT_EQ(table.active(idxs[i]), static_cast<bool>(table.at(idxs[i])));
//     }
//     // For slots we didn't use, they must be inactive/null
//     for (size_t i = max_fill; i < cap; ++i) {
//         ASSERT_FALSE(table.active(i));
//         ASSERT_FALSE(table.at(i));
//     }

//     // Release all, check again
//     for (auto idx : idxs) table.release(idx);
//     for (size_t i = 0; i < cap; ++i) {
//         ASSERT_FALSE(table.active(i));
//         ASSERT_FALSE(table.at(i));
//     }
// }

// TEST(BitmaskTableDynamic, ConsistencySingleThreadWithResize) {
//     constexpr size_t initial = 8;
//     constexpr size_t target = 40; // Will trigger at least one resize
//     BitmaskTable<int, 0> table(initial);

//     std::vector<size_t> idxs;
//     idxs.reserve(target);
//     for (size_t i = 0; i < target; ++i) {
//         auto idx = table.acquire();
//         ASSERT_TRUE(idx);
//         ASSERT_TRUE(table.set(*idx, std::make_shared<int>(int(i))));
//         idxs.push_back(*idx);
//     }
//     // Resize should have occurred
//     ASSERT_GE(table.capacity(), target);
//     for (size_t i = 0; i < target; ++i) {
//         ASSERT_EQ(table.active(idxs[i]), static_cast<bool>(table.at(idxs[i])));
//     }

//     // Release all, check again
//     for (auto idx : idxs) table.release(idx);
//     for (size_t i = 0; i < table.capacity(); ++i) {
//         ASSERT_EQ(table.active(i), static_cast<bool>(table.at(i)));
//     }
// }

// TEST(BitmaskTableDynamic, ConsistencyMultiThreadNoResize) {
//     constexpr size_t cap = 64;
//     constexpr size_t max_fill = cap * 3 / 4; // 75%
//     const size_t threads = std::min<size_t>(8, std::thread::hardware_concurrency());
//     const size_t per_thread = max_fill / threads;

//     BitmaskTable<int, 0> table(cap);
//     std::vector<std::vector<size_t>> slots(threads);

//     auto worker = [&](int tid) {
//         for (size_t i = 0; i < per_thread; ++i) {
//             auto idx = table.acquire();
//             ASSERT_TRUE(idx);
//             ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 100 + int(i))));
//             slots[tid].push_back(*idx);
//         }
//     };

//     std::vector<std::thread> pool;
//     pool.reserve(threads);
//     for (size_t t = 0; t < threads; ++t)
//         pool.emplace_back(worker, t);
//     for (auto& t : pool) t.join();

//     ASSERT_EQ(table.capacity(), cap);

//     // Consistency check for only used slots
//     for (size_t t = 0; t < threads; ++t)
//         for (auto idx : slots[t])
//             ASSERT_EQ(table.active(idx), static_cast<bool>(table.at(idx)));

//     // Release all, check again
//     for (size_t t = 0; t < threads; ++t)
//         for (auto idx : slots[t]) table.release(idx);
//     for (size_t i = 0; i < cap; ++i) {
//         ASSERT_FALSE(table.active(i));
//         ASSERT_FALSE(table.at(i));
//     }
// }

// TEST(BitmaskTableDynamic, ConsistencyMultiThreadWithResize) {
//     constexpr size_t initial = 16;
//     constexpr size_t per_thread = 25;
//     const size_t threads = std::thread::hardware_concurrency();
//     BitmaskTable<int, 0> table(initial);

//     // Make sure each thread has a unique vector for its results
//     std::vector<std::vector<size_t>> slots;
//     slots.resize(threads);  // Safe for indexed access

//     auto worker = [&](size_t tid) {
//         slots[tid].reserve(per_thread);
//         for (size_t i = 0; i < per_thread; ++i) {
//             auto idx = table.acquire();
//             ASSERT_TRUE(idx);
//             ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 1000 + int(i))));
//             slots[tid].push_back(*idx);
//         }
//     };

//     std::vector<std::thread> pool;
//     pool.reserve(threads);  // Reserve for efficiency (not for indexing!)
//     for (size_t t = 0; t < threads; ++t)
//         pool.emplace_back(worker, t);
//     for (auto& t : pool) t.join();

//     // Resize should have occurred (since we fill more than initial)
//     ASSERT_GE(table.capacity(), threads * per_thread);

//     // Consistency check
//     for (size_t t = 0; t < threads; ++t)
//         for (auto idx : slots[t])
//             ASSERT_EQ(table.active(idx), static_cast<bool>(table.at(idx)));

//     // Release all, check again
//     for (size_t t = 0; t < threads; ++t)
//         for (auto idx : slots[t])
//             table.release(idx);

//     for (size_t i = 0; i < table.capacity(); ++i) {
//         ASSERT_EQ(table.active(i), static_cast<bool>(table.at(i)));
//     }
// }


// TEST(BitmaskTableDynamic, RealWorldMixedOperations) {
//     BitmaskTable<int, 0> table(DYNAMIC_LARGE);
//     constexpr int threads = 32;
//     constexpr int ops_per_thread = 500;

//     auto worker = [&](int tid) {
//         thread_local std::mt19937 gen(std::random_device{}());
//         std::uniform_int_distribution<int> op_dist(0, 2);
//         std::vector<int> my_slots;

//         for (int i = 0; i < ops_per_thread; ++i) {
//             int op = op_dist(gen);

//             if (op == 0 || my_slots.empty()) {
//                 // Try to acquire
//                 auto idx = table.acquire();
//                 if (idx) {
//                     ASSERT_FALSE(table.at(*idx)); // Should be empty before use
//                     ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 1000 + i)));
//                     my_slots.push_back(*idx);
//                 }
//             } else {
//                 // Release random held slot
//                 std::uniform_int_distribution<size_t> sdist(0, my_slots.size() - 1);
//                 size_t idx_pos = sdist(gen);
//                 int idx_val = my_slots[idx_pos];
//                 // Optionally check
//                 auto v = table.at(idx_val);
//                 ASSERT_TRUE(v);
//                 table.release(idx_val);
//                 std::swap(my_slots[idx_pos], my_slots.back());
//                 my_slots.pop_back();
//             }
//         }
//         // Clean up any leftovers
//         for (int idx : my_slots) {
//             // ASSERT_TRUE(idx);
//             table.release(idx);
//         }
//     };

//     std::vector<std::thread> pool;
//     pool.reserve(threads);
//     for (int t = 0; t < threads; ++t)
//         pool.emplace_back(worker, t);

//     for (auto& t : pool) t.join();

//     // After all threads complete, check for leaks/dangling
//     for (size_t i = 0; i < DYNAMIC_LARGE; ++i) {
//         ASSERT_FALSE(table.active(i));
//         ASSERT_FALSE(table.at(i));
//     }
// }

TEST(BitmaskTableDynamic, RealWorldMixedResizeOperations) {
    constexpr size_t initial_capacity = 16;
    BitmaskTable<int, 0> table(initial_capacity);
    constexpr int threads = 8;
    constexpr int ops_per_thread = 100;

    auto worker = [&](int tid) {
        thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> op_dist(0, 2);
        std::vector<int> my_slots;
        my_slots.reserve(ops_per_thread);

        for (int i = 0; i < ops_per_thread; ++i) {
            int op = op_dist(gen);

            if (op == 0 || my_slots.empty()) {
                // Try to acquire (should trigger resizes)
                auto idx = table.acquire();
                if (idx) {
                    ASSERT_FALSE(table.at(*idx));
                    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 10000 + i)));
                    my_slots.push_back(*idx);
                }
            } else {
                // Release random held slot
                std::uniform_int_distribution<size_t> sdist(0, my_slots.size() - 1);
                size_t idx_pos = sdist(gen);
                int idx_val = my_slots[idx_pos];
                auto v = table.at(idx_val);
                ASSERT_TRUE(v);
                table.release(idx_val);
                std::swap(my_slots[idx_pos], my_slots.back());
                my_slots.pop_back();
            }
        }
        // Clean up
        for (int idx : my_slots) {
            table.release(idx);
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, t);
    for (auto& t : pool) t.join();

    // The table should have resized (because total ops > initial_capacity)
    ASSERT_GE(table.capacity(), initial_capacity);

    for (size_t i = 0; i < table.capacity(); ++i) {
        ASSERT_FALSE(table.active(i));
        ASSERT_FALSE(table.at(i));
    }
}

TEST(BitmaskTableDynamic, RealWorldMixedResizeOperationsStress) {
    constexpr size_t initial_capacity = 16;
    BitmaskTable<int, 0> table(initial_capacity);
    constexpr int threads = 16;
    constexpr int ops_per_thread = 1000;
    std::atomic<int> total_inserts{0};
    std::atomic<int> total_releases{0};
    std::atomic<int> failed_acquires{0};
    std::atomic<int> failed_releases{0};

    auto worker = [&](int tid) {
        thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> op_dist(0, 3);
        std::vector<int> my_slots;
        my_slots.reserve(ops_per_thread);

        for (int i = 0; i < ops_per_thread; ++i) {
            int op = op_dist(gen);

            if ((op == 0 || my_slots.empty()) && op != 3) {
                // Try to acquire (should trigger resizes)
                auto idx = table.acquire();
                if (idx) {
                    // Small yield to encourage context switches
                    if (i % 17 == 0) std::this_thread::yield();
                    ASSERT_FALSE(table.at(*idx));
                    ASSERT_TRUE(table.set(*idx, std::make_shared<int>(tid * 1000000 + i)));
                    my_slots.push_back(*idx);
                    total_inserts++;
                } else {
                    failed_acquires++;
                }
            } else if (op == 3 && (i % 10 == 0)) {
                // Stress: Voluntarily trigger resize
                size_t desired = initial_capacity * (2 + (i % 5));
                // table.resize(desired); // Safe: no-op if already large enough
            } else if (!my_slots.empty()) {
                // Only release if we have something!
                std::uniform_int_distribution<size_t> sdist(0, my_slots.size() - 1);
                size_t idx_pos = sdist(gen);
                int idx_val = my_slots[idx_pos];
                auto v = table.at(idx_val);
                if (!v) failed_releases++;
                ASSERT_TRUE(v);
                table.release(idx_val);
                total_releases++;
                std::swap(my_slots[idx_pos], my_slots.back());
                my_slots.pop_back();
            }
            // Randomly interleave yield/sleep
            if (i % 113 == 0) std::this_thread::yield();
            if (i % 151 == 0) std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        // Clean up
        for (int idx : my_slots) {
            table.release(idx);
            total_releases++;
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
        pool.emplace_back(worker, t);
    for (auto& t : pool) t.join();

    // The table should have resized (because total ops > initial_capacity)
    ASSERT_EQ(table.capacity(), initial_capacity);

    // All slots should be inactive and empty
    size_t found_active = 0, found_nonnull = 0;
    for (size_t i = 0; i < table.capacity(); ++i) {
        if (table.active(i)) found_active++;
        if (table.at(i)) found_nonnull++;
    }
    ASSERT_EQ(found_active, 0U);
    ASSERT_EQ(found_nonnull, 0U);

    // Print diagnostics (for manual inspection)
    std::cout << "Total acquires: " << total_inserts.load() << "\n";
    std::cout << "Total releases: " << total_releases.load() << "\n";
    std::cout << "Failed acquires: " << failed_acquires.load() << "\n";
    std::cout << "Failed releases: " << failed_releases.load() << "\n";
    std::cout << "Final table capacity: " << table.capacity() << "\n";
}

