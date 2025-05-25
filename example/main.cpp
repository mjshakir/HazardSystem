#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include "HazardPointerManager.hpp"

constexpr size_t HAZARD_POINTERS = 9UL, PER_THREAD = 3UL;

// For output synchronization
std::mutex cout_mutex;
#define SYNC_COUT(x) { std::lock_guard<std::mutex> lock(cout_mutex); std::cout << x; }

// Define a simple class to use with HazardPointerManager
struct TestNode {
    int data;
    TestNode(int d) : data(d) {
        SYNC_COUT("TestNode with data " << data << " created.\n");
    }
    ~TestNode() {
        SYNC_COUT("TestNode with data " << data << " deleted.\n");
    }
};

// Shared data structure that we'll protect with hazard pointers
std::atomic<std::shared_ptr<TestNode>> shared_node{nullptr};

// A function to periodically update the shared node - MUCH SIMPLER NOW!
void update_shared_node(HazardSystem::HazardPointerManager<TestNode, HAZARD_POINTERS, PER_THREAD>& manager) {
    for (int i = 0; i < 10; ++i) {
        // Create a new node
        auto new_node = std::make_shared<TestNode>(i);
        
        // Safely get the old node using protection
        auto old_protected = manager.protect(shared_node);
        
        // Swap the new node with the old one
        shared_node.store(new_node);
        
        // Retire the old node if it exists
        if (old_protected) {
            SYNC_COUT("Retiring node with data " << old_protected->data << "\n");
            manager.retire(old_protected.shared_ptr());
        }
        
        // Periodically reclaim retired nodes
        if (i % 3 == 0) {
            SYNC_COUT("Reclaiming retired nodes in update thread.\n");
            manager.reclaim();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// A function to read from the shared node - DRAMATICALLY SIMPLIFIED!
void read_shared_node(HazardSystem::HazardPointerManager<TestNode, HAZARD_POINTERS, PER_THREAD>& manager, int thread_id) {
    for (int i = 0; i < 15; ++i) {
        // ONE LINE! Automatic protection and cleanup
        auto protected_node = manager.protect(shared_node);
        
        // If we have a protected node, use it
        if (protected_node) {
            SYNC_COUT("Thread " << thread_id << ": Reading node with data " 
                     << protected_node->data << "\n");
            
            // Simulate some work with the node
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        
        // No manual release needed - automatic cleanup when protected_node goes out of scope!
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

// Alternative implementation showing retry logic for high contention
void read_shared_node_with_retries(HazardSystem::HazardPointerManager<TestNode, HAZARD_POINTERS, PER_THREAD>& manager, int thread_id) {
    for (int i = 0; i < 15; ++i) {
        // Use the retry version for high-contention scenarios
        auto protected_node = manager.try_protect(shared_node, 50); // Max 50 retries
        
        if (protected_node) {
            SYNC_COUT("Thread " << thread_id << ": Reading node with data " 
                     << protected_node->data << " (with retries)\n");
            
            // Simulate some work with the node
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } else {
            SYNC_COUT("Thread " << thread_id << ": Failed to protect node after retries\n");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

// Example showing different usage patterns
void demonstrate_usage_patterns(HazardSystem::HazardPointerManager<TestNode, HAZARD_POINTERS, PER_THREAD>& manager) {
    SYNC_COUT("=== Demonstrating different usage patterns ===\n");
    
    // Pattern 1: Direct pointer-like usage
    {
        auto protected_ptr = manager.protect(shared_node);
        if (protected_ptr) {
            SYNC_COUT("Pattern 1 - Direct access: " << protected_ptr->data << "\n");
            
            // Can use it like a regular pointer
            TestNode& node_ref = *protected_ptr;
            SYNC_COUT("Pattern 1 - Reference access: " << node_ref.data << "\n");
            
            // Get raw pointer if needed
            TestNode* raw_ptr = protected_ptr.get();
            SYNC_COUT("Pattern 1 - Raw pointer access: " << raw_ptr->data << "\n");
        }
    } // Automatic cleanup here
    
    // Pattern 2: Convert to shared_ptr when needed
    {
        auto protected_ptr = manager.protect(shared_node);
        if (protected_ptr) {
            std::shared_ptr<TestNode> shared = protected_ptr.shared_ptr();
            SYNC_COUT("Pattern 2 - Converted to shared_ptr: " << shared->data << "\n");
            // Both protected_ptr and shared are valid here
            // Protection remains active until protected_ptr destructs
        }
    }
    
    // Pattern 3: Protect a non-atomic shared_ptr
    {
        std::shared_ptr<TestNode> some_node = std::make_shared<TestNode>(999);
        auto protected_ptr = manager.protect(some_node);
        if (protected_ptr) {
            SYNC_COUT("Pattern 3 - Protected non-atomic: " << protected_ptr->data << "\n");
        }
    }
    
    // Pattern 4: Move semantics
    auto get_protected_node = [&]() {
        return manager.protect(shared_node); // Returns by value, efficient move
    };
    
    auto moved_ptr = get_protected_node(); // Moved, no copies
    if (moved_ptr) {
        SYNC_COUT("Pattern 4 - Moved protection: " << moved_ptr->data << "\n");
    }
    
    // Pattern 5: Reset protection manually
    {
        auto protected_ptr = manager.protect(shared_node);
        if (protected_ptr) {
            SYNC_COUT("Pattern 5 - Before reset: " << protected_ptr->data << "\n");
            protected_ptr.reset(); // Manual release
            SYNC_COUT("Pattern 5 - After reset, ptr is now invalid\n");
        }
    }
}

int main() {
    // Create a shared instance of HazardPointerManager
    auto& manager = HazardSystem::HazardPointerManager<TestNode, HAZARD_POINTERS, PER_THREAD>::instance();

    SYNC_COUT("Starting hazard pointer test with ProtectedPointer.\n");
    
    // Set initial shared node
    shared_node.store(std::make_shared<TestNode>(0));

    // Demonstrate usage patterns first
    demonstrate_usage_patterns(manager);

    // Launch update thread and multiple reader threads
    std::thread updater(update_shared_node, std::ref(manager));
    
    std::vector<std::thread> readers;
    readers.reserve(2);
    
    // Mix of regular readers and retry-enabled readers
    for (int i = 0; i < 2; ++i) {
        readers.emplace_back(read_shared_node, std::ref(manager), i);
    }
    
    // One reader with retry logic
    readers.emplace_back(read_shared_node_with_retries, std::ref(manager), 2);

    // Join all threads
    updater.join();
    for (auto& t : readers) {
        t.join();
    }

    // Print statistics
    SYNC_COUT("=== Final Statistics ===\n");
    SYNC_COUT("Active hazard pointers: " << manager.hazards_pointer_size() << "\n");
    SYNC_COUT("Retired nodes: " << manager.retire_size() << "\n");

    // Final reclamation
    SYNC_COUT("Final reclamation of retired nodes.\n");
    manager.reclaim();
    
    // Clear all remaining nodes
    SYNC_COUT("Clearing all nodes.\n");
    manager.reclaim_all();
    
    // Reset the shared node
    shared_node.store(nullptr);

    SYNC_COUT("Hazard pointer test completed.\n");
    
    // Final statistics after cleanup
    SYNC_COUT("=== After Cleanup Statistics ===\n");
    SYNC_COUT("Active hazard pointers: " << manager.hazards_pointer_size() << "\n");
    SYNC_COUT("Retired nodes: " << manager.retire_size() << "\n");
    
    return 0;
}