#include <benchmark/benchmark.h>

#include "BitmapTree.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

using HazardSystem::BitmapTree;

namespace {

constexpr uint64_t C_LCG_A = 6364136223846793005ULL;
constexpr uint64_t C_LCG_C = 1ULL;

inline uint64_t lcg_next(uint64_t value) noexcept {
    return (value * C_LCG_A) + C_LCG_C;
}

std::vector<size_t> make_indices(size_t count, size_t mod, uint64_t seed) {
    std::vector<size_t> indices;
    indices.reserve(count);
    uint64_t x = seed;
    for (size_t i = 0; i < count; ++i) {
        x = lcg_next(x);
        indices.push_back(static_cast<size_t>(x % mod));
    }
    return indices;
}

} // namespace

static void BM_AvTree_SetClear_SingleWord(benchmark::State& state) {
    const size_t bits = static_cast<size_t>(state.range(0));
    BitmapTree tree;
    tree.initialization(bits, 1);
    tree.reset_clear(0);

    const auto indices = make_indices(4096, bits, 0x123456789ABCDEF0ULL);
    size_t pos = 0;

    for (auto _ : state) {
        size_t bit = indices[pos++ & (indices.size() - 1)];
        tree.set(bit);
        tree.clear(bit);
        benchmark::DoNotOptimize(bit);
    }

    state.SetComplexityN(static_cast<int64_t>(bits));
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_AvTree_SetClear_SingleWord)
    ->RangeMultiplier(2)
    ->Range(8, 64)
    ->Complexity(benchmark::oAuto);

static void BM_AvTree_SetClear_Tree_Propagate(benchmark::State& state) {
    const size_t bits = static_cast<size_t>(state.range(0));
    BitmapTree tree;
    tree.initialization(bits, 1);
    tree.reset_clear(0);

    const auto indices = make_indices(4096, bits, 0xC0FFEEULL);
    size_t pos = 0;

    for (auto _ : state) {
        size_t bit = indices[pos++ & (indices.size() - 1)];
        tree.set(bit);
        tree.clear(bit);
        benchmark::DoNotOptimize(bit);
    }

    state.SetComplexityN(static_cast<int64_t>(bits));
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_AvTree_SetClear_Tree_Propagate)
    ->RangeMultiplier(8)
    ->Range(128, 1 << 20)
    ->Complexity(benchmark::oAuto);

static void BM_AvTree_SetClear_Tree_NoPropagate(benchmark::State& state) {
    const size_t bits = static_cast<size_t>(state.range(0));
    BitmapTree tree;
    tree.initialization(bits, 1);
    tree.reset_clear(0);

    // Keep one bit set so toggles in the same leaf word avoid summary propagation.
    tree.set(0);

    const auto indices = make_indices(4096, 63, 0xBADC0FFEE0DDF00DULL);
    size_t pos = 0;

    for (auto _ : state) {
        size_t bit = 1 + (indices[pos++ & (indices.size() - 1)] % 63);
        tree.set(bit);
        tree.clear(bit);
        benchmark::DoNotOptimize(bit);
    }

    tree.clear(0);
    state.SetComplexityN(static_cast<int64_t>(bits));
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_AvTree_SetClear_Tree_NoPropagate)
    ->RangeMultiplier(8)
    ->Range(128, 1 << 20)
    ->Complexity(benchmark::oAuto);

static void BM_AvTree_FindAny_SingleWord(benchmark::State& state) {
    const size_t bits = static_cast<size_t>(state.range(0));
    BitmapTree tree;
    tree.initialization(bits, 1);
    tree.reset_clear(0);

    for (size_t i = 0; i < bits; i += 8) {
        tree.set(i);
    }

    uint64_t rng = 0xA5A5A5A5A5A5A5A5ULL;
    for (auto _ : state) {
        rng = lcg_next(rng);
        const size_t hint = static_cast<size_t>(rng & (bits - 1));
        auto found = tree.find(hint);
        benchmark::DoNotOptimize(found);
    }

    state.SetComplexityN(static_cast<int64_t>(bits));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AvTree_FindAny_SingleWord)
    ->RangeMultiplier(2)
    ->Range(8, 64)
    ->Complexity(benchmark::oAuto);

static void BM_AvTree_FindAny_Tree_Sparse(benchmark::State& state) {
    const size_t bits = static_cast<size_t>(state.range(0));
    BitmapTree tree;
    tree.initialization(bits, 1);
    tree.reset_clear(0);

    // Sparse: 1 set bit per 128.
    for (size_t i = 0; i < bits; i += 128) {
        tree.set(i);
    }
    tree.set(bits - 1);

    uint64_t rng = 0x0123456789ABCDEFULL;
    for (auto _ : state) {
        rng = lcg_next(rng);
        const size_t hint = static_cast<size_t>(rng % bits);
        auto found = tree.find(hint);
        benchmark::DoNotOptimize(found);
    }

    state.SetComplexityN(static_cast<int64_t>(bits));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AvTree_FindAny_Tree_Sparse)
    ->RangeMultiplier(8)
    ->Range(128, 1 << 20)
    ->Complexity(benchmark::oAuto);

static void BM_AvTree_FindNext_Tree_Sparse(benchmark::State& state) {
    const size_t bits = static_cast<size_t>(state.range(0));
    BitmapTree tree;
    tree.initialization(bits, 1);
    tree.reset_clear(0);

    for (size_t i = 0; i < bits; i += 128) {
        tree.set(i);
    }
    tree.set(bits - 1);

    uint64_t rng = 0xF00DF00DF00DF00DULL;
    for (auto _ : state) {
        rng = lcg_next(rng);
        const size_t start = static_cast<size_t>(rng % bits);
        auto found = tree.find_next(start);
        benchmark::DoNotOptimize(found);
    }

    state.SetComplexityN(static_cast<int64_t>(bits));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AvTree_FindNext_Tree_Sparse)
    ->RangeMultiplier(8)
    ->Range(128, 1 << 20)
    ->Complexity(benchmark::oAuto);

// "Real-world" mixed workload modeled after BitmaskTable's use of BitmapTree as a
// per-part hint structure (available parts vs non-empty parts).
struct MixedOwned {
    size_t part{0};
    uint8_t bit{0};
};

static void BM_AvTree_MixedWorkload(benchmark::State& state) {
    const size_t parts = static_cast<size_t>(state.range(0)); // >64 => tree mode
    constexpr size_t plane_available = 0;
    constexpr size_t plane_non_empty = 1;

    BitmapTree tree;
    tree.initialization(parts, 2);
    tree.reset_set(plane_available);
    tree.reset_clear(plane_non_empty);

    std::vector<std::atomic<uint64_t>> masks(parts);
    for (auto& mask : masks) {
        mask.store(0ULL, std::memory_order_relaxed);
    }

    uint64_t rng = 0xA0761D6478BD642FULL;
    MixedOwned owned{};
    bool has_owned = false;

    for (auto _ : state) {
        rng = lcg_next(rng);
        const size_t hint = static_cast<size_t>(rng % parts);

        // Allocate one slot (best-effort).
        if (!has_owned) {
            const auto part_opt = tree.find(hint, plane_available);
            if (part_opt && *part_opt < parts) {
                const size_t part = *part_opt;
                uint64_t mask = masks[part].load(std::memory_order_relaxed);
                const uint8_t bit = static_cast<uint8_t>((rng >> 32) & 63U);
                const uint64_t flag = 1ULL << bit;

                if ((mask & flag) == 0) {
                    if (masks[part].compare_exchange_weak(mask, mask | flag, std::memory_order_acq_rel,
                                                         std::memory_order_relaxed)) {
                        tree.set(part, plane_non_empty);
                        owned = MixedOwned{part, bit};
                        has_owned = true;
                    }
                }
            }
        }

        // Read-side scan: clear a stale non-empty hint opportunistically.
        rng = lcg_next(rng);
        const size_t scan_hint = static_cast<size_t>(rng % parts);
        const auto scan_part = tree.find_next(scan_hint, plane_non_empty);
        if (scan_part && *scan_part < parts) {
            const uint64_t mask = masks[*scan_part].load(std::memory_order_acquire);
            if (!mask) {
                tree.clear(*scan_part, plane_non_empty);
            }
        }

        // Free immediately (keeps occupancy low and stable across runs).
        if (has_owned) {
            const uint64_t flag = 1ULL << owned.bit;
            uint64_t old = masks[owned.part].fetch_and(~flag, std::memory_order_acq_rel);
            benchmark::DoNotOptimize(old);
            has_owned = false;
        }
    }

    state.SetComplexityN(static_cast<int64_t>(parts));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AvTree_MixedWorkload)
    ->RangeMultiplier(8)
    ->Range(128, 1 << 20)
    ->Complexity(benchmark::oAuto);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    std::cout << "=== BitmapTree Benchmark ===\n";
    std::cout << "Single-threaded microbenchmarks for SingleWord vs Tree mode, read vs update paths,\n";
    std::cout << "and a BitmaskTable-like mixed workload.\n\n" << std::flush;

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
