#include <iostream>
#include <thread>
#include <vector>
#include "HazardPointerManager.hpp"

// Define a simple class to use with HazardPointerManager
struct TestNode {
    int data;
    TestNode(int d) : data(d) {}
    ~TestNode() {
        std::cout << "TestNode with data " << data << " is deleted.\n";
    }
};

// A simple function to retire nodes using HazardPointerManager
void retire_nodes(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
    for (int i = 0; i < 10; ++i) {
        auto node = std::make_unique<TestNode>(i);  // Use unique_ptr for safer memory management
        manager.retire(std::move(node));  // Retire the node
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// A function to acquire and release hazard pointers
void use_hazard_pointers(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
    for (int i = 0; i < 5; ++i) {
        auto hp = manager.acquire();
        if (hp) {
            auto node = std::make_unique<TestNode>(i * 100);  // Simulate work with the hazard pointer
            hp->pointer.reset(node.release());  // Transfer ownership to hazard pointer
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            manager.release(hp);  // Release the hazard pointer when done
        }
    }
}

// Main test to show behavior
int main() {
    HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager = HazardSystem::HazardPointerManager<TestNode, 10, 5>::instance();

    std::cout << "Starting hazard pointer management example.\n";

    // Launch threads to retire and use hazard pointers concurrently
    std::thread t1(retire_nodes, std::ref(manager));
    std::thread t2(use_hazard_pointers, std::ref(manager));
    std::thread t3(retire_nodes, std::ref(manager));
    std::thread t4(use_hazard_pointers, std::ref(manager));

    // Join the threads to ensure they finish execution
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    std::cout << "Reclaiming retired nodes.\n";
    manager.reclaim();  // Reclaim nodes that are no longer hazards

    std::cout << "Clearing all retired nodes.\n";
    manager.reclaim_all();  // Clear all retired nodes

    std::cout << "Finished hazard pointer management example.\n";
    return 0;
}