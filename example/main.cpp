#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include "HazardPointerManager.hpp"

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

// A function to periodically update the shared node
void update_shared_node(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
    for (int i = 0; i < 10; ++i) {
        // Create a new node
        auto new_node = std::make_shared<TestNode>(i);
        
        // Swap the new node with the old one
        auto old_node = shared_node.exchange(new_node);
        
        // Retire the old node if it exists
        if (old_node) {
            SYNC_COUT("Retiring node with data " << old_node->data << "\n");
            manager.retire(old_node);
        }
        
        // Periodically reclaim retired nodes
        if (i % 3 == 0) {
            SYNC_COUT("Reclaiming retired nodes in update thread.\n");
            manager.reclaim();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// A function to read from the shared node using hazard pointers
void read_shared_node(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager, int thread_id) {
    for (int i = 0; i < 15; ++i) {
        // Acquire a hazard pointer
        auto hp = manager.acquire();
        if (!hp) {
            SYNC_COUT("Thread " << thread_id << ": Failed to acquire hazard pointer\n");
            continue;
        }
        
        // Try to protect the shared node
        std::shared_ptr<TestNode> protected_node;
        do {
            protected_node = shared_node.load();
            if (!protected_node) break;  // No node to protect
            
            // Set the hazard pointer to protect this node
            hp->pointer.store(protected_node);
            
            // Ensure the node hasn't changed
            if (shared_node.load() != protected_node) {
                // Node changed, retry
                continue;
            }
            
            // Node is now protected by hazard pointer
            break;
        } while (true);
        
        // If we have a protected node, use it
        if (protected_node) {
            SYNC_COUT("Thread " << thread_id << ": Reading node with data " 
                     << protected_node->data << "\n");
            
            // Simulate some work with the node
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        
        // Release the hazard pointer
        manager.release(hp);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

int main() {
    // Create a shared instance of HazardPointerManager
    auto& manager = HazardSystem::HazardPointerManager<TestNode, 10, 5>::instance();

    SYNC_COUT("Starting hazard pointer test.\n");
    
    // Set initial shared node
    shared_node.store(std::make_shared<TestNode>(0));

    // Launch update thread and multiple reader threads
    std::thread updater(update_shared_node, std::ref(manager));
    
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) {
        readers.emplace_back(read_shared_node, std::ref(manager), i);
    }

    // Join all threads
    updater.join();
    for (auto& t : readers) {
        t.join();
    }

    // Final reclamation
    SYNC_COUT("Final reclamation of retired nodes.\n");
    manager.reclaim();
    
    // Clear all remaining nodes
    SYNC_COUT("Clearing all nodes.\n");
    manager.reclaim_all();
    
    // Reset the shared node
    shared_node.store(nullptr);

    SYNC_COUT("Hazard pointer test completed.\n");
    return 0;
}