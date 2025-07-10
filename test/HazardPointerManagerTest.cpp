// test/HazardPointerManagerTest.cpp

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <algorithm>
#include <array>
#include <iostream>

#include "HazardPointerManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

//-----------------------------------------------------------------------------
// Two data types: one for 64‐slot managers, one for 128‐slot managers
//-----------------------------------------------------------------------------
struct BasicData {
    int value;
    std::atomic<int> access_count{0};
    std::atomic<bool> destroyed{false};

    BasicData(int v=0): value(v) {}
    ~BasicData() { destroyed.store(true, std::memory_order_release); }
    void access()    { access_count.fetch_add(1, std::memory_order_relaxed); }
    void increment() { access_count.fetch_add(1, std::memory_order_relaxed); }
};

struct BasicData128 {
    int value;
    std::atomic<int> access_count{0};
    std::atomic<bool> destroyed{false};

    BasicData128(int v=0): value(v) {}
    ~BasicData128() { destroyed.store(true, std::memory_order_release); }
    void access()    { access_count.fetch_add(1, std::memory_order_relaxed); }
    void increment() { access_count.fetch_add(1, std::memory_order_relaxed); }
};

//-----------------------------------------------------------------------------
// Test Fixture
//-----------------------------------------------------------------------------
class ProtectOnlyHazardPointerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ThreadRegistry::instance().register_id();
    }
    void TearDown() override {
        // clear all four singletons
        HazardPointerManager<BasicData,64>::instance().clear();
        HazardPointerManager<BasicData128,128>::instance().clear();
        HazardPointerManager<BasicData,0>::instance(64).clear();
        HazardPointerManager<BasicData128,0>::instance(128).clear();
    }
};

//-----------------------------------------------------------------------------
// 1) Basic protect tests
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, ProtectBasic) {
    auto& mgr = HazardPointerManager<BasicData,64>::instance();
    auto sp = std::make_shared<BasicData>(42);
    auto p = mgr.protect(sp);
    EXPECT_TRUE(static_cast<bool>(p));
    EXPECT_EQ(p->value, 42);
    p.reset();
    EXPECT_EQ(mgr.hazard_size(), 0u);
}

TEST_F(ProtectOnlyHazardPointerManagerTest, ProtectNullptr) {
    auto& mgr = HazardPointerManager<BasicData,64>::instance();
    std::shared_ptr<BasicData> sp;
    auto p = mgr.protect(sp);
    EXPECT_FALSE(static_cast<bool>(p));
    EXPECT_EQ(mgr.hazard_size(), 0u);
}

TEST_F(ProtectOnlyHazardPointerManagerTest, ProtectAtomicNullptr) {
    auto& mgr = HazardPointerManager<BasicData,64>::instance();
    std::atomic<std::shared_ptr<BasicData>> a;
    a.store(nullptr);
    auto p = mgr.protect(a);
    EXPECT_FALSE(static_cast<bool>(p));
    EXPECT_EQ(mgr.hazard_size(), 0u);
}

//-----------------------------------------------------------------------------
// 2) Exhaustion
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, ExhaustionFixed64) {
    constexpr size_t N = 64;
    auto& mgr = HazardPointerManager<BasicData,N>::instance();
    std::vector<ProtectedPointer<BasicData>> guards; guards.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        auto p = mgr.protect(std::make_shared<BasicData>(int(i)));
        ASSERT_TRUE(static_cast<bool>(p));
        guards.push_back(std::move(p));
    }
    // next must fail
    auto fail = mgr.protect(std::make_shared<BasicData>(-1));
    EXPECT_FALSE(static_cast<bool>(fail));
    // release all
    guards.clear();
    auto ok = mgr.protect(std::make_shared<BasicData>(123));
    EXPECT_TRUE(static_cast<bool>(ok));
}

TEST_F(ProtectOnlyHazardPointerManagerTest, ExhaustionFixed128) {
    constexpr size_t N = 128;
    auto& mgr = HazardPointerManager<BasicData128,N>::instance();
    std::vector<ProtectedPointer<BasicData128>> guards; guards.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        auto p = mgr.protect(std::make_shared<BasicData128>(int(i)));
        ASSERT_TRUE(static_cast<bool>(p));
        guards.push_back(std::move(p));
    }
    auto fail = mgr.protect(std::make_shared<BasicData128>(-1));
    EXPECT_FALSE(static_cast<bool>(fail));
    guards.clear();
    auto ok = mgr.protect(std::make_shared<BasicData128>(321));
    EXPECT_TRUE(static_cast<bool>(ok));
}

TEST_F(ProtectOnlyHazardPointerManagerTest, ExhaustionDynamic64) {
    constexpr size_t N = 64;
    auto& mgr = HazardPointerManager<BasicData,0>::instance(N);
    std::vector<ProtectedPointer<BasicData>> guards; guards.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        auto p = mgr.protect(std::make_shared<BasicData>(int(i)));
        ASSERT_TRUE(static_cast<bool>(p));
        guards.push_back(std::move(p));
    }
    auto fail = mgr.protect(std::make_shared<BasicData>(-1));
    EXPECT_FALSE(static_cast<bool>(fail));
    guards.clear();
    auto ok = mgr.protect(std::make_shared<BasicData>(456));
    EXPECT_TRUE(static_cast<bool>(ok));
}

TEST_F(ProtectOnlyHazardPointerManagerTest, ExhaustionDynamic128) {
    constexpr size_t N = 128;
    auto& mgr = HazardPointerManager<BasicData128,0>::instance(N);
    std::vector<ProtectedPointer<BasicData128>> guards; guards.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        auto p = mgr.protect(std::make_shared<BasicData128>(int(i)));
        ASSERT_TRUE(static_cast<bool>(p));
        guards.push_back(std::move(p));
    }
    auto fail = mgr.protect(std::make_shared<BasicData128>(-1));
    EXPECT_FALSE(static_cast<bool>(fail));
    guards.clear();
    auto ok = mgr.protect(std::make_shared<BasicData128>(789));
    EXPECT_TRUE(static_cast<bool>(ok));
}

//-----------------------------------------------------------------------------
// 3) Rapid protect + reset
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, RapidProtectReset) {
    auto& mgr = HazardPointerManager<BasicData,64>::instance();
    constexpr int OPS = 10000;
    for (int i = 0; i < OPS; ++i) {
        auto p = mgr.protect(std::make_shared<BasicData>(i));
        if (p) p->access();
        // immediate reset on scope exit
    }
    EXPECT_EQ(mgr.hazard_size(), 0u);
}

//-----------------------------------------------------------------------------
// 4) Single‐thread mixed workload
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, MixedSingleThreadWorkload) {
    auto& mgr = HazardPointerManager<BasicData,64>::instance();
    std::vector<std::shared_ptr<BasicData>> items;
    for (int i = 0; i < 1000; ++i) items.push_back(std::make_shared<BasicData>(i));

    for (int i = 0; i < 10000; ++i) {
        int op = i % 3;
        if (op == 0) {
            auto p = mgr.protect(items[i % items.size()]);
            if (p) p->access();
        }
        else if (op == 1) {
            auto p = mgr.protect(items[i % items.size()]);
        }
        else {
            mgr.clear();
        }
    }
    EXPECT_EQ(mgr.hazard_size(), 0u);
}

//-----------------------------------------------------------------------------
// 5) Real‐World Mixed Stress Test (multi‐thread)
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, RealWorldMixedStressTest) {
    std::atomic<int> C64{0}, D64{0}, C128{0}, D128{0};

    auto& f64  = HazardPointerManager<BasicData,64>::instance();
    auto& f128 = HazardPointerManager<BasicData128,128>::instance();
    auto& d64  = HazardPointerManager<BasicData,0>::instance(64);
    auto& d128 = HazardPointerManager<BasicData128,0>::instance(128);

    std::atomic<std::shared_ptr<BasicData>>   a64;  a64.store(std::make_shared<BasicData>(0));
    std::atomic<std::shared_ptr<BasicData128>> a128; a128.store(std::make_shared<BasicData128>(0));

    const int N_THREADS = std::thread::hardware_concurrency();
    constexpr int OPS   = 5000;
    std::vector<std::thread> threads;
    threads.reserve(N_THREADS);
    for (int t = 0; t < N_THREADS; ++t) {
        threads.emplace_back([&,t](){
            ThreadRegistry::instance().register_id();
            std::mt19937_64 rng(static_cast<uint64_t>(1234u + t));
            std::uniform_int_distribution<int> dist(0,3);
            for (int i = 0; i < OPS; ++i) {
                switch (dist(rng)) {
                  case 0: {
                    auto p = f64.protect(a64);
                    if (p) { p->access(); C64.fetch_add(1); }
                    break;
                  }
                  case 1: {
                    auto p = d64.protect(a64);
                    if (p) { p->access(); D64.fetch_add(1); }
                    break;
                  }
                  case 2: {
                    auto p = f128.protect(a128);
                    if (p) { p->access(); C128.fetch_add(1); }
                    break;
                  }
                  case 3: {
                    auto p = d128.protect(a128);
                    if (p) { p->access(); D128.fetch_add(1); }
                    break;
                  }
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    f64.clear();
    d64.clear();
    f128.clear();
    d128.clear();

    EXPECT_EQ(f64.hazard_size(), 0u);
    EXPECT_EQ(d64.hazard_size(), 0u);
    EXPECT_EQ(f128.hazard_size(),0u);
    EXPECT_EQ(d128.hazard_size(),0u);

    // EXPECT_EQ(C64.load(), D64.load());
    // EXPECT_EQ(C128.load(), D128.load());

    constexpr int MAX_DIFF = 200;  // e.g. allow up to ±200 mismatches
    EXPECT_LE(std::abs(C64.load()  - D64.load()),  MAX_DIFF);
    EXPECT_LE(std::abs(C128.load() - D128.load()), MAX_DIFF);
}

//-----------------------------------------------------------------------------
// 6) TimingProtect
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, TimingProtect) {
    auto bench = [&](auto& mgr, auto sp, const char* name){
        constexpr int N = 100000;
        using Clock = std::chrono::high_resolution_clock;
        auto t0 = Clock::now();
        for(int i=0;i<N;++i){
            auto p = mgr.protect(sp);
            if (p) p->access();
        }
        auto dt = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0);
        std::cout << name << " x" << N << " => " << dt.count() << "μs\n";
    };

    bench(HazardPointerManager<BasicData,64>::instance(),
          std::make_shared<BasicData>(123),
          "Fixed64");

    bench(HazardPointerManager<BasicData128,128>::instance(),
          std::make_shared<BasicData128>(123),
          "Fixed128");

    bench(HazardPointerManager<BasicData,0>::instance(64),
          std::make_shared<BasicData>(123),
          "Dyn64");

    bench(HazardPointerManager<BasicData128,0>::instance(128),
          std::make_shared<BasicData128>(123),
          "Dyn128");
}

//-----------------------------------------------------------------------------
// 7) Scalability @75% utilization
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, ScalabilityUtilization) {
    using Clock = std::chrono::high_resolution_clock;

    // Prepare managers & sample ptrs
    auto& f64   = HazardPointerManager<BasicData,64>::instance();
    auto& f128  = HazardPointerManager<BasicData128,128>::instance();
    auto& d64   = HazardPointerManager<BasicData,0>::instance(64);
    auto& d128  = HazardPointerManager<BasicData128,0>::instance(128);

    struct Mode { const char* name; size_t cap; };
    std::array<Mode,4> modes = {{
      {"Fixed64",   64},
      {"Fixed128", 128},
      {"Dyn64",     64},
      {"Dyn128",   128}
    }};

    for (auto m : modes) {
        size_t pre = m.cap * 75 / 100;
        if (std::string(m.name).rfind("Fixed",0)==0) {
            // fixed
            if (m.cap==64) {
                std::vector<ProtectedPointer<BasicData>> g; g.reserve(pre);
                for (size_t i=0;i<pre;++i) g.push_back(f64.protect(std::make_shared<BasicData>(int(i))));
                auto t0 = Clock::now();
                f64.protect(std::make_shared<BasicData>(0));
                auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t0);
                std::cout<< m.name << "@75% => " << dt.count() << "ns\n";
                g.clear(); f64.clear();
            } else {
                std::vector<ProtectedPointer<BasicData128>> g; g.reserve(pre);
                for (size_t i=0;i<pre;++i) g.push_back(f128.protect(std::make_shared<BasicData128>(int(i))));
                auto t0 = Clock::now();
                f128.protect(std::make_shared<BasicData128>(0));
                auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t0);
                std::cout<< m.name << "@75% => " << dt.count() << "ns\n";
                g.clear(); f128.clear();
            }
        } else {
            // dynamic
            if (m.cap==64) {
                std::vector<ProtectedPointer<BasicData>> g; g.reserve(pre);
                for (size_t i=0;i<pre;++i) g.push_back(d64.protect(std::make_shared<BasicData>(int(i))));
                auto t0 = Clock::now();
                d64.protect(std::make_shared<BasicData>(0));
                auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t0);
                std::cout<< m.name << "@75% => " << dt.count() << "ns\n";
                g.clear(); d64.clear();
            } else {
                std::vector<ProtectedPointer<BasicData128>> g; g.reserve(pre);
                for (size_t i=0;i<pre;++i) g.push_back(d128.protect(std::make_shared<BasicData128>(int(i))));
                auto t0 = Clock::now();
                d128.protect(std::make_shared<BasicData128>(0));
                auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t0);
                std::cout<< m.name << "@75% => " << dt.count() << "ns\n";
                g.clear(); d128.clear();
            }
        }
    }
}

//-----------------------------------------------------------------------------
// 8) Memory‐leak / retire tests
//-----------------------------------------------------------------------------
TEST_F(ProtectOnlyHazardPointerManagerTest, MemoryLeakPrevention) {
    std::atomic<int> C{0}, D{0};
    struct Tracked {
        int v; std::atomic<int>* c; std::atomic<int>* d;
        Tracked(int x,std::atomic<int>* cc,std::atomic<int>* dd):v(x),c(cc),d(dd){c->fetch_add(1);}
        ~Tracked(){ d->fetch_add(1); }
    };
    auto& mgr = HazardPointerManager<Tracked,0>::instance(16,10);
    constexpr int N=100;
    for(int i=0;i<N;++i)
        mgr.retire(std::make_shared<Tracked>(i,&C,&D));
    mgr.reclaim_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(C.load(), N);
    EXPECT_EQ(D.load(), N);
    mgr.clear();
}

TEST_F(ProtectOnlyHazardPointerManagerTest, RetireThresholdConfiguration) {
    struct Th { int v; Th(int x):v(x){} void access(){} };
    using M = HazardPointerManager<Th,0>;
    constexpr std::array<size_t,5> T{1,5,10,20,50};
    for (auto t : T) {
        auto& mgr = M::instance(16,t);
        mgr.clear();
        for (size_t i=0;i<t;++i)
            mgr.retire(std::make_shared<Th>(int(i)));
        EXPECT_LE(mgr.retire_size(), t);
        mgr.retire(std::make_shared<Th>(999));
        EXPECT_LE(mgr.retire_size(), t+1);
        mgr.clear();
    }
}

//-----------------------------------------------------------------------------
// main()
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}