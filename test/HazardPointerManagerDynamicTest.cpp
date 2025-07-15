// HazardPointerManagerDynamicTest.cpp

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <algorithm>
#include <iostream>

#include "HazardPointerManager.hpp"
#include "ThreadRegistry.hpp"

using namespace HazardSystem;

// Macro to define a unique TestData type per test
#define DEFINE_TESTDATA_TYPE(TESTNAME)                   \
  struct TESTNAME##_TestData {                           \
    int value;                                           \
    std::atomic<bool> destroyed{false};                  \
    std::atomic<int> access_count{0};                    \
    TESTNAME##_TestData(int v = 0) : value(v) {}         \
    ~TESTNAME##_TestData() { destroyed.store(true); }    \
    void access()    { access_count.fetch_add(1); }      \
    void increment() { access_count.fetch_add(1); }      \
  }

// -----------------------------------------------------------------------------
// 1) Singleton
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(SingletonInstance);
TEST(DynamicHazardPointerManager, SingletonInstance) {
  using TestData = SingletonInstance_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& m1 = Manager::instance(4, 4);
  auto& m2 = Manager::instance(4, 4);
  EXPECT_EQ(&m1, &m2);
  m1.clear();
}

// -----------------------------------------------------------------------------
// 2) Dynamic sizing via protect()
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(DynamicSizing);
TEST(DynamicHazardPointerManager, DynamicSizingViaProtect) {
  using TestData = DynamicSizing_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  constexpr size_t HAZ  = 20, RET = 5;
  auto& mgr = Manager::instance(HAZ, RET);
  EXPECT_GE(mgr.hazard_capacity(), HAZ);

  size_t cap = mgr.hazard_capacity();
  std::vector<ProtectedPointer<TestData>> guards;
  guards.reserve(cap);
  for (size_t i = 0; i < cap; ++i) {
    auto p = mgr.protect(std::make_shared<TestData>(int(i)));
    EXPECT_TRUE(static_cast<bool>(p)) << "protect #" << i << " failed";
    guards.push_back(std::move(p));
  }
  // pool exhausted
  auto extra = mgr.protect(std::make_shared<TestData>(999));
  EXPECT_FALSE(static_cast<bool>(extra));

  guards.clear();
  EXPECT_EQ(mgr.hazard_size(), 0u);
  mgr.clear();
}

// -----------------------------------------------------------------------------
// 3) Acquire & Release → protect & reset
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(AcquireAndReleaseViaProtect);
TEST(DynamicHazardPointerManager, AcquireAndReleaseViaProtect) {
  using TestData = AcquireAndReleaseViaProtect_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);

  auto p = mgr.protect(std::make_shared<TestData>(7));
  EXPECT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(mgr.hazard_size(), 1u);

  p.reset();
  EXPECT_EQ(mgr.hazard_size(), 0u);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 4) Protect shared_ptr
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(ProtectSharedPtr);
TEST(DynamicHazardPointerManager, ProtectSharedPtr) {
  using TestData = ProtectSharedPtr_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);
 
  auto data = std::make_shared<TestData>(42);
  auto p = mgr.protect(data);
  EXPECT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->value, 42);
  EXPECT_EQ((*p).value, 42);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 5) Protect atomic shared_ptr
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(ProtectAtomicSharedPtr);
TEST(DynamicHazardPointerManager, ProtectAtomicSharedPtr) {
  using TestData = ProtectAtomicSharedPtr_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);

  std::atomic<std::shared_ptr<TestData>> atom;
  atom.store(std::make_shared<TestData>(100));
  auto p = mgr.protect(atom);
  EXPECT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->value, 100);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 6) try_protect with retries
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(TryProtectWithRetries);
TEST(DynamicHazardPointerManager, TryProtectWithRetries) {
  using TestData = TryProtectWithRetries_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);

  std::atomic<std::shared_ptr<TestData>> atom;
  atom.store(std::make_shared<TestData>(200));
  auto p = mgr.try_protect(atom, 5);
  EXPECT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->value, 200);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 7) try_protect with zero retries
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(TryProtectWithZeroRetries);
TEST(DynamicHazardPointerManager, TryProtectWithZeroRetries) {
  using TestData = TryProtectWithZeroRetries_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);

  std::atomic<std::shared_ptr<TestData>> atom;
  atom.store(std::make_shared<TestData>(300));
  auto p = mgr.try_protect(atom, 0);
  EXPECT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->value, 300);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 8) Retire and reclaim
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(RetireAndReclaim);
TEST(DynamicHazardPointerManager, RetireAndReclaim) {
  using TestData = RetireAndReclaim_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 3);

  auto d1 = std::make_shared<TestData>(1);
  auto d2 = std::make_shared<TestData>(2);
  auto d3 = std::make_shared<TestData>(3);

  EXPECT_TRUE(mgr.retire(d1));
  EXPECT_TRUE(mgr.retire(d2));
  EXPECT_EQ(mgr.retire_size(), 2u);

  // third retire should trigger a reclaim run
  EXPECT_TRUE(mgr.retire(d3));
  mgr.reclaim();
  EXPECT_LE(mgr.retire_size(), 2u);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 9) Protect null pointers
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(ProtectNullptr);
TEST(DynamicHazardPointerManager, ProtectNullptr) {
  using TestData = ProtectNullptr_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);

  std::shared_ptr<TestData> np;
  auto p = mgr.protect(np);
  EXPECT_FALSE(static_cast<bool>(p));

  std::atomic<std::shared_ptr<TestData>> anp;
  anp.store(nullptr);
  auto q = mgr.protect(anp);
  EXPECT_FALSE(static_cast<bool>(q));

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 10) Default ProtectedPointer reset is safe
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(DefaultProtectReset);
TEST(DynamicHazardPointerManager, DefaultProtectedPointerReset) {
  using TestData = DefaultProtectReset_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(10, 10);

  ProtectedPointer<TestData> p;  // default
  p.reset();                    // no crash
  EXPECT_EQ(mgr.hazard_size(), 0u);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 11) Acquire-from-empty-pool via protect
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(AcquireFromEmptyPoolViaProtect);
TEST(DynamicHazardPointerManager, AcquireFromEmptyPoolViaProtect) {
  using TestData = AcquireFromEmptyPoolViaProtect_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(0, 1);  // bit_ceil(0)==1 slot

  auto p1 = mgr.protect(std::make_shared<TestData>(1));
  EXPECT_TRUE(static_cast<bool>(p1));
  EXPECT_EQ(mgr.hazard_size(), 1u);

  auto p2 = mgr.protect(std::make_shared<TestData>(2));
  EXPECT_FALSE(static_cast<bool>(p2));
  EXPECT_EQ(mgr.hazard_size(), 1u);

  p1.reset();
  EXPECT_EQ(mgr.hazard_size(), 0u);

  auto p3 = mgr.protect(std::make_shared<TestData>(3));
  EXPECT_TRUE(static_cast<bool>(p3));

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 12) Acquire-from-single-slot-pool via protect
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(AcquireFromSingleSlotPoolViaProtect);
TEST(DynamicHazardPointerManager, AcquireFromSingleSlotPoolViaProtect) {
  using TestData = AcquireFromSingleSlotPoolViaProtect_TestData;
  using Manager  = HazardPointerManager<TestData, 0>;
  auto& mgr = Manager::instance(1, 1);

  auto p1 = mgr.protect(std::make_shared<TestData>(10));
  EXPECT_TRUE(static_cast<bool>(p1));
  EXPECT_EQ(mgr.hazard_size(), 1u);

  auto p2 = mgr.protect(std::make_shared<TestData>(20));
  EXPECT_FALSE(static_cast<bool>(p2));
  EXPECT_EQ(mgr.hazard_size(), 1u);

  p1.reset();
  EXPECT_EQ(mgr.hazard_size(), 0u);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 13) Pool-size macro via protect
// -----------------------------------------------------------------------------
#define TEST_POOL_SIZE_VIA_PROTECT(PREFIX, SIZE)                           \
  DEFINE_TESTDATA_TYPE(PREFIX##_##SIZE##ViaProtect);                       \
  TEST(DynamicHazardPointerManager, PREFIX##_##SIZE##ViaProtect) {         \
    using TestData = PREFIX##_##SIZE##ViaProtect_TestData;                 \
    using Manager = HazardPointerManager<TestData,0>;                      \
    auto& mgr = Manager::instance(SIZE, SIZE);                             \
    mgr.clear();                                                           \
                                                                           \
    std::vector<ProtectedPointer<TestData>> gs;                            \
    for(size_t i=0;i<SIZE;++i){                                            \
      auto p = mgr.protect(std::make_shared<TestData>(int(i)));            \
      EXPECT_TRUE(static_cast<bool>(p));                                   \
      gs.push_back(std::move(p));                                          \
    }                                                                      \
    auto extra = mgr.protect(std::make_shared<TestData>(-1));              \
    EXPECT_FALSE(static_cast<bool>(extra));                                \
                                                                           \
    gs.clear();                                                            \
    EXPECT_EQ(mgr.hazard_size(),0u);                                       \
    mgr.clear();                                                           \
  }

TEST_POOL_SIZE_VIA_PROTECT(VariablePoolSizes,1)
TEST_POOL_SIZE_VIA_PROTECT(VariablePoolSizes,4)
TEST_POOL_SIZE_VIA_PROTECT(VariablePoolSizes,16)
TEST_POOL_SIZE_VIA_PROTECT(VariablePoolSizes,64)
TEST_POOL_SIZE_VIA_PROTECT(VariablePoolSizes,128)
TEST_POOL_SIZE_VIA_PROTECT(VariablePoolSizes,256)

// -----------------------------------------------------------------------------
// 14) ProtectedPointer move/reset/access/self-assign/double-reset
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(ProtectedPointerMove);
TEST(DynamicHazardPointerManager, ProtectedPointerMove) {
  using TestData = ProtectedPointerMove_TestData;
  using Manager  = HazardPointerManager<TestData,0>;
  auto& mgr = Manager::instance(4,4);

  auto sp = std::make_shared<TestData>(42);
  auto p1 = mgr.protect(sp);
  EXPECT_TRUE(static_cast<bool>(p1));

  auto p2 = std::move(p1);
  EXPECT_FALSE(static_cast<bool>(p1));
  EXPECT_TRUE(static_cast<bool>(p2));
  EXPECT_EQ(p2->value,42);

  auto p3 = mgr.protect(std::make_shared<TestData>(100));
  p3 = std::move(p2);
  EXPECT_FALSE(static_cast<bool>(p2));
  EXPECT_TRUE(static_cast<bool>(p3));
  EXPECT_EQ(p3->value,42);

  mgr.clear();
}

DEFINE_TESTDATA_TYPE(ProtectedPointerReset);
TEST(DynamicHazardPointerManager, ProtectedPointerReset) {
  using TestData = ProtectedPointerReset_TestData;
  using Manager  = HazardPointerManager<TestData,0>;
  auto& mgr = Manager::instance(4,4);

  auto p = mgr.protect(std::make_shared<TestData>(7));
  EXPECT_TRUE(static_cast<bool>(p));
  p.reset();
  EXPECT_FALSE(static_cast<bool>(p));
  EXPECT_EQ(mgr.hazard_size(), 0u);

  mgr.clear();
}

DEFINE_TESTDATA_TYPE(ProtectedPointerAccessors);
TEST(DynamicHazardPointerManager, ProtectedPointerAccessors) {
  using TestData = ProtectedPointerAccessors_TestData;
  using Manager  = HazardPointerManager<TestData,0>;
  auto& mgr = Manager::instance(4,4);

  auto p = mgr.protect(std::make_shared<TestData>(5));
  ASSERT_TRUE(static_cast<bool>(p));
  EXPECT_EQ(p->value,5);
  EXPECT_EQ((*p).value,5);
  EXPECT_EQ(p.get()->value,5);
  EXPECT_EQ(p.shared_ptr()->value,5);

  mgr.clear();
}

DEFINE_TESTDATA_TYPE(ProtectedPointerSelfAssign);
TEST(DynamicHazardPointerManager, ProtectedPointerSelfAssign) {
  using TestData = ProtectedPointerSelfAssign_TestData;
  using Manager  = HazardPointerManager<TestData,0>;
  auto& mgr = Manager::instance(4,4);

  auto p = mgr.protect(std::make_shared<TestData>(9));
  EXPECT_TRUE(static_cast<bool>(p));
  p = std::move(p);
  EXPECT_TRUE(static_cast<bool>(p));

  mgr.clear();
}

DEFINE_TESTDATA_TYPE(ProtectedPointerDoubleReset);
TEST(DynamicHazardPointerManager, ProtectedPointerDoubleReset) {
  using TestData = ProtectedPointerDoubleReset_TestData;
  using Manager  = HazardPointerManager<TestData,0>;
  auto& mgr = Manager::instance(4,4);

  auto p = mgr.protect(std::make_shared<TestData>(11));
  EXPECT_TRUE(static_cast<bool>(p));
  p.reset();
  p.reset();
  EXPECT_FALSE(static_cast<bool>(p));
  EXPECT_EQ(mgr.hazard_size(),0u);

  mgr.clear();
}

// -----------------------------------------------------------------------------
// 15) Clear operation via protect
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(ClearOperationViaProtect);
TEST(DynamicHazardPointerManager, ClearOperationViaProtect) {
  using TestData = ClearOperationViaProtect_TestData;
  using Manager  = HazardPointerManager<TestData,0>;
  auto& mgr = Manager::instance(8,8);

  std::vector<ProtectedPointer<TestData>> guards;
  for (int i = 0; i < 8; ++i) {
    guards.push_back(mgr.protect(std::make_shared<TestData>(i)));
  }
  EXPECT_EQ(mgr.hazard_size(), 8u);

  guards.clear();
  EXPECT_EQ(mgr.hazard_size(), 0u);

  mgr.clear();
  EXPECT_EQ(mgr.hazard_size(), 0u);
}

// -----------------------------------------------------------------------------
// 16) Real-world mixed stress via protect
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(RealWorldDynamicProtect);
TEST(DynamicHazardPointerManager, RealWorldDynamicProtectStressTest) {
  struct Tracked {
    int                  v;
    std::atomic<int>*    c;
    std::atomic<int>*    d;
    Tracked(int vv, std::atomic<int>* cc, std::atomic<int>* dd)
      : v(vv), c(cc), d(dd) { c->fetch_add(1); }
    ~Tracked() { d->fetch_add(1); }
    void access() {}
  };

  std::atomic<int> created{0}, destroyed{0};
  using Mgr = HazardPointerManager<Tracked, 0>;
  auto& mgr = Mgr::instance(64,16);

  std::atomic<std::shared_ptr<Tracked>> shared;
  shared.store(std::make_shared<Tracked>(0,&created,&destroyed),
               std::memory_order_relaxed);

  const int THREADS = std::thread::hardware_concurrency();
  constexpr int OPS = 5000;
  std::mt19937_64 rnd{12345};
  std::vector<std::thread> ths;
  ths.reserve(THREADS);

  for(int t=0;t<THREADS;++t){
    ths.emplace_back([&](){
      ThreadRegistry::instance().register_id();
      std::uniform_int_distribution<int> act(0,4);
      for(int i=0;i<OPS;++i){
        switch(act(rnd)){
          case 0: { // writer
            auto n = std::make_shared<Tracked>(i,&created,&destroyed);
            auto o = shared.exchange(n,std::memory_order_acq_rel);
            if(o) mgr.retire(o);
            break;
          }
          case 1: { // reader
            auto p = mgr.protect(shared);
            if(p) { p->access(); if((i&0x3FF)==0) p.reset(); }
            break;
          }
          case 2: { // bulk retire
            for(int k=0;k<2;++k)
              mgr.retire(std::make_shared<Tracked>(i+k,&created,&destroyed));
            break;
          }
          case 3: mgr.reclaim(); break;
          case 4: { auto p = mgr.protect(
                      std::make_shared<Tracked>(i,&created,&destroyed));
                     if(p) p->access();
                   }
        }
      }
    });
  }
  for(auto &th:ths) th.join();

  // final cleanup
  mgr.clear();
  mgr.reclaim_all();

  EXPECT_EQ(mgr.hazard_size(), 0u);
  EXPECT_EQ(created.load(), destroyed.load()+1);
  EXPECT_EQ(mgr.retire_size(), 0u);
}

// -----------------------------------------------------------------------------
// 17) Memory leak prevention
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(MemoryLeakPrevention);
TEST(DynamicHazardPointerManager, MemoryLeakPrevention) {
  using TestData = MemoryLeakPrevention_TestData;
  struct Tracked2 {
    std::atomic<int>* c; std::atomic<int>* d;
    Tracked2(std::atomic<int>* cc,std::atomic<int>* dd): c(cc),d(dd){c->fetch_add(1);}
    ~Tracked2(){d->fetch_add(1);}
  };

  std::atomic<int> created{0}, destroyed{0};
  auto& mgr = HazardPointerManager<Tracked2,0>::instance(32,8);

  for(int i=0;i<50;i++){
    mgr.retire(std::make_shared<Tracked2>(&created,&destroyed));
  }
  mgr.reclaim_all();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(created, destroyed);
  mgr.clear();
}

// -----------------------------------------------------------------------------
// 18) Retire threshold configuration
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(RetireThresholdConfig);
TEST(DynamicHazardPointerManager, RetireThresholdConfiguration) {
  using TestData = RetireThresholdConfig_TestData;
  constexpr std::array<size_t,3> THR = {1,5,10};
  for(auto t:THR){
    auto& mgr = HazardPointerManager<TestData,0>::instance(16, t);
    mgr.clear();
    for(size_t i=0;i<t;i++){
      mgr.retire(std::make_shared<TestData>(int(i)));
    }
    EXPECT_LE(mgr.retire_size(), t);
    mgr.retire(std::make_shared<TestData>(999));
    EXPECT_LE(mgr.retire_size(), t+1);
    mgr.clear();
  }
}

// -----------------------------------------------------------------------------
// 19) Real World Mixed Stress Concurrency Test
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(RealWorldMixedConcurrency);
TEST(DynamicHazardPointerManager, RealWorldMixedConcurrency) {
    struct Tracked {
        int                v;
        std::atomic<int>*  created;
        std::atomic<int>*  destroyed;
        Tracked(int vv, std::atomic<int>* c, std::atomic<int>* d)
          : v(vv), created(c), destroyed(d) { created->fetch_add(1); }
        ~Tracked() { destroyed->fetch_add(1); }
        void access() {}
    };

    std::atomic<int> created{0}, destroyed{0};
    using Mgr = HazardPointerManager<Tracked, 0>;
    auto& mgr = Mgr::instance(64, 16);

    // shared pointer for readers/writers
    std::atomic<std::shared_ptr<Tracked>> shared;
    shared.store(std::make_shared<Tracked>(0, &created, &destroyed),
                 std::memory_order_relaxed);

    const int THREADS = std::thread::hardware_concurrency();
    constexpr int OPS = 25000;
    std::mt19937_64 rnd{123456};

    std::vector<std::thread> ths;
    ths.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        ths.emplace_back([&, t]() {
            ThreadRegistry::instance().register_id();
            std::uniform_int_distribution<int> action(0,5);
            for (int i = 0; i < OPS; ++i) {
                switch (action(rnd)) {
                  case 0: { // swap writer + retire
                    auto node = std::make_shared<Tracked>(i, &created, &destroyed);
                    auto old  = shared.exchange(node, std::memory_order_acq_rel);
                    if (old) mgr.retire(old);
                    break;
                  }
                  case 1: { // protect + access + occasional reset
                    auto p = mgr.protect(shared);
                    if (p) {
                        p->access();
                        if ((i & 0x7FF)==0) p.reset();
                    }
                    break;
                  }
                  case 2: { // bulk retire
                    for (int k = 0; k < 3; ++k) {
                      mgr.retire(std::make_shared<Tracked>(i+k, &created, &destroyed));
                    }
                    break;
                  }
                  case 3:  // manual reclaim
                    mgr.reclaim();
                    break;
                  case 4: { // fresh protect on new
                    auto p = mgr.protect(std::make_shared<Tracked>(i, &created, &destroyed));
                    if (p) p->access();
                    break;
                  }
                  case 5:  // clear (only when no live ProtectedPointer in this iteration)
                    if ((i % 5000)==0) mgr.clear();
                    break;
                }
            }
        });
    }
    for (auto &th : ths) th.join();

    // final cleanup
    mgr.clear();
    mgr.reclaim_all();

    EXPECT_EQ(mgr.hazard_size(), 0u);
    // there is exactly one node still in 'shared'
    EXPECT_EQ(created.load(), destroyed.load() + 1);
    EXPECT_EQ(mgr.retire_size(), 0u);
}

// -----------------------------------------------------------------------------
// 20) ABA Simulation under try_protect
// -----------------------------------------------------------------------------
DEFINE_TESTDATA_TYPE(ABASimulation);
TEST(DynamicHazardPointerManager, ABASimulation) {
    struct Node {
        int                val;
        std::atomic<int>*  created;
        std::atomic<int>*  destroyed;
        Node(int v, std::atomic<int>* c, std::atomic<int>* d)
          : val(v), created(c), destroyed(d) { created->fetch_add(1); }
        ~Node() { destroyed->fetch_add(1); }
    };

    std::atomic<int> created{0}, destroyed{0};
    using Mgr = HazardPointerManager<Node, 0>;
    auto& mgr = Mgr::instance(32, 8);

    std::atomic<std::shared_ptr<Node>> atom;
    atom.store(std::make_shared<Node>(1, &created, &destroyed),
               std::memory_order_relaxed);

    const int THREADS = std::thread::hardware_concurrency();
    constexpr int OPS = 30000;
    std::mt19937_64 rnd{98765};

    std::vector<std::thread> ths;
    ths.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        ths.emplace_back([&](){
            ThreadRegistry::instance().register_id();
            std::uniform_int_distribution<int> op(0,9);
            for (int i = 0; i < OPS; ++i) {
                if (i % 25 == 0) {
                    // do an ABA: replace A->B->A
                    auto orig = atom.load();
                    auto tmp  = std::make_shared<Node>(999, &created, &destroyed);
                    atom.store(tmp);
                    atom.store(orig);
                    if (orig) mgr.retire(orig);
                    mgr.retire(tmp);
                } else {
                    // try_protect pattern
                    auto p = mgr.try_protect(atom, 5);
                    if (p) { /* read safely */ }
                }
                if (i % 1000 == 0) mgr.reclaim();
            }
        });
    }
    for (auto &th : ths) th.join();

    // final cleanup
    mgr.clear();
    mgr.reclaim_all();

    EXPECT_EQ(mgr.hazard_size(),   0u);
    EXPECT_EQ(mgr.retire_size(),   0u);
    // One left in `atom`
    EXPECT_EQ(created.load(), destroyed.load() + 1);
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main(int argc,char**argv){
  ::testing::InitGoogleTest(&argc,argv);
  std::cout<<"Running Dynamic HazardPointerManager tests...\n";
  return RUN_ALL_TESTS();
}
