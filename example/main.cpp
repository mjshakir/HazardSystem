#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <memory>
#include "HazardPointerManager.hpp"

constexpr size_t HAZARD_POINTERS = 9UL, PER_THREAD = 3UL;

// For output synchronization
std::mutex cout_mutex;
#define SYNC_COUT(x) { std::lock_guard<std::mutex> lock(cout_mutex); std::cout << x; }

struct TestNode {
    int data;
    TestNode(int d) : data(d) { SYNC_COUT("TestNode with data " << data << " created.\n"); }
    ~TestNode() { SYNC_COUT("TestNode with data " << data << " deleted.\n"); }
};

// ----- Thread Functions -----
template <typename Manager>
void update_shared_node(Manager& manager, std::atomic<std::shared_ptr<TestNode>>& shared_node) {
    for (int i = 0; i < 10; ++i) {
        auto new_node = std::make_shared<TestNode>(i);
        auto old_protected = manager.protect(shared_node);
        shared_node.store(new_node);
        if (old_protected) {
            SYNC_COUT("Retiring node with data " << old_protected->data << "\n");
            manager.retire(old_protected.shared_ptr());
        }
        if (i % 3 == 0) {
            SYNC_COUT("Reclaiming retired nodes in update thread.\n");
            manager.reclaim();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

template <typename Manager>
void read_shared_node(Manager& manager, std::atomic<std::shared_ptr<TestNode>>& shared_node, int thread_id) {
    for (int i = 0; i < 15; ++i) {
        auto protected_node = manager.protect(shared_node);
        if (protected_node) {
            SYNC_COUT("Thread " << thread_id << ": Reading node with data "
                     << protected_node->data << "\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

template <typename Manager>
void read_shared_node_with_retries(Manager& manager, std::atomic<std::shared_ptr<TestNode>>& shared_node, int thread_id) {
    for (int i = 0; i < 15; ++i) {
        auto protected_node = manager.try_protect(shared_node, 50);
        if (protected_node) {
            SYNC_COUT("Thread " << thread_id << ": Reading node with data "
                     << protected_node->data << " (with retries)\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } else {
            SYNC_COUT("Thread " << thread_id << ": Failed to protect node after retries\n");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

// Usage pattern function, taking shared_node by ref
template <typename Manager>
void demonstrate_usage_patterns(Manager& manager, std::atomic<std::shared_ptr<TestNode>>& shared_node, const std::string& label) {
    SYNC_COUT("=== [" << label << "] Demonstrating usage patterns ===\n");
    {
        auto protected_ptr = manager.protect(shared_node);
        if (protected_ptr) {
            SYNC_COUT("Pattern 1 - Direct access: " << protected_ptr->data << "\n");
            TestNode& node_ref = *protected_ptr;
            SYNC_COUT("Pattern 1 - Reference access: " << node_ref.data << "\n");
            TestNode* raw_ptr = protected_ptr.get();
            SYNC_COUT("Pattern 1 - Raw pointer access: " << raw_ptr->data << "\n");
        }
    }
    {
        auto protected_ptr = manager.protect(shared_node);
        if (protected_ptr) {
            std::shared_ptr<TestNode> shared = protected_ptr.shared_ptr();
            SYNC_COUT("Pattern 2 - Converted to shared_ptr: " << shared->data << "\n");
        }
    }
    {
        std::shared_ptr<TestNode> some_node = std::make_shared<TestNode>(999);
        auto protected_ptr = manager.protect(some_node);
        if (protected_ptr) {
            SYNC_COUT("Pattern 3 - Protected non-atomic: " << protected_ptr->data << "\n");
        }
    }
    {
        auto get_protected_node = [&]() {
            return manager.protect(shared_node);
        };
        auto moved_ptr = get_protected_node();
        if (moved_ptr) {
            SYNC_COUT("Pattern 4 - Moved protection: " << moved_ptr->data << "\n");
        }
    }
    {
        auto protected_ptr = manager.protect(shared_node);
        if (protected_ptr) {
            SYNC_COUT("Pattern 5 - Before reset: " << protected_ptr->data << "\n");
            protected_ptr.reset();
            SYNC_COUT("Pattern 5 - After reset, ptr is now invalid\n");
        }
    }
}

// ---- Test Harness -----
template <typename Manager>
void run_hazard_pointer_test(Manager& manager, const std::string& label) {
    std::atomic<std::shared_ptr<TestNode>> shared_node;
    shared_node.store(std::make_shared<TestNode>(0));
    demonstrate_usage_patterns(manager, shared_node, label);

    std::thread updater(update_shared_node<Manager>, std::ref(manager), std::ref(shared_node));

    const size_t threads_size = std::thread::hardware_concurrency() - 1;
    std::vector<std::thread> readers;
    readers.reserve(threads_size);

    for (int i = 0; i < threads_size; ++i)
        readers.emplace_back(read_shared_node<Manager>, std::ref(manager), std::ref(shared_node), i);

    updater.join();
    for (auto& t : readers) t.join();

    SYNC_COUT("=== [" << label << "] Final Statistics ===\n");
    SYNC_COUT("Active hazard pointers: " << manager.hazard_size() << "\n");
    SYNC_COUT("Retired nodes: " << manager.retire_size() << "\n");

    SYNC_COUT("Final reclamation of retired nodes.\n");
    manager.reclaim();
    SYNC_COUT("Clearing all nodes.\n");
    manager.reclaim_all();
    shared_node.store(nullptr);

    SYNC_COUT("Hazard pointer test completed.\n");
    SYNC_COUT("=== [" << label << "] After Cleanup Statistics ===\n");
    SYNC_COUT("Active hazard pointers: " << manager.hazard_size() << "\n");
    SYNC_COUT("Retired nodes: " << manager.retire_size() << "\n");
}

// --- Baseline Overhead Test (NO hazard manager logic, just atomics/shared_ptr) ---
void run_baseline_overhead_test(const std::string& label) {
    SYNC_COUT("=== [" << label << "] Running baseline test (NO hazard pointer protection) ===\n");
    std::atomic<std::shared_ptr<TestNode>> shared_node;
    shared_node.store(std::make_shared<TestNode>(0));

    // Updater (no protection)
    auto updater = [&shared_node]() {
        for (int i = 0; i < 10; ++i) {
            auto new_node = std::make_shared<TestNode>(i);
            auto old = shared_node.load();
            shared_node.store(new_node);
            (void)old; // mimic retire, but do nothing
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    // Reader (no protection)
    auto reader = [&shared_node](int thread_id) {
        for (int i = 0; i < 15; ++i) {
            auto current = shared_node.load();
            if (current) {
                SYNC_COUT("Thread " << thread_id << ": (Baseline) Reading node with data "
                         << current->data << "\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    };

    const size_t threads_size = std::thread::hardware_concurrency() - 1;
    std::vector<std::thread> readers;
    readers.reserve(threads_size);

    std::thread updater_thread(updater);
    for (int i = 0; i < threads_size; ++i)
        readers.emplace_back(reader, i);

    updater_thread.join();
    for (auto& t : readers) t.join();

    shared_node.store(nullptr);
    SYNC_COUT("=== [" << label << "] Baseline test completed ===\n");
}

// --- Timing Helper ---
template<typename Func>
double run_timed_test(const std::string& label, Func&& func) {
    using namespace std::chrono;
    SYNC_COUT("------ Timing [" << label << "] ------\n");
    auto start = high_resolution_clock::now();
    func();
    auto end = high_resolution_clock::now();
    double elapsed_ms = duration<double, std::milli>(end - start).count();
    SYNC_COUT("------ [" << label << "] Elapsed: " << elapsed_ms << " ms ------\n\n");
    return elapsed_ms;
}

int main() {
    double fixed_time = 0, dynamic_time = 0, baseline_time = 0;

    // Fixed-size mode
    SYNC_COUT("==============[Fixed]==============\n\n");
    auto& manager_fixed = HazardSystem::HazardPointerManager<TestNode, HAZARD_POINTERS>::instance(PER_THREAD);
    fixed_time = run_timed_test("Fixed-Size", [&](){
        run_hazard_pointer_test(manager_fixed, "Fixed-Size");
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    SYNC_COUT("\n\n");

    // Dynamic-size mode (zero means "dynamic" in your code)
    SYNC_COUT("==============[Dynamic]==============\n\n");
    constexpr size_t DYNAMIC_HAZARD_POINTERS = 0UL;
    constexpr size_t HAZARDS_SIZE = 5UL, RETIRED_SIZE = 4UL;
    auto& manager_dynamic = HazardSystem::HazardPointerManager<TestNode, DYNAMIC_HAZARD_POINTERS>::instance(HAZARDS_SIZE, RETIRED_SIZE);
    dynamic_time = run_timed_test("Dynamic-Size", [&](){
        run_hazard_pointer_test(manager_dynamic, "Dynamic-Size");
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    SYNC_COUT("\n\n");

    // Baseline (NO hazard pointer management)
    SYNC_COUT("==============[Baseline/Overhead]==============\n\n");
    baseline_time = run_timed_test("Baseline (no hazard management)", [&](){
        run_baseline_overhead_test("Baseline");
    });

    // --- Comparison Output ---
    SYNC_COUT("\n\n=====[ Summary ]=====\n");
    SYNC_COUT("Fixed-Size HazardPointerManager:    " << fixed_time    << " ms\n");
    SYNC_COUT("Dynamic-Size HazardPointerManager:  " << dynamic_time  << " ms\n");
    SYNC_COUT("Baseline (NO hazard management):    " << baseline_time << " ms\n");

    auto pct_fixed = ((fixed_time-baseline_time)/baseline_time) * 100.0;
    auto pct_dynamic = ((dynamic_time-baseline_time)/baseline_time) * 100.0;
    SYNC_COUT("\nOverhead vs Baseline:\n");
    SYNC_COUT("  Fixed-Size:   " << pct_fixed << " %\n");
    SYNC_COUT("  Dynamic-Size: " << pct_dynamic << " %\n");
    SYNC_COUT("  (lower % = lower hazard pointer overhead)\n");

    return 0;
}