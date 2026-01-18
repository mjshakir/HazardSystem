#include <gtest/gtest.h>

#include "BitmapTree.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <utility>
#include <vector>

using HazardSystem::BitmapTree;

namespace {

std::vector<size_t> collect_set_bits(const BitmapTree& tree, size_t plane = 0) {
    std::vector<size_t> bits;
    if (!tree.leaf_bits()) {
        return bits;
    }

    size_t start = 0;
    for (;;) {
        auto bit = tree.find_next(start, plane);
        if (!bit) {
            return bits;
        }
        bits.push_back(*bit);
        if (*bit + 1 >= tree.leaf_bits()) {
            return bits;
        }
        start = *bit + 1;
    }
}

struct OwnedSlot {
    size_t part{0};
    uint8_t bit{0};
};

} // namespace

TEST(AvailabilityBitmapTreeTest, DefaultIsEmptyAndNoOps) {
    BitmapTree tree;
    EXPECT_EQ(tree.leaf_bits(), 0u);
    EXPECT_EQ(tree.planes(), 0u);
    EXPECT_FALSE(tree.find().has_value());
    EXPECT_FALSE(tree.find_next(0).has_value());

    tree.reset_set();
    tree.reset_clear();
    tree.set(0);
    tree.clear(0);
    EXPECT_EQ(tree.leaf_bits(), 0u);
    EXPECT_EQ(tree.planes(), 0u);
    EXPECT_FALSE(tree.find(0, 0).has_value());
    EXPECT_FALSE(tree.find_next(0, 0).has_value());
}

TEST(AvailabilityBitmapTreeTest, InitWithZeroBitsOrPlanesIsEmpty) {
    BitmapTree tree;

    tree.initialization(0);
    EXPECT_EQ(tree.leaf_bits(), 0u);
    EXPECT_EQ(tree.planes(), 0u);
    EXPECT_FALSE(tree.find().has_value());

    tree.initialization(128, 0);
    EXPECT_EQ(tree.leaf_bits(), 0u);
    EXPECT_EQ(tree.planes(), 0u);
    EXPECT_FALSE(tree.find().has_value());
    tree.set(0);
    tree.reset_set();
    EXPECT_FALSE(tree.find().has_value());
}

TEST(AvailabilityBitmapTreeTest, PlanesClampedToTwoAndIndependent) {
    BitmapTree tree;
    tree.initialization(64, 10);
    EXPECT_EQ(tree.leaf_bits(), 64u);
    EXPECT_EQ(tree.planes(), 2u);

    EXPECT_FALSE(tree.find(0, 0).has_value());
    EXPECT_FALSE(tree.find(0, 1).has_value());
    EXPECT_FALSE(tree.find(0, 2).has_value());

    tree.set(3, 0);
    const auto p0_first = tree.find_next(0, 0);
    ASSERT_TRUE(p0_first.has_value());
    EXPECT_EQ(*p0_first, 3u);
    EXPECT_FALSE(tree.find_next(0, 1).has_value());

    tree.set(7, 1);
    const auto p1_first = tree.find_next(0, 1);
    ASSERT_TRUE(p1_first.has_value());
    EXPECT_EQ(*p1_first, 7u);
    const auto p0_first_again = tree.find_next(0, 0);
    ASSERT_TRUE(p0_first_again.has_value());
    EXPECT_EQ(*p0_first_again, 3u);

    tree.clear(3, 0);
    EXPECT_FALSE(tree.find(0, 0).has_value());
    const auto p1_any = tree.find(0, 1);
    ASSERT_TRUE(p1_any.has_value());
    EXPECT_EQ(*p1_any, 7u);
}

TEST(AvailabilityBitmapTreeTest, SingleWordSetClearFindNextAndFindAny) {
    BitmapTree tree;
    tree.initialization(10, 1);
    EXPECT_EQ(tree.leaf_bits(), 10u);
    EXPECT_EQ(tree.planes(), 1u);

    EXPECT_FALSE(tree.find().has_value());
    EXPECT_FALSE(tree.find_next(0).has_value());

    tree.set(3);
    tree.set(7);
    const auto next0 = tree.find_next(0);
    ASSERT_TRUE(next0.has_value());
    EXPECT_EQ(*next0, 3u);
    const auto next4 = tree.find_next(4);
    ASSERT_TRUE(next4.has_value());
    EXPECT_EQ(*next4, 7u);
    EXPECT_FALSE(tree.find_next(8).has_value());

    const auto any0 = tree.find(0);
    ASSERT_TRUE(any0.has_value());
    EXPECT_EQ(*any0, 3u);
    const auto any4 = tree.find(4);
    ASSERT_TRUE(any4.has_value());
    EXPECT_EQ(*any4, 7u);
    const auto any8 = tree.find(8);
    ASSERT_TRUE(any8.has_value());
    EXPECT_EQ(*any8, 3u); // wrap
    const auto any18 = tree.find(18);
    ASSERT_TRUE(any18.has_value());
    EXPECT_EQ(*any18, 3u); // hint wraps before search

    tree.clear(7);
    const auto any4_after_clear = tree.find(4);
    ASSERT_TRUE(any4_after_clear.has_value());
    EXPECT_EQ(*any4_after_clear, 3u);
    tree.clear(7); // idempotent
    tree.set(3);   // idempotent

    tree.set(10);   // out-of-range
    tree.clear(100); // out-of-range
    const auto any0_after_oob = tree.find(0);
    ASSERT_TRUE(any0_after_oob.has_value());
    EXPECT_EQ(*any0_after_oob, 3u);

    tree.reset_set();
    for (size_t i = 0; i < tree.leaf_bits(); ++i) {
        const auto next = tree.find_next(i);
        ASSERT_TRUE(next.has_value()) << "i=" << i;
        EXPECT_EQ(*next, i);
    }
    EXPECT_FALSE(tree.find_next(tree.leaf_bits()).has_value());

    tree.reset_clear();
    EXPECT_FALSE(tree.find().has_value());
}

TEST(AvailabilityBitmapTreeTest, SingleWordLeafBits64MaskIsFullWidth) {
    BitmapTree tree;
    tree.initialization(64, 1);
    tree.reset_set();
    EXPECT_EQ(tree.leaf_bits(), 64u);
    const auto next0 = tree.find_next(0);
    ASSERT_TRUE(next0.has_value());
    EXPECT_EQ(*next0, 0u);
    const auto next63 = tree.find_next(63);
    ASSERT_TRUE(next63.has_value());
    EXPECT_EQ(*next63, 63u);
    EXPECT_FALSE(tree.find_next(64).has_value());

    tree.reset_clear();
    EXPECT_FALSE(tree.find().has_value());
    tree.set(63);
    const auto any0 = tree.find(0);
    ASSERT_TRUE(any0.has_value());
    EXPECT_EQ(*any0, 63u);
    const auto any64 = tree.find(64);
    ASSERT_TRUE(any64.has_value());
    EXPECT_EQ(*any64, 63u);
}

TEST(AvailabilityBitmapTreeTest, TreeModePartialLastWordResetAndSearch) {
    BitmapTree tree;
    tree.initialization(65, 1);
    EXPECT_EQ(tree.leaf_bits(), 65u);
    EXPECT_EQ(tree.planes(), 1u);
    EXPECT_FALSE(tree.find().has_value());

    tree.set(64);
    const auto next0 = tree.find_next(0);
    ASSERT_TRUE(next0.has_value());
    EXPECT_EQ(*next0, 64u);
    EXPECT_FALSE(tree.find_next(65).has_value());
    tree.set(0);
    const auto next0_after_set0 = tree.find_next(0);
    ASSERT_TRUE(next0_after_set0.has_value());
    EXPECT_EQ(*next0_after_set0, 0u);
    const auto next1 = tree.find_next(1);
    ASSERT_TRUE(next1.has_value());
    EXPECT_EQ(*next1, 64u);
    const auto any1 = tree.find(1);
    ASSERT_TRUE(any1.has_value());
    EXPECT_EQ(*any1, 64u);
    const auto any66 = tree.find(66);
    ASSERT_TRUE(any66.has_value());
    EXPECT_EQ(*any66, 64u);

    tree.clear(0);
    const auto next0_after_clear0 = tree.find_next(0);
    ASSERT_TRUE(next0_after_clear0.has_value());
    EXPECT_EQ(*next0_after_clear0, 64u);
    tree.clear(64);
    EXPECT_FALSE(tree.find().has_value());

    tree.reset_set();
    for (size_t i = 0; i < tree.leaf_bits(); ++i) {
        const auto next = tree.find_next(i);
        ASSERT_TRUE(next.has_value()) << "i=" << i;
        EXPECT_EQ(*next, i);
    }
    EXPECT_FALSE(tree.find_next(tree.leaf_bits()).has_value());
}

TEST(AvailabilityBitmapTreeTest, TreeModeMultipleLevelsSetClearPropagation) {
    BitmapTree tree;
    constexpr size_t bits = 4160; // 65 leaf words => 3-level tree
    tree.initialization(bits, 1);
    EXPECT_EQ(tree.leaf_bits(), bits);

    tree.set(0);
    tree.set(2000);
    tree.set(4096);
    tree.set(4097);
    tree.set(4159);

    const auto next0 = tree.find_next(0);
    ASSERT_TRUE(next0.has_value());
    EXPECT_EQ(*next0, 0u);
    const auto next1 = tree.find_next(1);
    ASSERT_TRUE(next1.has_value());
    EXPECT_EQ(*next1, 2000u);
    const auto next2001 = tree.find_next(2001);
    ASSERT_TRUE(next2001.has_value());
    EXPECT_EQ(*next2001, 4096u);
    const auto next4097 = tree.find_next(4097);
    ASSERT_TRUE(next4097.has_value());
    EXPECT_EQ(*next4097, 4097u);
    const auto next4098 = tree.find_next(4098);
    ASSERT_TRUE(next4098.has_value());
    EXPECT_EQ(*next4098, 4159u);
    EXPECT_FALSE(tree.find_next(bits).has_value());

    tree.clear(4097);
    const auto next4096_after_clear = tree.find_next(4096);
    ASSERT_TRUE(next4096_after_clear.has_value());
    EXPECT_EQ(*next4096_after_clear, 4096u);
    const auto next4097_after_clear = tree.find_next(4097);
    ASSERT_TRUE(next4097_after_clear.has_value());
    EXPECT_EQ(*next4097_after_clear, 4159u);

    tree.clear(0);
    tree.clear(2000);
    tree.clear(4096);
    tree.clear(4159);
    EXPECT_FALSE(tree.find().has_value());
    EXPECT_FALSE(tree.find_next(0).has_value());
}

TEST(AvailabilityBitmapTreeTest, MoveTransfersStateAndResetsSource) {
    BitmapTree src;
    src.initialization(130, 2);
    src.set(5, 0);
    src.set(127, 0);
    src.set(9, 1);

    BitmapTree moved(std::move(src));
    EXPECT_EQ(moved.leaf_bits(), 130u);
    EXPECT_EQ(moved.planes(), 2u);
    const auto moved_p0_first = moved.find_next(0, 0);
    ASSERT_TRUE(moved_p0_first.has_value());
    EXPECT_EQ(*moved_p0_first, 5u);
    const auto moved_p0_next = moved.find_next(6, 0);
    ASSERT_TRUE(moved_p0_next.has_value());
    EXPECT_EQ(*moved_p0_next, 127u);
    const auto moved_p1_first = moved.find_next(0, 1);
    ASSERT_TRUE(moved_p1_first.has_value());
    EXPECT_EQ(*moved_p1_first, 9u);

    EXPECT_EQ(src.leaf_bits(), 0u);
    EXPECT_EQ(src.planes(), 0u);
    EXPECT_FALSE(src.find().has_value());

    BitmapTree assigned;
    assigned.initialization(64, 1);
    assigned.reset_set();
    assigned = std::move(moved);
    EXPECT_EQ(assigned.leaf_bits(), 130u);
    EXPECT_EQ(assigned.planes(), 2u);
    const auto assigned_p0_first = assigned.find_next(0, 0);
    ASSERT_TRUE(assigned_p0_first.has_value());
    EXPECT_EQ(*assigned_p0_first, 5u);
    const auto assigned_p1_first = assigned.find_next(0, 1);
    ASSERT_TRUE(assigned_p1_first.has_value());
    EXPECT_EQ(*assigned_p1_first, 9u);

    EXPECT_EQ(moved.leaf_bits(), 0u);
    EXPECT_EQ(moved.planes(), 0u);
}

TEST(AvailabilityBitmapTreeTest, MultiThreadedReadersAndWriters) {
    BitmapTree tree;
    constexpr size_t bits = 8192; // >4096 => exercises multi-level tree
    tree.initialization(bits, 2);

    // Sentinel bit keeps each plane non-empty for deterministic assertions.
    tree.set(0, 0);
    tree.set(0, 1);

    const unsigned hw = std::max(2u, std::thread::hardware_concurrency());
    const size_t writer_threads = static_cast<size_t>(std::min(8u, hw));
    const size_t reader_threads = static_cast<size_t>(std::min(4u, hw));

    std::atomic<size_t> reader_errors{0};
    std::barrier start(static_cast<std::ptrdiff_t>(writer_threads + reader_threads));

    auto writer = [&](size_t thread_index) {
        start.arrive_and_wait();
        constexpr int iters = 20000;
        const size_t stride = writer_threads;
        size_t prev_bit = 0;
        for (int i = 0; i < iters; ++i) {
            const size_t bit = 1 + ((thread_index + (static_cast<size_t>(i) * stride)) % (bits - 1));
            tree.set(bit, 0);
            if (prev_bit) {
                tree.clear(prev_bit, 0);
            }
            prev_bit = bit;
        }
        if (prev_bit) {
            tree.clear(prev_bit, 0);
        }
    };

    auto reader = [&]() {
        start.arrive_and_wait();
        constexpr int iters = 20000;
        for (int i = 0; i < iters; ++i) {
            const size_t hint = static_cast<size_t>(i);
            const auto r0 = tree.find(hint, 0);
            if (!r0 or (*r0 >= bits)) {
                reader_errors.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            const auto r1 = tree.find(hint, 1);
            if (!r1 or (*r1 != 0u)) {
                reader_errors.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            const auto n0 = tree.find_next(hint % bits, 0);
            if (n0 and (*n0 >= bits)) {
                reader_errors.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(writer_threads + reader_threads);
    for (size_t i = 0; i < writer_threads; ++i) {
        threads.emplace_back(writer, i);
    }
    for (size_t i = 0; i < reader_threads; ++i) {
        threads.emplace_back(reader);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(reader_errors.load(std::memory_order_relaxed), 0u);

    // Writers clear all non-sentinel bits they touch; the tree should end with just bit 0 set in both planes.
    EXPECT_EQ(collect_set_bits(tree, 0), std::vector<size_t>({0u}));
    EXPECT_EQ(collect_set_bits(tree, 1), std::vector<size_t>({0u}));
}

TEST(AvailabilityBitmapTreeTest, RealWorldMixedOperations) {
    constexpr size_t plane_available = 0;
    constexpr size_t plane_non_empty = 1;
    constexpr size_t parts = 257; // >64 (tree mode) and non-multiple of 64

    BitmapTree tree;
    tree.initialization(parts, 2);
    tree.reset_set(plane_available);
    tree.reset_clear(plane_non_empty);

    std::vector<std::atomic<uint64_t>> masks(parts);
    for (auto& mask : masks) {
        mask.store(0ULL, std::memory_order_relaxed);
    }

    constexpr size_t threads = 8;
    constexpr int ops_per_thread = 2000;
    constexpr size_t max_owned = 256; // keeps total occupancy well below parts*64
    constexpr int max_allocate_attempts = 32;

    std::atomic<uint64_t> alloc_success{0};
    std::atomic<uint64_t> alloc_fail{0};
    std::atomic<uint64_t> free_success{0};
    std::atomic<uint64_t> free_errors{0};
    std::atomic<uint64_t> stale_available_fixed{0};
    std::atomic<uint64_t> stale_non_empty_fixed{0};
    std::atomic<uint64_t> scan_steps{0};
    std::atomic<uint64_t> errors{0};

    auto now_seed = []() -> uint64_t {
        return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    };

    auto worker = [&](size_t tid) {
        std::mt19937_64 rng(now_seed() ^ (tid * 0x9E3779B97F4A7C15ULL));
        std::uniform_int_distribution<int> op_dist(0, 2); // 0=alloc, 1=free, 2=scan
        std::vector<OwnedSlot> owned;
        owned.reserve(max_owned);

        auto try_alloc = [&](size_t hint) -> bool {
            size_t local_hint = hint % parts;
            for (int attempt = 0; attempt < max_allocate_attempts; ++attempt) {
                auto part_opt = tree.find(local_hint, plane_available);
                if (!part_opt) {
                    return false;
                }
                const size_t part = *part_opt;
                if (part >= parts) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }

                uint64_t mask = masks[part].load(std::memory_order_relaxed);
                if (mask == ~0ULL) {
                    tree.clear(part, plane_available);
                    if (masks[part].load(std::memory_order_acquire) != ~0ULL) {
                        tree.set(part, plane_available);
                    } else {
                        stale_available_fixed.fetch_add(1, std::memory_order_relaxed);
                    }
                    local_hint = (part + 1) % parts;
                    continue;
                }

                const uint64_t free_mask = ~mask;
                if (!free_mask) {
                    tree.clear(part, plane_available);
                    if (masks[part].load(std::memory_order_acquire) != ~0ULL) {
                        tree.set(part, plane_available);
                    } else {
                        stale_available_fixed.fetch_add(1, std::memory_order_relaxed);
                    }
                    local_hint = (part + 1) % parts;
                    continue;
                }

                const uint8_t bit = static_cast<uint8_t>(std::countr_zero(free_mask));
                const uint64_t flag = 1ULL << bit;
                const uint64_t desired = mask | flag;

                if (!masks[part].compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    local_hint = part;
                    continue;
                }

                owned.push_back(OwnedSlot{part, bit});
                tree.set(part, plane_non_empty); // like BitmaskTable: non-empty is a hint (may be stale set)
                if (desired == ~0ULL) {
                    tree.clear(part, plane_available);
                    if (masks[part].load(std::memory_order_acquire) != ~0ULL) {
                        tree.set(part, plane_available);
                    }
                }
                return true;
            }
            return false;
        };

        auto do_free = [&]() -> bool {
            if (owned.empty()) {
                return false;
            }
            std::uniform_int_distribution<size_t> pick(0, owned.size() - 1);
            const size_t pos = pick(rng);
            const OwnedSlot slot = owned[pos];
            owned[pos] = owned.back();
            owned.pop_back();

            const uint64_t flag = 1ULL << slot.bit;
            const uint64_t old = masks[slot.part].fetch_and(~flag, std::memory_order_acq_rel);
            if ((old & flag) == 0) {
                free_errors.fetch_add(1, std::memory_order_relaxed);
                errors.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            if (old == ~0ULL) {
                tree.set(slot.part, plane_available);
            }
            return true;
        };

        auto scan_non_empty = [&]() {
            size_t hint = static_cast<size_t>(rng() % parts);
            for (int step = 0; step < 16; ++step) {
                ++scan_steps;
                auto part_opt = tree.find_next(hint, plane_non_empty);
                if (!part_opt) {
                    return;
                }
                const size_t part = *part_opt;
                if (part >= parts) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                const uint64_t mask = masks[part].load(std::memory_order_acquire);
                if (!mask) {
                    tree.clear(part, plane_non_empty);
                    stale_non_empty_fixed.fetch_add(1, std::memory_order_relaxed);
                    hint = part + 1;
                    continue;
                }
                return;
            }
        };

        for (int i = 0; i < ops_per_thread; ++i) {
            const int op = op_dist(rng);
            if ((op == 0 || owned.empty()) && owned.size() < max_owned) {
                if (try_alloc(static_cast<size_t>(rng() % parts))) {
                    alloc_success.fetch_add(1, std::memory_order_relaxed);
                } else {
                    alloc_fail.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (op == 1) {
                if (do_free()) {
                    free_success.fetch_add(1, std::memory_order_relaxed);
                }
            } else {
                scan_non_empty();
            }

            if ((i % 113) == 0) {
                std::this_thread::yield();
            }
        }

        while (!owned.empty()) {
            if (do_free()) {
                free_success.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (size_t t = 0; t < threads; ++t) {
        pool.emplace_back(worker, t);
    }
    for (auto& t : pool) {
        t.join();
    }

    EXPECT_EQ(errors.load(std::memory_order_relaxed), 0u);
    EXPECT_EQ(free_errors.load(std::memory_order_relaxed), 0u);
    EXPECT_EQ(alloc_success.load(std::memory_order_relaxed), free_success.load(std::memory_order_relaxed));

    for (size_t part = 0; part < parts; ++part) {
        EXPECT_EQ(masks[part].load(std::memory_order_acquire), 0ULL);
    }

    // Final pass: clear stale "non-empty" hints (matches BitmaskTable's for_each_active_fast cleanup path).
    for (size_t hint = 0; hint < parts;) {
        const auto part_opt = tree.find_next(hint, plane_non_empty);
        if (!part_opt) {
            break;
        }
        const size_t part = *part_opt;
        ASSERT_LT(part, parts);
        EXPECT_EQ(masks[part].load(std::memory_order_acquire), 0ULL);
        tree.clear(part, plane_non_empty);
        hint = part + 1;
    }

    EXPECT_FALSE(tree.find(0, plane_non_empty).has_value());

    std::vector<size_t> expected_available;
    expected_available.reserve(parts);
    for (size_t i = 0; i < parts; ++i) {
        expected_available.push_back(i);
    }
    EXPECT_EQ(collect_set_bits(tree, plane_available), expected_available);
}

TEST(AvailabilityBitmapTreeTest, RealWorldMixedOperationsStress) {
    constexpr size_t plane_available = 0;
    constexpr size_t plane_non_empty = 1;
    constexpr size_t parts = 257;

    BitmapTree tree;
    tree.initialization(parts, 2);
    tree.reset_set(plane_available);
    tree.reset_clear(plane_non_empty);

    std::vector<std::atomic<uint64_t>> masks(parts);
    for (auto& mask : masks) {
        mask.store(0ULL, std::memory_order_relaxed);
    }

    const size_t hw = std::max<size_t>(2, static_cast<size_t>(std::thread::hardware_concurrency()));
    const size_t threads = std::min<size_t>(16, hw);
    constexpr int ops_per_thread = 5000;
    constexpr size_t max_owned = 512;
    constexpr int max_allocate_attempts = 64;

    std::atomic<uint64_t> alloc_success{0};
    std::atomic<uint64_t> alloc_fail{0};
    std::atomic<uint64_t> free_success{0};
    std::atomic<uint64_t> free_errors{0};
    std::atomic<uint64_t> stale_available_fixed{0};
    std::atomic<uint64_t> stale_non_empty_fixed{0};
    std::atomic<uint64_t> scan_steps{0};
    std::atomic<uint64_t> errors{0};

    auto now_seed = []() -> uint64_t {
        return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    };

    auto worker = [&](size_t tid) {
        std::mt19937_64 rng(now_seed() ^ (tid * 0xD1B54A32D192ED03ULL));
        std::uniform_int_distribution<int> op_dist(0, 3); // 0=alloc, 1=free, 2=scan, 3=extra scan/yield
        std::vector<OwnedSlot> owned;
        owned.reserve(max_owned);

        auto try_alloc = [&](size_t hint) -> bool {
            size_t local_hint = hint % parts;
            for (int attempt = 0; attempt < max_allocate_attempts; ++attempt) {
                auto part_opt = tree.find(local_hint, plane_available);
                if (!part_opt) {
                    return false;
                }
                const size_t part = *part_opt;
                if (part >= parts) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }

                uint64_t mask = masks[part].load(std::memory_order_relaxed);
                if (mask == ~0ULL) {
                    tree.clear(part, plane_available);
                    if (masks[part].load(std::memory_order_acquire) != ~0ULL) {
                        tree.set(part, plane_available);
                    } else {
                        stale_available_fixed.fetch_add(1, std::memory_order_relaxed);
                    }
                    local_hint = (part + 1) % parts;
                    continue;
                }

                const uint64_t free_mask = ~mask;
                if (!free_mask) {
                    tree.clear(part, plane_available);
                    if (masks[part].load(std::memory_order_acquire) != ~0ULL) {
                        tree.set(part, plane_available);
                    } else {
                        stale_available_fixed.fetch_add(1, std::memory_order_relaxed);
                    }
                    local_hint = (part + 1) % parts;
                    continue;
                }

                const uint8_t bit = static_cast<uint8_t>(std::countr_zero(free_mask));
                const uint64_t flag = 1ULL << bit;
                const uint64_t desired = mask | flag;

                if (!masks[part].compare_exchange_weak(mask, desired, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    local_hint = part;
                    continue;
                }

                owned.push_back(OwnedSlot{part, bit});
                tree.set(part, plane_non_empty);
                if (desired == ~0ULL) {
                    tree.clear(part, plane_available);
                    if (masks[part].load(std::memory_order_acquire) != ~0ULL) {
                        tree.set(part, plane_available);
                    }
                }
                return true;
            }
            return false;
        };

        auto do_free = [&]() -> bool {
            if (owned.empty()) {
                return false;
            }
            std::uniform_int_distribution<size_t> pick(0, owned.size() - 1);
            const size_t pos = pick(rng);
            const OwnedSlot slot = owned[pos];
            owned[pos] = owned.back();
            owned.pop_back();

            const uint64_t flag = 1ULL << slot.bit;
            const uint64_t old = masks[slot.part].fetch_and(~flag, std::memory_order_acq_rel);
            if ((old & flag) == 0) {
                free_errors.fetch_add(1, std::memory_order_relaxed);
                errors.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            if (old == ~0ULL) {
                tree.set(slot.part, plane_available);
            }
            return true;
        };

        auto scan_non_empty = [&]() {
            size_t hint = static_cast<size_t>(rng() % parts);
            for (int step = 0; step < 32; ++step) {
                ++scan_steps;
                auto part_opt = tree.find_next(hint, plane_non_empty);
                if (!part_opt) {
                    return;
                }
                const size_t part = *part_opt;
                if (part >= parts) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                const uint64_t mask = masks[part].load(std::memory_order_acquire);
                if (!mask) {
                    tree.clear(part, plane_non_empty);
                    stale_non_empty_fixed.fetch_add(1, std::memory_order_relaxed);
                    hint = part + 1;
                    continue;
                }
                return;
            }
        };

        for (int i = 0; i < ops_per_thread; ++i) {
            const int op = op_dist(rng);
            if ((op == 0 || owned.empty()) && owned.size() < max_owned) {
                if (try_alloc(static_cast<size_t>(rng() % parts))) {
                    alloc_success.fetch_add(1, std::memory_order_relaxed);
                } else {
                    alloc_fail.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (op == 1) {
                if (do_free()) {
                    free_success.fetch_add(1, std::memory_order_relaxed);
                }
            } else {
                scan_non_empty();
            }

            if ((i % 151) == 0) {
                std::this_thread::yield();
            }
            if ((i % 509) == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }

        while (!owned.empty()) {
            if (do_free()) {
                free_success.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (size_t t = 0; t < threads; ++t) {
        pool.emplace_back(worker, t);
    }
    for (auto& t : pool) {
        t.join();
    }

    // All slots should have been freed.
    for (size_t part = 0; part < parts; ++part) {
        EXPECT_EQ(masks[part].load(std::memory_order_acquire), 0ULL);
    }

    // Final cleanup: clear stale non-empty bits.
    for (size_t hint = 0; hint < parts;) {
        const auto part_opt = tree.find_next(hint, plane_non_empty);
        if (!part_opt) {
            break;
        }
        const size_t part = *part_opt;
        ASSERT_LT(part, parts);
        if (masks[part].load(std::memory_order_acquire) == 0ULL) {
            tree.clear(part, plane_non_empty);
        }
        hint = part + 1;
    }

    EXPECT_FALSE(tree.find(0, plane_non_empty).has_value());

    std::vector<size_t> expected_available;
    expected_available.reserve(parts);
    for (size_t i = 0; i < parts; ++i) {
        expected_available.push_back(i);
    }
    EXPECT_EQ(collect_set_bits(tree, plane_available), expected_available);

    // Print diagnostics (shown by CTest on failure or with -V).
    std::cout << "Threads: " << threads << "\n";
    std::cout << "Alloc success: " << alloc_success.load() << "\n";
    std::cout << "Alloc fail: " << alloc_fail.load() << "\n";
    std::cout << "Free success: " << free_success.load() << "\n";
    std::cout << "Free errors: " << free_errors.load() << "\n";
    std::cout << "Stale available fixed: " << stale_available_fixed.load() << "\n";
    std::cout << "Stale non-empty fixed: " << stale_non_empty_fixed.load() << "\n";
    std::cout << "Scan steps: " << scan_steps.load() << "\n";
    std::cout << "Errors: " << errors.load() << "\n";

    EXPECT_EQ(errors.load(std::memory_order_relaxed), 0u);
    EXPECT_EQ(free_errors.load(std::memory_order_relaxed), 0u);
    EXPECT_EQ(alloc_success.load(std::memory_order_relaxed), free_success.load(std::memory_order_relaxed));
}
