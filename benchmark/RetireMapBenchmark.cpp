#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "HazardPointerManager.hpp"
#include "RetireMap.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

namespace {

struct Node {
    int value;
    explicit Node(int v) : value(v) {}
};

constexpr size_t kHpmHazardSize  = 64;
constexpr size_t kHpmRetiredSize = 32;

// ============================================================================
// Micro 1: pure retire(T*) — predicate is always-hazard so the threshold scan
// never fires. Measures the raw cost of the retire fast path: hash insert +
// Deleter construction.
// ============================================================================

class RetireMapRetireFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        m_n = static_cast<size_t>(state.range(0));
        rebuild();
        m_idx = 0;
    }

    void TearDown(const ::benchmark::State&) override {
        m_map.reset();   // map dtor releases the Node*s via default delete
        m_raws.clear();
    }

protected:
    void rebuild(void) {
        auto always = std::make_shared<std::function<bool(const Node*)>>(
            [](const Node*) { return true; });
        m_map = std::make_unique<RetireMap<Node>>(m_n * 2UL, always);
        m_raws.clear();
        m_raws.reserve(m_n);
        for (size_t i = 0; i < m_n; ++i) {
            m_raws.push_back(new Node(static_cast<int>(i)));
        }
    }

    size_t m_n{};
    size_t m_idx{};
    std::vector<Node*> m_raws;
    std::unique_ptr<RetireMap<Node>> m_map;
};

BENCHMARK_DEFINE_F(RetireMapRetireFixture, Retire)(benchmark::State& state) {
    for (auto _ : state) {
        if (m_idx == m_raws.size()) {
            state.PauseTiming();
            rebuild();
            m_idx = 0;
            state.ResumeTiming();
        }
        benchmark::DoNotOptimize(m_map->retire(m_raws[m_idx]));
        ++m_idx;
    }
    state.SetComplexityN(static_cast<int64_t>(m_n));
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(RetireMapRetireFixture, Retire)
    ->RangeMultiplier(2)
    ->Range(64, 16384)
    ->Complexity(benchmark::o1);

// ============================================================================
// Micro 2: pure scan/reclaim — pre-fill N retired pointers, then reclaim with
// a predicate that drops 50%. Measures scan_and_reclaim cost (predicate calls
// + erase).
// ============================================================================

class RetireMapScanFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        m_n = static_cast<size_t>(state.range(0));
    }

    void TearDown(const ::benchmark::State&) override {
        // Per-iteration map goes out of scope; nothing persistent to clean up.
    }

protected:
    size_t m_n{};
};

BENCHMARK_DEFINE_F(RetireMapScanFixture, Scan)(benchmark::State& state) {
    uint64_t lcg = 0x9E3779B97F4A7C15ULL;
    for (auto _ : state) {
        state.PauseTiming();
        auto always = std::make_shared<std::function<bool(const Node*)>>(
            [](const Node*) { return true; });
        RetireMap<Node> m(m_n * 2UL, always);
        for (size_t i = 0; i < m_n; ++i) {
            m.retire(new Node(static_cast<int>(i)));
        }
        state.ResumeTiming();

        auto removed = m.reclaim_with([&lcg](const Node*) {
            lcg = lcg * 6364136223846793005ULL + 1442695040888963407ULL;
            return (lcg & 1ULL) != 0; // ~50% kept (hazard) vs reclaimed
        });
        benchmark::DoNotOptimize(removed);
    }
    state.SetComplexityN(static_cast<int64_t>(m_n));
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(m_n));
}

BENCHMARK_REGISTER_F(RetireMapScanFixture, Scan)
    ->RangeMultiplier(2)
    ->Range(64, 16384)
    ->Complexity(benchmark::oN);

// ============================================================================
// Micro 3: retire-triggered auto-reclaim cycle — steady-state where retire()
// repeatedly hits the threshold and forces an internal scan via the type-
// erased m_hazard predicate. Mimics real per-thread retire pressure.
// ============================================================================

class RetireMapCycleFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        m_threshold = static_cast<size_t>(state.range(0));
        m_lcg = 0x9E3779B97F4A7C15ULL;
        // Hazard predicate keeps ~50% so reclaim succeeds and retire can keep flowing.
        auto half = std::make_shared<std::function<bool(const Node*)>>(
            [this](const Node*) {
                m_lcg = m_lcg * 6364136223846793005ULL + 1442695040888963407ULL;
                return (m_lcg & 1ULL) != 0;
            });
        m_map = std::make_unique<RetireMap<Node>>(m_threshold, half);
    }

    void TearDown(const ::benchmark::State&) override {
        m_map.reset();
    }

protected:
    size_t   m_threshold{};
    uint64_t m_lcg{};
    std::unique_ptr<RetireMap<Node>> m_map;
};

BENCHMARK_DEFINE_F(RetireMapCycleFixture, RetireReclaimCycle)(benchmark::State& state) {
    int64_t i = 0;
    for (auto _ : state) {
        Node* p = new Node(static_cast<int>(i));
        if (!m_map->retire(p)) {
            // Threshold full and nothing reclaimable, or duplicate (very rare with fresh ptrs).
            delete p;
        }
        ++i;
    }
    state.SetComplexityN(static_cast<int64_t>(m_threshold));
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(RetireMapCycleFixture, RetireReclaimCycle)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->Complexity(benchmark::o1);

// ============================================================================
// Application: HazardPointerManager protect → retire → reclaim end-to-end.
// Each thread keeps its own static thread_local RetireMap<Node>. ThreadRange
// covers single-thread and multi-thread scaling.
// ============================================================================

using ManagerT = HazardPointerManager<Node, 0>;

class HPMEndToEndFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override {
        ThreadRegistry::instance().register_id();
    }

    void TearDown(const ::benchmark::State&) override {
        // Per-thread RetireMap stays alive until thread exit; that's fine.
    }
};

BENCHMARK_DEFINE_F(HPMEndToEndFixture, ProtectRetireReclaim)(benchmark::State& state) {
    auto& manager = ManagerT::instance(kHpmHazardSize, kHpmRetiredSize);

    int64_t i = 0;
    for (auto _ : state) {
        auto data = std::make_shared<Node>(static_cast<int>(i));
        auto guard = manager.protect(data);
        benchmark::DoNotOptimize(guard);
        if (guard) {
            guard.reset();              // release hazard slot before retiring
        }
        benchmark::DoNotOptimize(manager.retire(data));
        ++i;
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_REGISTER_F(HPMEndToEndFixture, ProtectRetireReclaim)
    ->ThreadRange(1, static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));

}  // namespace

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);

    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    // Pre-instantiate the singleton so the first benchmark thread doesn't pay
    // construction cost mid-loop.
    auto& manager = ManagerT::instance(kHpmHazardSize, kHpmRetiredSize);
    static_cast<void>(manager);

    ThreadRegistry::instance().register_id();

    std::cout << "=== RetireMap Benchmark Suite ===\n";
    std::cout << "Micro: Retire / Scan / RetireReclaimCycle\n";
    std::cout << "Application: HazardPointerManager protect+retire+reclaim ("
              << "hazards=" << kHpmHazardSize
              << ", retired_threshold=" << kHpmRetiredSize << ")\n";
    std::cout << "==================================\n\n";

    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}
